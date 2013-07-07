
//******************************************************************************
// RCF - Remote Call Framework
//
// Copyright (c) 2005 - 2013, Delta V Software. All rights reserved.
// http://www.deltavsoft.com
//
// RCF is distributed under dual licenses - closed source or GPL.
// Consult your particular license for conditions of use.
//
// If you have not purchased a commercial license, you are using RCF 
// under GPL terms.
//
// Version: 2.0
// Contact: support <at> deltavsoft.com 
//
//******************************************************************************

#ifndef INCLUDE_RCF_FUTURE_HPP
#define INCLUDE_RCF_FUTURE_HPP

#include <RCF/ClientStub.hpp>
#include <RCF/Marshal.hpp>

namespace RCF {

    class I_Future
    {
    public:
        virtual ~I_Future() {}
        virtual void setClientStub(ClientStub *pClientStub) = 0;
    };

    template<typename T>
    class FutureImpl;

    template<typename T>
    class Future
    {
    public:
        Future() : mStatePtr(new State())
        {}

        Future(T *pt) : mStatePtr( new State(pt))
        {}

        Future(T *pt, ClientStub *pClientStub) : mStatePtr( new State(pt))
        {
            pClientStub->enrol(mStatePtr.get());
        }

        Future(const T &t) : mStatePtr( new State(t))
        {}

        operator T&() 
        { 
            return mStatePtr->operator T&();
        }

        T& operator*()
        {
            return mStatePtr->operator T&();
        }

        Future &operator=(const Future &rhs)
        {
            mStatePtr = rhs.mStatePtr;
            return *this;
        }
    
        Future &operator=(const FutureImpl<T> &rhs)
        {
            rhs.assignTo(*this);
            return *this;
        }

        Future(const FutureImpl<T> &rhs) : mStatePtr( new State())
        {
            rhs.assignTo(*this);
        }

        bool ready()
        {
            return mStatePtr->ready();
        }

        void wait(boost::uint32_t timeoutMs = 0)
        {
            mStatePtr->wait(timeoutMs);
        }

        void cancel()
        {
            mStatePtr->cancel();
        }

        void clear()
        {
            mStatePtr->clear();
        }

        ClientStub & getClientStub()
        {
            return mStatePtr->getClientStub();
        }

        std::auto_ptr<Exception> getAsyncException()
        {
            return mStatePtr->getClientStub().getAsyncException();
        }

    private:

        template<typename U>
        friend class FutureImpl;

        class State : public I_Future, boost::noncopyable
        {
        public:
            State() : 
                mpt(), 
                mtPtr( new T() ), 
                mpClientStub()
            {}

            State(T *pt) : 
                mpt(pt), 
                mpClientStub()
            {}

            State(const T &t) : 
                mpt(), 
                mtPtr( new T(t) ), 
                mpClientStub()
            {}

            ~State()
            {
                RCF_DTOR_BEGIN
                unregisterFromCandidates();                            
                RCF_DTOR_END
            }

            operator T&()
            {
                // If a call has been made, check that it has completed, and
                // that there was no exception.

                if (mpClientStub)
                {
                    if (!mpClientStub->ready())
                    {
                        mpClientStub->waitForReady();
                    }

                    std::auto_ptr<Exception> ePtr = 
                        mpClientStub->getAsyncException();

                    if (ePtr.get())
                    {
                        ePtr->throwSelf();
                    }
                }

                T *pt = mpt ? mpt : mtPtr.get();
                {
                    Lock lock(gCandidatesMutex());
                    gCandidates().add(pt, this);
                }
                
                return *pt;
            }

            void setClientStub(ClientStub *pClientStub)
            {
                mpClientStub = pClientStub;
            }

            void setClientStub(ClientStub *pClientStub, T * pt)
            {
                unregisterFromCandidates();

                mpClientStub = pClientStub;
                mpt = pt;
                mtPtr.reset();
            }

        private:

            T *                     mpt;
            boost::scoped_ptr<T>    mtPtr;
            RCF::ClientStub *       mpClientStub;

        public:

            bool ready()
            {
                return mpClientStub->ready();
            }

            void wait(boost::uint32_t timeoutMs = 0)
            {
                mpClientStub->waitForReady(timeoutMs);
            }

            void cancel()
            {
                mpClientStub->cancel();                
            }

            ClientStub & getClientStub()
            {
                return *mpClientStub;
            }

            void unregisterFromCandidates()
            {
                T *pt = mpt ? mpt : mtPtr.get();
                Lock lock(gCandidatesMutex());
                I_Future * pFuture = gCandidates().find(pt);
                if (pFuture)
                {
                    gCandidates().erase(pt);
                }
            }

        };

        boost::shared_ptr<State> mStatePtr;
    };

    class LogEntryExit
    {
    public:
        LogEntryExit(ClientStub & clientStub) :
            mClientStub(clientStub),
            mMsg(clientStub.mCurrentCallDesc)
        {
            if (mClientStub.mCallInProgress)
            {
                RCF_THROW(_RcfError_ConcurrentCalls());
            }

            mClientStub.mCallInProgress = true;
            RCF_LOG_2() << "RcfClient - begin remote call. " << mMsg;
        }

        ~LogEntryExit()
        {
            if (!mClientStub.getAsync())
            {
                RCF_LOG_2() << "RcfClient - end remote call. " << mMsg;
                mClientStub.mCallInProgress = false;
            }
        }

    private:
        ClientStub & mClientStub;
        const std::string & mMsg;
    };

    template<typename T>
    class FutureImpl
    {
    public:
        FutureImpl(
            T &t, 
            ClientStub &clientStub, 
            const std::string & interfaceName,
            int fnId,
            RemoteCallSemantics rcs,
            const char * szFunc = "",
            const char * szArity = "") :
                mpT(&t),
                mpClientStub(&clientStub),
                mFnId(fnId),
                mRcs(rcs),
                mSzFunc(szFunc),
                mSzArity(szArity),
                mOwn(true)
        {
            // TODO: put this in the initializer list instead?
            clientStub.init(interfaceName, fnId, rcs);
        }

        FutureImpl(const FutureImpl &rhs) :
            mpT(rhs.mpT),
            mpClientStub(rhs.mpClientStub),
            mFnId(rhs.mFnId),
            mRcs(rhs.mRcs),
            mSzFunc(rhs.mSzFunc),
            mSzArity(rhs.mSzArity),
            mOwn(rhs.mOwn)
        {
            rhs.mOwn = false;
        }

        FutureImpl &operator=(const FutureImpl &rhs)
        {
            mpT = rhs.mpT;
            mpClientStub = rhs.mpClientStub;
            mFnId = rhs.mFnId;
            mRcs = rhs.mRcs;
            mSzFunc = rhs.mSzFunc;
            mSzArity = rhs.mSzArity;

            mOwn = rhs.mOwn;
            rhs.mOwn = false;
            return *this;
        }

        T get()
        {
            return operator T();
        }

        // Conversion to T kicks off a sync call.
        operator T() const
        {
            mOwn = false;
            call();
            T t = *mpT;
            mpClientStub->clearParameters();
            return t;
        }

        // Assignment to Future<> kicks off an async call.
        void assignTo(Future<T> &future) const
        {
            mOwn = false;
            mpClientStub->setAsync(true);
            future.mStatePtr->setClientStub(mpClientStub, mpT);
            call();
        }

        // Void or ignored return value, kicks off a sync call.
        ~FutureImpl()
        {
            if(mOwn)
            {
                call();

                if (!mpClientStub->getAsync())
                {
                    mpClientStub->clearParameters();
                }
            }
        }

    private:

        void call() const
        {
            if (mpClientStub->getTransport().isInProcess())
            {
                mpClientStub->getTransport().doInProcessCall(*mpClientStub);
                if (mpClientStub->mAsyncCallback)
                {
                    mpClientStub->mAsyncCallback = boost::function0<void>();
                }
                return;
            }

#if RCF_FEATURE_FILETRANSFER==1

            // File uploads are done before the call itself.
            mpClientStub->processUploadStreams();

#endif

            // TODO
            bool async = mpClientStub->getAsync();

            mpClientStub->setTries(0);

            setCurrentCallDesc(mpClientStub->mCurrentCallDesc, mpClientStub->mRequest, mSzFunc, mSzArity);

            if (async)
            {
                callAsync();
            }
            else
            {
                callSync();
            }
        }

        void callSync() const
        {
            // ClientStub::onConnectCompleted() uses the contents of mEncodedByteBuffers
            // to determine what stage the current call is in. So mEncodedByteBuffers
            // needs to be cleared after a remote call, even if an exception is thrown.

            // Error handling code here will generally also need to be present in 
            // ClientStub::onError().

            LogEntryExit logEntryExit(*mpClientStub);

            RCF_LOG_3()(mpClientStub)(mpClientStub->mRequest) 
                << "RcfClient - sending synchronous request.";

            try
            {
                mpClientStub->call(mRcs);
            }
            catch(const RCF::RemoteException & e)
            {
                mpClientStub->mEncodedByteBuffers.resize(0);
                if (shouldDisconnectOnRemoteError( e.getError() ))
                {
                    mpClientStub->disconnect();
                }
                throw; 
            }
            catch(const RCF::Exception &)
            {
                mpClientStub->mEncodedByteBuffers.resize(0);
                mpClientStub->disconnect();
                throw;
            }
            catch(...)
            {
                mpClientStub->mEncodedByteBuffers.resize(0);
                mpClientStub->disconnect();
                throw;
            }
        }

        void callAsync() const
        {
            LogEntryExit logEntryExit(*mpClientStub);

            RCF_LOG_3()(mpClientStub)(mpClientStub->mRequest) 
                << "RcfClient - sending asynchronous request.";

            std::auto_ptr<RCF::Exception> ape;

            try
            {
                mpClientStub->call(mRcs);
            }
            catch(const RCF::Exception & e)
            {
                ape.reset( e.clone().release() );
            }
            catch(...)
            {
                ape.reset( new Exception(_RcfError_NonStdException()) );
            }

            if (ape.get())
            {
                mpClientStub->onError(*ape);
            }

            getTlsAmiNotification().run();
        }

        T *                     mpT;
        ClientStub *            mpClientStub;
        int                     mFnId;
        RemoteCallSemantics     mRcs;
        const char *            mSzFunc;
        const char *            mSzArity;

        mutable bool            mOwn;
    };

    template<typename T, typename U>
    bool operator==(const FutureImpl<T> & fi, const U & u)
    {
        return fi.operator T() == u;
    }

    template<typename T, typename U>
    bool operator==(const U & u, const FutureImpl<T> & fi)
    {
        return u == fi.operator T();
    }

    template<typename T>
    std::ostream & operator<<(std::ostream & os, const FutureImpl<T> & fi)
    {
        return os << fi.operator T();
    }


}

#endif // INCLUDE_RCF_FUTURE_HPP

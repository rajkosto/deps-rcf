
//******************************************************************************
// RCF - Remote Call Framework
//
// Copyright (c) 2005 - 2012, Delta V Software. All rights reserved.
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

#include <RCF/RcfServer.hpp>

#include <algorithm>

#include <boost/bind.hpp>

#include <RCF/Filter.hpp>
#include <RCF/CurrentSession.hpp>
#include <RCF/Endpoint.hpp>
#include <RCF/FilterService.hpp>
#include <RCF/IpServerTransport.hpp>
#include <RCF/Marshal.hpp>
#include <RCF/MethodInvocation.hpp>
#include <RCF/ObjectFactoryService.hpp>
#include <RCF/PingBackService.hpp>
#include <RCF/PublishingService.hpp>
#include <RCF/RcfClient.hpp>
#include <RCF/RcfSession.hpp>
#include <RCF/ServerTask.hpp>
#include <RCF/SessionObjectFactoryService.hpp>
#include <RCF/SessionTimeoutService.hpp>
#include <RCF/Service.hpp>
#include <RCF/StubEntry.hpp>
#include <RCF/SubscriptionService.hpp>
#include <RCF/TcpClientTransport.hpp>
#include <RCF/ThreadLocalData.hpp>
#include <RCF/Token.hpp>
#include <RCF/Tools.hpp>

#include <RCF/Version.hpp>

#ifdef RCF_USE_SF_SERIALIZATION
#include <SF/memory.hpp>
#endif

#ifdef RCF_USE_BOOST_SERIALIZATION
#include <RCF/BsAutoPtr.hpp>
#endif

#ifdef RCF_USE_BOOST_FILESYSTEM
#include <RCF/FileTransferService.hpp>
#else
namespace RCF { class FileTransferService {}; }
#endif

#ifdef RCF_USE_ZLIB
#include <RCF/ZlibCompressionFilter.hpp>
#endif

#ifdef RCF_USE_OPENSSL
#include <RCF/OpenSslEncryptionFilter.hpp>
#endif

#ifdef RCF_USE_JSON
#include <RCF/JsonRpc.hpp>
#endif

#ifdef BOOST_WINDOWS
#include <RCF/Schannel.hpp>
#include <RCF/SspiFilter.hpp>
#include <RCF/Win32NamedPipeServerTransport.hpp>
#include <RCF/Win32Username.hpp>
#endif

namespace RCF {

    // RcfServer

    RcfServer::RcfServer() :
        mStubMapMutex(WriterPriority),
        mStarted(),
        mThreadPoolPtr( new ThreadPool(1, "RCF Server") ),
        mRuntimeVersion(RCF::getDefaultRuntimeVersion()),
        mArchiveVersion(RCF::getDefaultArchiveVersion()),
        mPropertiesMutex(WriterPriority)
    {
        init();
    }

    RcfServer::RcfServer(const I_Endpoint &endpoint) :
        mStubMapMutex(WriterPriority),
        mStarted(),
        mThreadPoolPtr( new ThreadPool(1, "RCF Server") ),
        mRuntimeVersion(RCF::getDefaultRuntimeVersion()),
        mArchiveVersion(RCF::getDefaultArchiveVersion()),
        mPropertiesMutex(WriterPriority)
    {
        addEndpoint(endpoint);
        init();
    }

    RcfServer::RcfServer(ServicePtr servicePtr) :
        mStubMapMutex(WriterPriority),
        mStarted(),
        mThreadPoolPtr( new ThreadPool(1, "RCF Server") ),
        mRuntimeVersion(RCF::getDefaultRuntimeVersion()),
        mArchiveVersion(RCF::getDefaultArchiveVersion()),
        mPropertiesMutex(WriterPriority)
    {
        addService(servicePtr);
        init();
    }

    RcfServer::RcfServer(ServerTransportPtr serverTransportPtr) :
        mStubMapMutex(WriterPriority),
        mStarted(),
        mThreadPoolPtr( new ThreadPool(1, "RCF Server") ),
        mRuntimeVersion(RCF::getDefaultRuntimeVersion()),
        mArchiveVersion(RCF::getDefaultArchiveVersion()),
        mPropertiesMutex(WriterPriority)
    {
        addService( boost::dynamic_pointer_cast<I_Service>(serverTransportPtr) );
        init();
    }

    RcfServer::~RcfServer()
    {
        RCF_DTOR_BEGIN
            stop();
        RCF_DTOR_END
    }

    void RcfServer::init()
    {
        mSessionTimeoutMs = 0;
        mSessionReapingIntervalMs = 30*1000;

        mOfsMaxNumberOfObjects = 0;
        mOfsObjectTimeoutS = 0;
        mOfsCleanupIntervalS = 0;
        mOfsCleanupThreshold = 0.0;

        mPreferSchannel = false;

#ifdef RCF_USE_BOOST_FILESYSTEM
        mFileUploadQuota = 0;
        mFileDownloadQuota = 0;
#endif
        mFilterServicePtr.reset( new FilterService() );

#if defined(BOOST_WINDOWS)
        mFilterServicePtr->addFilterFactory( FilterFactoryPtr( new NtlmFilterFactory() ) );
        mFilterServicePtr->addFilterFactory( FilterFactoryPtr( new KerberosFilterFactory() ) );
        mFilterServicePtr->addFilterFactory( FilterFactoryPtr( new NegotiateFilterFactory() ) );
        mFilterServicePtr->addFilterFactory( FilterFactoryPtr( new SchannelFilterFactory() ) );
#endif

#ifdef RCF_USE_ZLIB
        mFilterServicePtr->addFilterFactory( FilterFactoryPtr( new ZlibCompressionFilterFactory() ) );
        mFilterServicePtr->addFilterFactory( FilterFactoryPtr( new ZlibStatelessCompressionFilterFactory() ) );
#endif

#ifdef RCF_USE_OPENSSL
        mFilterServicePtr->addFilterFactory( FilterFactoryPtr( new OpenSslEncryptionFilterFactory() ));        
#endif

        mFilterServicePtr->addFilterFactory( FilterFactoryPtr( new IdentityFilterFactory() ) );
        mFilterServicePtr->addFilterFactory( FilterFactoryPtr( new XorFilterFactory() ) );


        mPingBackServicePtr.reset( new PingBackService() );
        mSessionTimeoutServicePtr.reset( new SessionTimeoutService() );
        mPublishingServicePtr.reset( new PublishingService() );
        mSubscriptionServicePtr.reset( new SubscriptionService() );
        mCallbackConnectionServicePtr.reset( new CallbackConnectionService() );
        mSessionObjectFactoryServicePtr.reset( new SessionObjectFactoryService() );
        mObjectFactoryServicePtr.reset( new ObjectFactoryService() );

        addService(mFilterServicePtr);
        addService(mPingBackServicePtr);
        addService(mSessionTimeoutServicePtr);
        addService(mPublishingServicePtr);
        addService(mSubscriptionServicePtr);
        addService(mCallbackConnectionServicePtr);
        addService(mSessionObjectFactoryServicePtr);
        addService(mObjectFactoryServicePtr);

#ifdef RCF_USE_BOOST_FILESYSTEM
        mFileTransferServicePtr.reset( new FileTransferService() );
        addService(mFileTransferServicePtr);
#endif

    }

    bool RcfServer::addService(ServicePtr servicePtr)
    {
        RCF_LOG_2()(typeid(*servicePtr).name()) << "RcfServer - adding service.";

        RCF_ASSERT(!mStarted && "Services cannot be added or removed while server is running.");

        if (
            std::find(
                mServices.begin(),
                mServices.end(),
                servicePtr) == mServices.end())
        {
            mServices.push_back(servicePtr);

            ServerTransportPtr serverTransportPtr =
                boost::dynamic_pointer_cast<I_ServerTransport>(servicePtr);

            if (serverTransportPtr)
            {
                mServerTransports.push_back(serverTransportPtr);
            }

            servicePtr->onServiceAdded(*this);

            return true;
        }
        return false;
    }

    bool RcfServer::removeService(ServicePtr servicePtr)
    {
        RCF_LOG_2()(typeid(*servicePtr).name()) << "Removing service.";

        RCF_ASSERT(!mStarted && "Services cannot be added or removed while server is running.");

        std::vector<ServicePtr>::iterator iter =
            std::find(mServices.begin(), mServices.end(), servicePtr);

        if (iter != mServices.end())
        {
            mServices.erase(iter);

            ServerTransportPtr serverTransportPtr =
                boost::dynamic_pointer_cast<I_ServerTransport>(servicePtr);

            if (serverTransportPtr)
            {
                eraseRemove(mServerTransports, serverTransportPtr);
            }

            stopService(servicePtr);
            servicePtr->onServiceRemoved(*this);
            return true;
        }
        return false;
    }

    bool RcfServer::addServerTransport(ServerTransportPtr serverTransportPtr)
    {
        return addService(
            boost::dynamic_pointer_cast<I_Service>(serverTransportPtr));
    }

    bool RcfServer::removeServerTransport(ServerTransportPtr serverTransportPtr)
    {
        return removeService(
            boost::dynamic_pointer_cast<I_Service>(serverTransportPtr));
    }

    void RcfServer::start()
    {
        startImpl();
    }

    void RcfServer::resolveServiceThreadPools(ServicePtr servicePtr) const
    {
        I_Service & service = *servicePtr;
        TaskEntries & taskEntries = service.mTaskEntries;
        for (std::size_t j=0; j<taskEntries.size(); ++j)
        {
            TaskEntry & taskEntry = taskEntries[j];

            // Muxer type == 0 , means we have to have a local thread manager.
            if (taskEntry.mMuxerType == Mt_None && !taskEntry.mLocalThreadPoolPtr)
            {
                taskEntry.mLocalThreadPoolPtr.reset( new ThreadPool(1, "") );
            }

            if (taskEntry.mLocalThreadPoolPtr)
            {
                taskEntry.mLocalThreadPoolPtr->setTask(taskEntry.mTask);
                taskEntry.mLocalThreadPoolPtr->setStopFunctor(taskEntry.mStopFunctor);
                taskEntry.mLocalThreadPoolPtr->setThreadName(taskEntry.mThreadName);
                taskEntry.mWhichThreadPoolPtr = taskEntry.mLocalThreadPoolPtr;
            }
            else if (service.mThreadPoolPtr)
            {
                taskEntry.mWhichThreadPoolPtr = service.mThreadPoolPtr;
            }
            else
            {
                taskEntry.mWhichThreadPoolPtr = mThreadPoolPtr;
            }

            taskEntry.mWhichThreadPoolPtr->enableMuxerType(taskEntry.mMuxerType);
        }
    }

    void RcfServer::startImpl()
    {
        Lock lock(mStartStopMutex);
        if (!mStarted)
        {
            // open the server

            for (std::size_t i=0; i<mServices.size(); ++i)
            {
                resolveServiceThreadPools(mServices[i]);
            }

            // notify all services
            for (std::size_t i=0; i<mServices.size(); ++i)
            {
                mServices[i]->onServerStart(*this);
            }

            // spawn internal worker threads
            for (std::size_t i=0; i<mServices.size(); ++i)
            {
                startService(mServices[i]);
            }

            mStarted = true;

            // call the start notification callback, if there is one
            invokeStartCallback();

            // notify anyone who was waiting on the stop event
            mStartEvent.notify_all(lock);
        }
    }

    void RcfServer::startService(ServicePtr servicePtr) const
    {
        RCF_LOG_2()(typeid(*servicePtr).name()) << "RcfServer - starting service.";

        TaskEntries &taskEntries = servicePtr->mTaskEntries;

        for (std::size_t i=0; i<taskEntries.size(); ++i)
        {
            if (taskEntries[i].getAutoStart())
            {
                taskEntries[i].start();
            }
        }
    }

    void RcfServer::stopService(ServicePtr servicePtr) const
    {
        RCF_LOG_2()(typeid(*servicePtr).name()) << "RcfServer - stopping service.";

        TaskEntries &taskEntries = servicePtr->mTaskEntries;

        for (std::size_t i=taskEntries.size()-1; i != std::size_t(-1); --i)
        {
            taskEntries[i].stop();
        }
    }

    void RcfServer::stop()
    {
        Lock lock(mStartStopMutex);
        if (mStarted)
        {
            mStarted = false;

            for (std::size_t i=mServices.size()-1; i != std::size_t(-1); --i)
            {
                stopService(mServices[i]);
            }

            // notify all services
            for (std::size_t i=mServices.size()-1; i != std::size_t(-1); --i)
            {
                mServices[i]->onServerStop(*this);
            }

            // Reset all muxers.
            if (mThreadPoolPtr)
            {
                mThreadPoolPtr->resetMuxers();
            }

            for (std::size_t i=mServices.size()-1; i != std::size_t(-1); --i)
            {
                mServices[i]->resetMuxers();
            }

            // notify anyone who was waiting on the stop event
            mStopEvent.notify_all(lock);
        }
    }

    void RcfServer::waitForStopEvent()
    {
        Lock lock(mStartStopMutex);
        if (mStarted)
        {
            mStopEvent.wait(lock);
        }
    }

    void RcfServer::waitForStartEvent()
    {
        Lock lock(mStartStopMutex);
        if (!mStarted)
        {
            mStartEvent.wait(lock);
        }
    }   

    bool RcfServer::isStarted()
    {
        return mStarted;
    }

    SessionPtr RcfServer::createSession()
    {
        RcfSessionPtr rcfSessionPtr(new RcfSession(*this));
        rcfSessionPtr->setWeakThisPtr();
        registerSession(rcfSessionPtr);
        return rcfSessionPtr;
    }

    void RcfServer::registerSession(
        RcfSessionPtr rcfSessionPtr)
    {
        Lock lock(mSessionsMutex);
        mSessions.insert( RcfSessionWeakPtr(rcfSessionPtr));
    }

    void RcfServer::unregisterSession(
        RcfSessionWeakPtr rcfSessionWeakPtr)
    {
        Lock lock(mSessionsMutex);

        std::set<RcfSessionWeakPtr>::iterator iter =
            mSessions.find(rcfSessionWeakPtr);

        RCF_ASSERT(iter != mSessions.end());

        mSessions.erase(iter);
    }

    void RcfServer::onReadCompleted(SessionPtr sessionPtr)
    {
        RcfSessionPtr rcfSessionPtr = sessionPtr;

        // Need a recursion limiter here. When processing many sequential oneway
        // calls, over a caching transport filter such as the zlib filter, we 
        // would otherwise be at risk of encountering unlimited recursion and 
        // eventual stack overflow.

        RecursionState<int, int> & recursionState = 
            getTlsRcfSessionRecursionState();

        applyRecursionLimiter(
            recursionState, 
            &RcfSession::onReadCompleted, 
            *rcfSessionPtr);

        //rcfSessionPtr->onReadCompleted();
    }

    void RcfSession::onReadCompleted()
    {
        // 1. Deserialize request data
        // 2. Store request data in session
        // 3. Move session to corresponding queue

        Lock lock(mStopCallInProgressMutex);
        if (!mStopCallInProgress)
        {
            I_ServerTransport & transport = mpSessionState->getServerTransport();

            // If this is the first call on a Win32 pipe transport, impersonate the client
            // and get their username.

#if defined(BOOST_WINDOWS)
            if (    getTransportType() == Tt_Win32NamedPipe 
                &&  mClientUsername.empty() 
                &&  !getIsCallbackSession())
            {
                Win32NamedPipeImpersonator impersonator;
                tstring domainAndUsername = RCF::getMyDomain() + RCF_T("\\") + RCF::getMyUserName();
                getCurrentRcfSession().mClientUsername = domainAndUsername;
            }
#endif

            RpcProtocol rpcProtocol = transport.getRpcProtocol();
            if (rpcProtocol == Rp_JsonRpc)
            {
                processJsonRpcRequest();
                return;
            }

            ByteBuffer readByteBuffer = getSessionState().getReadByteBuffer();

            RCF_LOG_3()(this)(readByteBuffer.getLength()) 
                << "RcfServer - received packet from transport.";

            ByteBuffer messageBody;

            bool ok = mRequest.decodeRequest(
                readByteBuffer,
                messageBody,
                shared_from_this(),
                mRcfServer);

            RCF_LOG_3()(this)(mRequest) 
                << "RcfServer - received request.";

            // Setup the in stream for this remote call.
            mIn.reset(
                messageBody, 
                mRequest.mSerializationProtocol, 
                mRuntimeVersion, 
                mArchiveVersion,
                mRequest.mEnableSfPointerTracking);

            messageBody.clear();
            
            readByteBuffer.clear();

            if (!ok)
            {
                // version mismatch (client is newer than we are)
                // send a control message back to client, with our runtime version

                if (mRequest.mOneway)
                {
                    mIn.clearByteBuffer();
                    onWriteCompleted();
                }
                else
                {
                    std::vector<ByteBuffer> byteBuffers(1);

                    encodeServerError(
                        mRcfServer,
                        byteBuffers.front(),
                        RcfError_VersionMismatch, 
                        mRcfServer.getRuntimeVersion(),
                        mRcfServer.getArchiveVersion());

                    getSessionState().postWrite(byteBuffers);
                }
            }
            else
            {
                if (mRequest.getClose()) 
                {
                    getSessionState().postClose();
                }
                else
                {
                    processRequest();
                }
            }
        }
    }

    void RcfServer::onWriteCompleted(SessionPtr sessionPtr)
    {
        RcfSessionPtr rcfSessionPtr = sessionPtr;
        rcfSessionPtr->onWriteCompleted();
    }

    void RcfSession::onWriteCompleted()
    {
        RCF_LOG_3()(this) << "RcfServer - completed sending of response.";

        {
            Lock lock(mIoStateMutex);

            if (mWritingPingBack)
            {
                mWritingPingBack = false;

                typedef std::vector<ByteBuffer> ByteBuffers;
                ThreadLocalCached< ByteBuffers > tlcQueuedBuffers;
                ByteBuffers & queuedBuffers = tlcQueuedBuffers.get();

                queuedBuffers = mQueuedSendBuffers;
                mQueuedSendBuffers.clear();
                if (!queuedBuffers.empty())
                {
                    lock.unlock();
                    getSessionState().postWrite(queuedBuffers);
                    RCF_ASSERT(queuedBuffers.empty());
                }

                return;
            }
        }

        typedef std::vector<RcfSession::OnWriteCompletedCallback> OnWriteCompletedCallbacks;
        ThreadLocalCached< OnWriteCompletedCallbacks > tlcOwcc;
        OnWriteCompletedCallbacks &onWriteCompletedCallbacks = tlcOwcc.get();
        
        extractOnWriteCompletedCallbacks(onWriteCompletedCallbacks);

        for (std::size_t i=0; i<onWriteCompletedCallbacks.size(); ++i)
        {
            onWriteCompletedCallbacks[i](*this);
        }

        onWriteCompletedCallbacks.resize(0);

        mIn.clear();
        mOut.clear();

        if (!mCloseSessionAfterWrite)
        {
            getSessionState().postRead();
        }        
    }

    void RcfSession::sendSessionResponse()
    {
        mIn.clearByteBuffer();

        ThreadLocalCached< std::vector<ByteBuffer> > tlcByteBuffers;
        std::vector<ByteBuffer> &byteBuffers = tlcByteBuffers.get();

        mOut.extractByteBuffers(byteBuffers);
        const std::vector<FilterPtr> &filters = mFilters;
        ThreadLocalCached< std::vector<ByteBuffer> > tlcEncodedByteBuffers;
        std::vector<ByteBuffer> &encodedByteBuffers = tlcEncodedByteBuffers.get();

        ThreadLocalCached< std::vector<FilterPtr> > tlcNoFilters;
        std::vector<FilterPtr> &noFilters = tlcNoFilters.get();

        mRequest.encodeToMessage(
            encodedByteBuffers, 
            byteBuffers, 
            mFiltered ? filters : noFilters);

        RCF_LOG_3()(this)(lengthByteBuffers(byteBuffers))(lengthByteBuffers(encodedByteBuffers))
            << "RcfServer - sending response.";

        byteBuffers.resize(0);

        bool okToWrite = false;
        {
            Lock lock(mIoStateMutex);
            unregisterForPingBacks();
            if (mWritingPingBack)
            {
                mQueuedSendBuffers = encodedByteBuffers;
                encodedByteBuffers.resize(0);
                byteBuffers.resize(0);
            }
            else
            {
                okToWrite = true;
            }
        }

        if (okToWrite)
        {
            getSessionState().postWrite(encodedByteBuffers);
            RCF_ASSERT(encodedByteBuffers.empty());
            RCF_ASSERT(byteBuffers.empty());
        }

        setTlsRcfSessionPtr();
    }

    void RcfSession::sendResponseUncaughtException()
    {
        RCF_LOG_3() << "RcfServer - non-std::exception-derived exception was thrown. Sending an error response.";
        sendResponseException( RemoteException(_RcfError_NonStdException()));
    }

    void RcfSession::encodeRemoteException(
        SerializationProtocolOut & out, 
        const RemoteException & e)
    {
        ByteBuffer buffer;
        bool shouldSerializeException = mRequest.encodeResponse(&e, buffer, false);

        mOut.reset(
            mRequest.mSerializationProtocol, 
            32, 
            buffer, 
            mRuntimeVersion, 
            mArchiveVersion,
            mEnableSfPointerTracking);

        if (shouldSerializeException)
        {
            if (
                out.getSerializationProtocol() == Sp_BsBinary 
                || out.getSerializationProtocol() == Sp_BsText 
                || out.getSerializationProtocol() == Sp_BsXml)
            {
                int runtimeVersion = mRequest.mRuntimeVersion;
                if (runtimeVersion < 8)
                {
                    // Boost serialization is very picky about serializing pointers 
                    // vs values. Since the client will deserialize an auto_ptr, we 
                    // are forced to create an auto_ptr here as well.

                    std::auto_ptr<RemoteException> apRe( 
                        static_cast<RemoteException *>(e.clone().release()) );

                    serialize(out, apRe);
                }
                else
                {
                    const RCF::RemoteException * pRe = &e;
                    serialize(out, pRe);
                }
            }
            else
            {
                // SF is a bit more flexible.
                serialize(out, e);
            }
        }
    }

    void RcfSession::sendResponse()
    {
        bool exceptionalResponse = false;
        try
        {
            ByteBuffer buffer;
            mRequest.encodeResponse(NULL, buffer, mEnableSfPointerTracking);

            mOut.reset(
                mRequest.mSerializationProtocol, 
                32, 
                buffer, 
                mRuntimeVersion, 
                mArchiveVersion,
                mEnableSfPointerTracking);

            mpParameters->write(mOut);
            clearParameters();
        }
        catch(const std::exception &e)
        {
            sendResponseException(e);
            exceptionalResponse = true;
        }

        if (!exceptionalResponse)
        {
            sendSessionResponse();
            RCF_LOG_2() << "RcfServer - end remote call. " << mCurrentCallDesc;
        }
    }

    void RcfSession::sendResponseException(
        const std::exception &e)
    {
        clearParameters();

        const SerializationException *pSE =
            dynamic_cast<const SerializationException *>(&e);

        const RemoteException *pRE =
            dynamic_cast<const RemoteException *>(&e);

        const Exception *pE =
            dynamic_cast<const Exception *>(&e);

        if (pSE)
        {
            RCF_LOG_1()(typeid(*pSE))(*pSE) << "Returning RCF::SerializationException to client.";
            encodeRemoteException(
                mOut,
                RemoteException(
                    Error( pSE->getError() ),
                    pSE->what(),
                    pSE->getContext(),
                    typeid(*pSE).name()));
        }
        else if (pRE)
        {
            RCF_LOG_1()(typeid(*pRE))(*pRE) << "Returning RCF::RemoteException to client.";
            try
            {
                encodeRemoteException(mOut, *pRE);
            }
            catch(const RCF::Exception &e)
            {
                encodeRemoteException(
                    mOut,
                    RemoteException(
                        _RcfError_Serialization(typeid(*pRE).name(), typeid(e).name(), e.getError().getErrorString()),
                        e.getWhat(),
                        e.getContext(),
                        typeid(e).name()));
            }
            catch(const std::exception &e)
            {
                encodeRemoteException(
                    mOut,
                    RemoteException(
                        _RcfError_Serialization(typeid(*pRE).name(), typeid(e).name(), e.what()),
                        e.what(),
                        "",
                        typeid(e).name()));
            }
        }
        else if (pE)
        {
            RCF_LOG_1()(typeid(*pE))(*pE) << "Returning RCF::Exception to client.";
            encodeRemoteException(
                mOut,
                RemoteException(
                    Error( pE->getError() ),
                    pE->getSubSystemError(),
                    pE->getSubSystem(),
                    pE->what(),
                    pE->getContext(),
                    typeid(*pE).name()));
        }
        else
        {
            RCF_LOG_1()(typeid(e))(e) << "Returning std::exception to client.";
            encodeRemoteException(
                mOut,
                RemoteException(
                    _RcfError_AppException(typeid(e).name(), e.what()),
                    e.what(),
                    "",
                    typeid(e).name()));
        }

        sendSessionResponse();

        RCF_LOG_2() << "RcfServer - end remote call. " << mCurrentCallDesc;
    }

    class SessionTouch 
    {
    public:
        SessionTouch(RcfSession &rcfSession) : mRcfSession(rcfSession)
        {
            mRcfSession.touch();
        }
        ~SessionTouch()
        {
            mRcfSession.touch();
        }

    private:
        RcfSession & mRcfSession;
    };

    class StubEntryTouch 
    {
    public:
        StubEntryTouch(StubEntryPtr stubEntry) : mStubEntry(stubEntry)
        {
            if (mStubEntry)
            {
                mStubEntry->touch();
            }
        }
        ~StubEntryTouch()
        {
            if (mStubEntry)
            {
                mStubEntry->touch();
            }
        }

    private:
        StubEntryPtr mStubEntry;
    };

#ifdef RCF_USE_JSON

    void RcfSession::processJsonRpcRequest()
    {
        ByteBuffer readByteBuffer = getSessionState().getReadByteBuffer();

        RCF_LOG_3()(this)(readByteBuffer.getLength()) 
            << "RcfServer - received JSON-RPC packet from transport.";

        CurrentRcfSessionSentry guard(*this);

        boost::scoped_ptr<JsonRpcResponse> jsonResponsePtr;
        bool isOneway = false;
        boost::uint64_t jsonRequestId = 0;

        try
        {
            JsonRpcRequest jsonRequest(readByteBuffer);
            jsonRequestId = jsonRequest.getRequestId();
            jsonResponsePtr.reset( new JsonRpcResponse(jsonRequestId) );

            std::string jsonRpcName = jsonRequest.getMethodName();
            isOneway = jsonRequest.isNotification();

            RcfServer::JsonRpcMethod jsonRpcMethod;

            {
                ReadLock lock(mRcfServer.mStubMapMutex);
                RcfServer::JsonRpcMethods::iterator iter = mRcfServer.mJsonRpcMethods.find(jsonRpcName);
                if (iter != mRcfServer.mJsonRpcMethods.end())
                {
                    jsonRpcMethod = iter->second;
                }
            }

            if (jsonRpcMethod)
            {
                jsonRpcMethod(jsonRequest, *jsonResponsePtr);
            }
            else
            {
                std::string errMsg = "Unknown JSON-RPC method name: " + jsonRpcName;
                jsonResponsePtr->getJsonResponse()["error"] = errMsg;
            }
        }
        catch(...)
        {
            std::string errMsg;
            try
            {
                throw;
            }
            catch(const RCF::Exception & e)
            {
                errMsg = e.getErrorString();
            }
            catch(const std::exception & e)
            {
                errMsg = e.what();
            }
            catch(const std::string & jsonSpiritErrMsg)
            {
                errMsg = jsonSpiritErrMsg;
            }
            catch(...)
            {
                errMsg = "Caught C++ exception of unknown type.";
            }

            jsonResponsePtr.reset( new JsonRpcResponse(jsonRequestId) );
            jsonResponsePtr->getJsonResponse()["result"] = json_spirit::mValue();
            jsonResponsePtr->getJsonResponse()["error"] = errMsg;
        }

        setTlsRcfSessionPtr();

        if (isOneway)
        {
            onWriteCompleted();
        }
        else
        {
            json_spirit::mObject & obj = jsonResponsePtr->getJsonResponse();

            MemOstreamPtr osPtr = getObjectPool().getMemOstreamPtr();
            json_spirit::write_stream(json_spirit::mValue(obj), *osPtr, json_spirit::pretty_print);
            ByteBuffer buffer(osPtr->str(), static_cast<std::size_t>(osPtr->tellp()), osPtr);
            ThreadLocalCached< std::vector<ByteBuffer> > tlcByteBuffers;
            std::vector<ByteBuffer> & buffers = tlcByteBuffers.get();
            buffers.push_back(buffer);
            getSessionState().postWrite(buffers);
        }
    }

#else

    void RcfSession::processJsonRpcRequest()
    {
        std::string jsonErrorResponse = 
            "{\"error\" : \"RCF not built with JSON-RPC support.\", \"id\" : null}";

        ByteBuffer buffer(jsonErrorResponse);
        std::vector<ByteBuffer> buffers;
        buffers.push_back(buffer);
        getSessionState().postWrite(buffers);
    }

#endif


    void RcfSession::processRequest()
    {
        CurrentRcfSessionSentry guard(*this);

        using namespace boost::multi_index::detail;

        scope_guard sendResponseUncaughtExceptionGuard =
            make_obj_guard(
            *this,
            &RcfSession::sendResponseUncaughtException);

        try
        {
            mAutoSend = true;

            ++mRemoteCallCount;
        
            invokeServant();

            sendResponseUncaughtExceptionGuard.dismiss();

            // mAutoSend is false for async server dispatch.
            if (mAutoSend)
            {
                if (mRequest.mOneway)
                {
                    RCF_LOG_3()(this) << "RcfServer - suppressing response to oneway call.";
                    mIn.clearByteBuffer();
                    clearParameters();
                    setTlsRcfSessionPtr();
                    onWriteCompleted();
                }
                else
                {
                    sendResponse();
                }
            }
        }
        catch(const std::exception &e)
        {
            sendResponseUncaughtExceptionGuard.dismiss();
            if (mAutoSend)
            {
                if (mRequest.mOneway)
                {
                    mIn.clearByteBuffer();
                    clearParameters();
                    setTlsRcfSessionPtr();
                    onWriteCompleted();
                    
                }
                else
                {
                    sendResponseException(e);
                }
            }
        }
    }
    
    void RcfSession::verifyTransportProtocol(RCF::TransportProtocol protocol)
    {
        std::vector<TransportProtocol> supportedProtocols;
        supportedProtocols = mpSessionState->getServerTransport().getSupportedProtocols();
        if (supportedProtocols.empty())
        {
            supportedProtocols = mRcfServer.mSupportedProtocols;
        }

        if (supportedProtocols.size() > 0)
        {
            bool supported = false;
            for (std::size_t i=0; i<mRcfServer.mSupportedProtocols.size(); ++i)
            {
                if (mRcfServer.mSupportedProtocols[i] == protocol)
                {
                    supported = true;
                }
            }
            if (!supported)
            {
                throw Exception(RcfError_ProtocolNotSupported);
            }
        }
    }

    void RcfSession::invokeServant()
    {
        StubEntryPtr stubEntryPtr = mRequest.locateStubEntryPtr(mRcfServer);

        if (    NULL == stubEntryPtr.get() 
            &&  mRequest.getFnId() != -1)
        {
            Exception e( _RcfError_NoServerStub(
                mRequest.getService(), 
                mRequest.getSubInterface(),
                mRequest.getFnId()));

            RCF_THROW(e)(mRequest.getFnId());
        }
        else
        {
            setCachedStubEntryPtr(stubEntryPtr);

            SessionTouch sessionTouch(*this);

            StubEntryTouch stubEntryTouch(stubEntryPtr);

            if (!mTransportProtocolVerified)
            {
                if (mRequest.getService() != "I_RequestTransportFilters")
                {
                    if (mTransportProtocol == Tp_Clear)
                    {
                        const std::vector<TransportProtocol> & protocols = mRcfServer.getSupportedTransportProtocols();
                        if (protocols.size() > 0)
                        {
                            if (std::find(protocols.begin(), protocols.end(), Tp_Clear) == protocols.end())
                            {
                                std::string protocolNames;
                                for (std::size_t i=0; i<protocols.size(); ++i)
                                {
                                    if (i > 0)
                                    {
                                        protocolNames += ", ";
                                    }
                                    protocolNames += getTransportProtocolName(protocols[i]);
                                    
                                }
                                RCF_THROW( Exception( _RcfError_ClearCommunicationNotAllowed(protocolNames) ));
                            }
                        }
                    }

                    mTransportProtocolVerified = true;
                }
            }

            if (mRequest.getFnId() == -1)
            {
                // Function id -1 is a canned ping request. Set a timestamp 
                // on the current session and return immediately.

                AllocateServerParameters<Void>()(*this);
                setPingTimestamp();
            }
            else
            {
                if (!isInProcess())
                {
                    registerForPingBacks();

                    ThreadInfoPtr threadInfoPtr = getTlsThreadInfoPtr();
                    if (threadInfoPtr)
                    {
                        threadInfoPtr->notifyBusy();
                    }
                }

                stubEntryPtr->getRcfClientPtr()->getServerStub().invoke(
                    mRequest.getSubInterface(),
                    mRequest.getFnId(),
                    *this);
            }
        }
    }

    I_ServerTransport &RcfServer::getServerTransport()
    {
        return *getServerTransportPtr();
    }

    I_Service &RcfServer::getServerTransportService()
    {
        return dynamic_cast<I_Service &>(*getServerTransportPtr());
    }

    ServerTransportPtr RcfServer::getServerTransportPtr()
    {
        RCF_ASSERT( ! mServerTransports.empty() );
        return mServerTransports[0];
    }

    I_IpServerTransport &RcfServer::getIpServerTransport()
    {
        return dynamic_cast<RCF::I_IpServerTransport &>(getServerTransport());
    }    

    ServerBindingPtr RcfServer::bindImpl(
        const std::string &name,
        RcfClientPtr rcfClientPtr)
    {
        RCF_ASSERT(rcfClientPtr.get());
        RCF_LOG_2()(name) << "RcfServer - exposing static binding.";

        WriteLock writeLock(mStubMapMutex);
        mStubMap[name] = StubEntryPtr( new StubEntry(rcfClientPtr));
        return rcfClientPtr->getServerStubPtr();
    }

#ifdef RCF_USE_JSON

    void RcfServer::bindJsonRpc(JsonRpcMethod jsonRpcMethod, const std::string & jsonRpcName)
    {
        RCF_ASSERT(!jsonRpcName.empty());

        WriteLock writeLock(mStubMapMutex);
        mJsonRpcMethods[jsonRpcName] = jsonRpcMethod;
    }

    void RcfServer::unbindJsonRpc(const std::string & jsonRpcName)
    {
        WriteLock writeLock(mStubMapMutex);

        JsonRpcMethods::iterator iter = mJsonRpcMethods.find(jsonRpcName);
        if (iter != mJsonRpcMethods.end())
        {
            mJsonRpcMethods.erase(iter);
        }
    }

#endif

    FilterPtr RcfServer::createFilter(int filterId)
    {
        if (mFilterServicePtr)
        {
            FilterFactoryPtr filterFactoryPtr = 
                mFilterServicePtr->getFilterFactoryPtr(filterId);

            if (filterFactoryPtr)
            {
                return filterFactoryPtr->createFilter(*this);
            }
        }
        return FilterPtr();
    }

    void RcfServer::setStartCallback(const StartCallback &startCallback)
    {
        mStartCallback = startCallback;
    }

    void RcfServer::invokeStartCallback()
    {
        if (mStartCallback)
        {
            mStartCallback(*this);
        }
    }

    boost::uint32_t RcfServer::getRuntimeVersion()
    {
        return mRuntimeVersion;
    }

    void RcfServer::setRuntimeVersion(boost::uint32_t version)
    {
        mRuntimeVersion = version;
    }

    boost::uint32_t RcfServer::getArchiveVersion()
    {
        return mArchiveVersion;
    }

    void RcfServer::setArchiveVersion(boost::uint32_t version)
    {
        mArchiveVersion = version;
    }

    PingBackServicePtr RcfServer::getPingBackServicePtr()
    {
        return mPingBackServicePtr;
    }

    FileTransferServicePtr RcfServer::getFileTransferServicePtr()
    {
        return mFileTransferServicePtr;
    }

    SessionTimeoutServicePtr RcfServer::getSessionTimeoutServicePtr()
    {
        return mSessionTimeoutServicePtr;
    }

    ObjectFactoryServicePtr RcfServer::getObjectFactoryServicePtr()
    {
        return mObjectFactoryServicePtr;
    }

    SessionObjectFactoryServicePtr RcfServer::getSessionObjectFactoryServicePtr()
    {
        return mSessionObjectFactoryServicePtr;
    }

    PublishingServicePtr RcfServer::getPublishingServicePtr()
    {
        return mPublishingServicePtr;
    }

    SubscriptionServicePtr RcfServer::getSubscriptionServicePtr()
    {
        return mSubscriptionServicePtr;
    }

    void RcfServer::setThreadPool(ThreadPoolPtr threadPoolPtr)
    {
        if (threadPoolPtr->getThreadName().empty())
        {
            threadPoolPtr->setThreadName("RCF Server");
        }
        mThreadPoolPtr = threadPoolPtr;
    }

    ThreadPoolPtr RcfServer::getThreadPool()
    {
        return mThreadPoolPtr;
    }

    I_ServerTransport & RcfServer::addEndpoint(const RCF::I_Endpoint & endpoint)
    {
        ServerTransportPtr transportPtr(endpoint.createServerTransport().release());
        addServerTransport(transportPtr);
        return *transportPtr;
    }

} // namespace RCF

#include <RCF/Config.hpp>

#if defined(BOOST_WINDOWS)
#include <RCF/Win32NamedPipeClientTransport.hpp>
#include <RCF/Win32NamedPipeServerTransport.hpp>
#endif

#include <RCF/TcpAsioServerTransport.hpp>

#if defined(RCF_HAS_LOCAL_SOCKETS)
#include <RCF/UnixLocalServerTransport.hpp>
#include <RCF/UnixLocalClientTransport.hpp>
#endif

namespace RCF {

    I_ServerTransport & RcfServer::findTransportCompatibleWith(
        I_ClientTransport & clientTransport)
    {
        std::string ti = typeid(clientTransport).name();
        std::vector<std::string> serverTypeids;
        if (ti == typeid(TcpClientTransport).name())
        {
            serverTypeids.push_back( typeid(TcpAsioServerTransport).name() );
        }

#if defined(BOOST_WINDOWS)
        else if (ti == typeid(Win32NamedPipeClientTransport).name())
        {
            serverTypeids.push_back( typeid(Win32NamedPipeServerTransport).name() );
        }
#endif

#ifdef RCF_HAS_LOCAL_SOCKETS
        else if (ti == typeid(UnixLocalClientTransport).name())
        {
            serverTypeids.push_back( typeid(UnixLocalServerTransport).name() );
        }
#endif

        for (std::size_t i=0; i<mServerTransports.size(); ++i)
        {
            for (std::size_t j=0; j<serverTypeids.size(); ++j)
            {
                if (typeid(*mServerTransports[i]).name() == serverTypeids[j])
                {
                    // Got one.
                    return *mServerTransports[i];
                }
            }
        }

        RCF_THROW(Exception("No corresponding server transport."));
        
        return * (I_ServerTransport *) NULL;
    }

    void RcfServer::setSupportedTransportProtocols(
        const std::vector<TransportProtocol> & protocols)
    {
        RCF_ASSERT(!mStarted);
        mSupportedProtocols = protocols;
    }

    const std::vector<TransportProtocol> & 
        RcfServer::getSupportedTransportProtocols() const
    {
        return mSupportedProtocols;
    }

    void RcfServer::setSslCertificate(CertificatePtr certificatePtr)
    {
        WriteLock lock(mPropertiesMutex);
        mCertificatePtr = certificatePtr;
    }

    CertificatePtr RcfServer::getSslCertificate()
    {
        ReadLock lock(mPropertiesMutex);
        return mCertificatePtr;
    }

    void RcfServer::setOpenSslCipherSuite(const std::string & cipherSuite)
    {
        WriteLock lock(mPropertiesMutex);
        mOpenSslCipherSuite = cipherSuite;
    }

    std::string RcfServer::getOpenSslCipherSuite() const
    {
        ReadLock lock(mPropertiesMutex);
        return mOpenSslCipherSuite;
    }



    //--------------------------------------------------------------------------
    // Certificate validation.

    void RcfServer::setSslCaCertificate(CertificatePtr caCertificatePtr)
    {
        WriteLock lock(mPropertiesMutex);
        mCaCertificatePtr = caCertificatePtr;

        mOpenSslCertificateValidationCb.clear();
        mSchannelCertificateValidationCb.clear();
        mSchannelAutoCertificateValidation.clear();
    }

    CertificatePtr RcfServer::getSslCaCertificate()
    {
        ReadLock lock(mPropertiesMutex);
        return mCaCertificatePtr;
    }

    void RcfServer::setOpenSslCertificateValidationCb(
        OpenSslCertificateValidationCb certificateValidationCb)
    {
        WriteLock lock(mPropertiesMutex);
        mOpenSslCertificateValidationCb = certificateValidationCb;

        mCaCertificatePtr.reset();
        mSchannelCertificateValidationCb.clear();
        mSchannelAutoCertificateValidation.clear();
    }

    const RcfServer::OpenSslCertificateValidationCb & RcfServer::getOpenSslCertificateValidationCb() const
    {
        ReadLock lock(mPropertiesMutex);
        return mOpenSslCertificateValidationCb;
    }

    void RcfServer::setSchannelCertificateValidationCb(SchannelCertificateValidationCb certificateValidationCb)
    {
        WriteLock lock(mPropertiesMutex);
        mSchannelCertificateValidationCb = certificateValidationCb;

        mCaCertificatePtr.reset();
        mOpenSslCertificateValidationCb.clear();
        mSchannelAutoCertificateValidation.clear();
    }

    const RcfServer::SchannelCertificateValidationCb & 
    RcfServer::getSchannelCertificateValidationCb() const
    {
        ReadLock lock(mPropertiesMutex);
        return mSchannelCertificateValidationCb;
    }

    void RcfServer::setSchannelDefaultCertificateValidation(const tstring & peerName)
    {
        WriteLock lock(mPropertiesMutex);
        mSchannelAutoCertificateValidation = peerName;

        mCaCertificatePtr.reset();
        mOpenSslCertificateValidationCb.clear();
        mSchannelCertificateValidationCb.clear();
    }

    tstring RcfServer::getSchannelDefaultCertificateValidation() const
    {
        ReadLock lock(mPropertiesMutex);
        return mSchannelAutoCertificateValidation;
    }

    void RcfServer::setPreferSchannel(bool preferSchannel)
    {
        RCF_ASSERT(!mStarted);
        mPreferSchannel = preferSchannel;
    }

    bool RcfServer::getPreferSchannel() const
    {
        return mPreferSchannel;
    }

    void RcfServer::setSessionTimeoutMs(boost::uint32_t sessionTimeoutMs)
    {
        RCF_ASSERT(!mStarted);
        mSessionTimeoutMs = sessionTimeoutMs;
    }

    boost::uint32_t RcfServer::getSessionTimeoutMs()
    {
        return mSessionTimeoutMs;
    }

    void RcfServer::setSessionReapingIntervalMs(boost::uint32_t sessionReapingIntervalMs)
    {
        RCF_ASSERT(!mStarted);
        mSessionReapingIntervalMs = sessionReapingIntervalMs;
    }

    boost::uint32_t RcfServer::getSessionReapingIntervalMs()
    {
        return mSessionReapingIntervalMs;
    }

    void RcfServer::setOnCallbackConnectionCreated(OnCallbackConnectionCreated onCallbackConnectionCreated)
    {
        RCF_ASSERT(!mStarted);
        mOnCallbackConnectionCreated = onCallbackConnectionCreated;
    }

    RcfServer::OnCallbackConnectionCreated RcfServer::getOnCallbackConnectionCreated()
    {
        return mOnCallbackConnectionCreated;
    }

    void RcfServer::setOfsMaxNumberOfObjects(boost::uint32_t ofsMaxNumberOfObjects)
    {
        RCF_ASSERT(!mStarted);
        mOfsMaxNumberOfObjects = ofsMaxNumberOfObjects;
    }

    void RcfServer::setOfsObjectTimeoutS(boost::uint32_t ofsObjectTimeoutS)
    {
        RCF_ASSERT(!mStarted);
        mOfsObjectTimeoutS = ofsObjectTimeoutS;
    }

    void RcfServer::setOfsCleanupIntervalS(boost::uint32_t ofsCleanupIntervalS)
    {
        RCF_ASSERT(!mStarted);
        mOfsCleanupIntervalS = ofsCleanupIntervalS;
    }

    void RcfServer::setOfsCleanupThreshold(float ofsCleanupThreshold)
    {
        RCF_ASSERT(!mStarted);
        mOfsCleanupThreshold = ofsCleanupThreshold;
    }

    boost::uint32_t RcfServer::getOfsMaxNumberOfObjects() const
    {
        return mOfsMaxNumberOfObjects;
    }

    boost::uint32_t  RcfServer::getOfsObjectTimeoutS() const
    {
        return mOfsObjectTimeoutS;
    }

    boost::uint32_t RcfServer::getOfsCleanupIntervalS() const
    {
        return mOfsCleanupIntervalS;
    }

    float RcfServer::getOfsCleanupThreshold() const
    {
        return mOfsCleanupThreshold;
    }

#ifdef RCF_USE_BOOST_FILESYSTEM

    void RcfServer::setOnFileDownloadProgress(OnFileDownloadProgress onFileDownloadProgress)
    {
        RCF_ASSERT(!mStarted);
        mOnFileDownloadProgress = onFileDownloadProgress;
    }

    void RcfServer::setOnFileUploadProgress(OnFileUploadProgress onFileUploadProgress)
    {
        RCF_ASSERT(!mStarted);
        mOnFileUploadProgress = onFileUploadProgress;
    }

    void RcfServer::setFileUploadDirectory(const std::string & uploadFolder)
    {
        RCF_ASSERT(!mStarted);
        mFileUploadDirectory = uploadFolder;
    }

    std::string RcfServer::getFileUploadDirectory() const
    {
        return mFileUploadDirectory;
    }

    void RcfServer::setFileUploadBandwidthLimit(boost::uint32_t uploadQuotaBps)
    {
        RCF_ASSERT(!mStarted);
        mFileUploadQuota = uploadQuotaBps;
    }

    boost::uint32_t RcfServer::getFileUploadBandwidthLimit() const
    {
        return mFileUploadQuota;
    }

    void RcfServer::setFileUploadCustomBandwidthLimit(FileUploadQuotaCallback uploadQuotaCb)
    {
        RCF_ASSERT(!mStarted);
        mFileUploadQuotaCb = uploadQuotaCb;
    }

    void RcfServer::setFileDownloadBandwidthLimit(boost::uint32_t downloadQuotaBps)
    {
        RCF_ASSERT(!mStarted);
        mFileDownloadQuota = downloadQuotaBps;
    }

    boost::uint32_t RcfServer::getFileDownloadBandwidthLimit() const
    {
        return mFileDownloadQuota;
    }

    void RcfServer::setFileDownloadCustomBandwidthLimit(FileDownloadQuotaCallback downloadQuotaCb)
    {
        RCF_ASSERT(!mStarted);
        mFileDownloadQuotaCb = downloadQuotaCb;
    }

#endif

}


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

#include <RCF/AsioServerTransport.hpp>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include <RCF/Asio.hpp>
#include <RCF/Filter.hpp>
#include <RCF/ConnectionOrientedClientTransport.hpp>
#include <RCF/CurrentSession.hpp>
#include <RCF/HttpFrameFilter.hpp>
#include <RCF/MethodInvocation.hpp>
#include <RCF/ObjectPool.hpp>
#include <RCF/RcfServer.hpp>
#include <RCF/TimedBsdSockets.hpp>

namespace RCF {    

    // FilterAdapter

    class FilterAdapter : public RCF::IdentityFilter
    {
    public:
        FilterAdapter(AsioSessionState &sessionState) :
            mSessionState(sessionState)
        {}

    private:
        void read(
            const ByteBuffer &byteBuffer,
            std::size_t bytesRequested)
        {
            mSessionState.read(byteBuffer, bytesRequested);
        }

        void write(
            const std::vector<ByteBuffer> &byteBuffers)
        {
            mSessionState.write(byteBuffers);
        }

        void onReadCompleted(
            const ByteBuffer &byteBuffer)
        {
            mSessionState.onAppReadWriteCompleted(byteBuffer.getLength());
        }

        void onWriteCompleted(
            std::size_t bytesTransferred)
        {
            mSessionState.onAppReadWriteCompleted(bytesTransferred);
        }

        int getFilterId() const
        {
            return RcfFilter_Unknown;
        }

        AsioSessionState &mSessionState;
    };


    ReadHandler::ReadHandler(AsioSessionStatePtr sessionStatePtr) : 
        mSessionStatePtr(sessionStatePtr)
    {
    }

    void ReadHandler::operator()(AsioErrorCode err, std::size_t bytes)
    {
        mSessionStatePtr->onNetworkReadCompleted(err, bytes);
    }

    void * ReadHandler::allocate(std::size_t size)
    {
        if (mSessionStatePtr->mReadHandlerBuffer.size() < size)
        {
            mSessionStatePtr->mReadHandlerBuffer.resize(size);
        }
        return & mSessionStatePtr->mReadHandlerBuffer[0];
    }

    WriteHandler::WriteHandler(AsioSessionStatePtr sessionStatePtr) : 
        mSessionStatePtr(sessionStatePtr)
    {
    }

    void WriteHandler::operator()(AsioErrorCode err, std::size_t bytes)
    {
        mSessionStatePtr->onNetworkWriteCompleted(err, bytes);
    }

    void * WriteHandler::allocate(std::size_t size)
    {
        if (mSessionStatePtr->mWriteHandlerBuffer.size() < size)
        {
            mSessionStatePtr->mWriteHandlerBuffer.resize(size);
        }
        return & mSessionStatePtr->mWriteHandlerBuffer[0];
    }

    void * asio_handler_allocate(std::size_t size, ReadHandler * pHandler)
    {
        return pHandler->allocate(size);
    }

    void asio_handler_deallocate(void * pointer, std::size_t size, ReadHandler * pHandler)
    {
        RCF_UNUSED_VARIABLE(pointer);
        RCF_UNUSED_VARIABLE(size);
        RCF_UNUSED_VARIABLE(pHandler);
    }

    void * asio_handler_allocate(std::size_t size, WriteHandler * pHandler)
    {
        return pHandler->allocate(size);
    }

    void asio_handler_deallocate(void * pointer, std::size_t size, WriteHandler * pHandler)
    {
        RCF_UNUSED_VARIABLE(pointer);
        RCF_UNUSED_VARIABLE(size);
        RCF_UNUSED_VARIABLE(pHandler);
    }

    void AsioSessionState::postRead()
    {
        if (mLastError)
        {
            return;
        }

        mState = AsioSessionState::ReadingDataCount;

        mAppReadByteBuffer.clear();
        mAppReadBufferPtr.reset();

        mNetworkReadByteBuffer.clear();
        mNetworkReadBufferPtr.reset();

        mReadBufferRemaining = 0;
        mIssueZeroByteRead = true;
        beginRead();
    }

    void AsioSessionState::postWrite(
        std::vector<ByteBuffer> &byteBuffers)
    {
        if (mLastError)
        {
            return;
        }

        BOOST_STATIC_ASSERT(sizeof(unsigned int) == 4);

        mSlicedWriteByteBuffers.resize(0);
        mWriteByteBuffers.resize(0);

        std::copy(
            byteBuffers.begin(),
            byteBuffers.end(),
            std::back_inserter(mWriteByteBuffers));

        byteBuffers.resize(0);

        if (!mTransport.mCustomFraming)
        {
            // Add frame (4 byte length prefix).
            int messageSize = 
                static_cast<int>(RCF::lengthByteBuffers(mWriteByteBuffers));

            ByteBuffer &byteBuffer = mWriteByteBuffers.front();

            RCF_ASSERT_GTEQ(byteBuffer.getLeftMargin() , 4);
            byteBuffer.expandIntoLeftMargin(4);
            memcpy(byteBuffer.getPtr(), &messageSize, 4);
            RCF::machineToNetworkOrder(byteBuffer.getPtr(), 4, 1);
        }

        mState = AsioSessionState::WritingData;
        
        mWriteBufferRemaining = RCF::lengthByteBuffers(mWriteByteBuffers);
        
        beginWrite();
    }

    void AsioSessionState::postClose()
    {
        close();
    }

    ServerTransport & AsioSessionState::getServerTransport()
    {
        return mTransport;
    }

    const RemoteAddress &
        AsioSessionState::getRemoteAddress()
    {
        return implGetRemoteAddress();
    }

    bool AsioSessionState::isConnected()
    {
        return implIsConnected();
    }

    // SessionState

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4355 )  // warning C4355: 'this' : used in base member initializer list
#endif

    AsioSessionState::AsioSessionState(
        AsioServerTransport & transport,
        AsioIoService & ioService) :
            mIoService(ioService),
            mState(Ready),
            mIssueZeroByteRead(false),
            mReadBufferRemaining(),
            mWriteBufferRemaining(),
            mTransport(transport),
            mFilterAdapterPtr(new FilterAdapter(*this)),
            mCloseAfterWrite(),
            mReflecting()
    {
        if (transport.mWireProtocol == Wp_Http || transport.mWireProtocol == Wp_Https)
        {
#if RCF_FEATURE_HTTP==1
            mWireFilters.clear();
            mWireFilters.push_back( FilterPtr(new HttpFrameFilter()) );
#else
            RCF_ASSERT(0 && "This RCF build does not support HTTP tunneling.");
#endif
        }

        if (transport.mWireProtocol == Wp_Https)
        {
            FilterPtr sslFilterPtr;
            if (transport.mpServer->getSslImplementation() == Si_Schannel)
            {
                sslFilterPtr = transport.mpServer->createFilter(RcfFilter_SspiSchannel);
            }
            else
            {
                sslFilterPtr = transport.mpServer->createFilter(RcfFilter_OpenSsl);
            }
            if (!sslFilterPtr)
            {
                RCF_THROW( Exception(_RcfError_SslNotSupported()) );
            }

            mWireFilters.push_back( sslFilterPtr );
        }

        if (mWireFilters.size() > 0)
        {
            setTransportFilters( std::vector<FilterPtr>() );
        }
    }

#ifdef _MSC_VER
#pragma warning( pop )
#endif

    AsioSessionState::~AsioSessionState()
    {
        RCF_DTOR_BEGIN

        // TODO: invoke accept if appropriate
        // TODO: need a proper acceptex strategy in the first place
        //RCF_ASSERT(mState != Accepting);

        mTransport.unregisterSession(mWeakThisPtr);

        RCF_LOG_4()(mState)(mSessionPtr.get())(mSessionPtr->mDisableIo)
            << "AsioSessionState - destructor.";

        // close reflecting session if appropriate
        if (mReflecting)
        {
            AsioSessionStatePtr sessionStatePtr(mReflecteeWeakPtr.lock());
            if (sessionStatePtr)
            {
                sessionStatePtr->close();
            }
        }

        RCF_DTOR_END;
    }

    AsioSessionStatePtr AsioSessionState::sharedFromThis()
    {
        return boost::static_pointer_cast<AsioSessionState>(shared_from_this());
    }

    ByteBuffer AsioSessionState::getReadByteBuffer()
    {
        if (!mAppReadBufferPtr)
        {
            return ByteBuffer();            
        }
        return ByteBuffer(mAppReadBufferPtr);
    }

    void AsioSessionState::read(
        const ByteBuffer &byteBuffer,
        std::size_t bytesRequested)
    {
        if (byteBuffer.getLength() == 0 && bytesRequested > 0)
        {
            if (!mNetworkReadBufferPtr || mNetworkReadBufferPtr.unique())
            {
                mNetworkReadBufferPtr = getObjectPool().getReallocBufferPtr();
            }
            mNetworkReadBufferPtr->resize(bytesRequested);
            mNetworkReadByteBuffer = ByteBuffer(mNetworkReadBufferPtr);
        }
        else
        {
            mNetworkReadByteBuffer = ByteBuffer(byteBuffer, 0, bytesRequested);
        }

        RCF_ASSERT_LTEQ(bytesRequested, mNetworkReadByteBuffer.getLength());

        char *buffer = mNetworkReadByteBuffer.getPtr();
        std::size_t bufferLen = mNetworkReadByteBuffer.getLength();

        Lock lock(mSessionPtr->mDisableIoMutex);
        if (!mSessionPtr->mDisableIo)
        {
            if (mSocketOpsMutexPtr)
            {
                Lock lock(*mSocketOpsMutexPtr);
                implRead(buffer, bufferLen);
            }
            else
            {
                implRead(buffer, bufferLen);
            }
        }
    }

    void AsioSessionState::write(
        const std::vector<ByteBuffer> &byteBuffers)
    {
        RCF_ASSERT(!byteBuffers.empty());

        Lock lock(mSessionPtr->mDisableIoMutex);
        if (!mSessionPtr->mDisableIo)
        {
            if (mSocketOpsMutexPtr)
            {
                Lock lock(*mSocketOpsMutexPtr);
                implWrite(byteBuffers);
            }
            else
            {
                implWrite(byteBuffers);
            }
        }
    }

    // TODO: merge onReadCompletion/onWriteCompletion into one function

    void AsioSessionState::onNetworkReadCompleted(
        AsioErrorCode error, size_t bytesTransferred)
    {
        RCF_LOG_4()(this)(bytesTransferred) << "AsioSessionState - read from socket completed.";

        ThreadTouchGuard threadTouchGuard;

        mLastError = error;

        mBytesReceivedCounter += bytesTransferred;

#ifdef BOOST_WINDOWS

        if (error.value() == ERROR_OPERATION_ABORTED)
        {
            error = AsioErrorCode();
        }

#endif

        if (!error && !mTransport.mStopFlag)
        {
            if (bytesTransferred == 0 && mIssueZeroByteRead)
            {
                // TCP framing.
                if (!mAppReadBufferPtr || !mAppReadBufferPtr.unique())
                {
                    mAppReadBufferPtr = getObjectPool().getReallocBufferPtr();
                }
                mAppReadBufferPtr->resize(4);

                mReadBufferRemaining = 4;
                mIssueZeroByteRead = false;
                beginRead();
            }
            else if (mReflecting)
            {
                AsioErrorCode ec;
                onReflectedReadWriteCompleted(ec, bytesTransferred);
            }
            else
            {
                CurrentRcfSessionSentry guard(*mSessionPtr);

                mNetworkReadByteBuffer = ByteBuffer(
                    mNetworkReadByteBuffer,
                    0,
                    bytesTransferred);

                mTransportFilters.empty() ?
                    onAppReadWriteCompleted(bytesTransferred) : 
                    mTransportFilters.back()->onReadCompleted(mNetworkReadByteBuffer);
            }
        }
    }

    void AsioSessionState::onNetworkWriteCompleted(
        AsioErrorCode error, 
        size_t bytesTransferred)
    {
        RCF_LOG_4()(this)(bytesTransferred) << "AsioSessionState - write to socket completed.";

        ThreadTouchGuard threadTouchGuard;

        mLastError = error;

        mBytesSentCounter += bytesTransferred;

#ifdef BOOST_WINDOWS

        if (error.value() == ERROR_OPERATION_ABORTED)
        {
            error = AsioErrorCode();
        }

#endif

        if (!error && !mTransport.mStopFlag)
        {
            if (mReflecting)
            {
                if (mReflecteePtr)
                {
                    mReflecteePtr.reset();
                }
                AsioErrorCode ec;
                onReflectedReadWriteCompleted(ec, bytesTransferred);
            }
            else
            {
                CurrentRcfSessionSentry guard(*mSessionPtr);
                mTransportFilters.empty() ?
                    onAppReadWriteCompleted(bytesTransferred) :
                    mTransportFilters.back()->onWriteCompleted(bytesTransferred);
            }
        }
    }

    void AsioSessionState::setTransportFilters(
        const std::vector<FilterPtr> &filters)
    {
        mTransportFilters.assign(filters.begin(), filters.end());

        std::copy(
            mWireFilters.begin(), 
            mWireFilters.end(), 
            std::back_inserter(mTransportFilters));

        connectFilters(mTransportFilters);

        if (!mTransportFilters.empty())
        {
            mTransportFilters.front()->setPreFilter( *mFilterAdapterPtr );
            mTransportFilters.back()->setPostFilter( *mFilterAdapterPtr );
        }
    }

    void AsioSessionState::getTransportFilters(
        std::vector<FilterPtr> &filters)
    {
        filters = mTransportFilters;
        for (std::size_t i=0; i<mWireFilters.size(); ++i)
        {
            filters.pop_back();
        }
    }

    void AsioSessionState::beginRead()
    {
        RCF_ASSERT(
                mReadBufferRemaining == 0 
            ||  (mAppReadBufferPtr && mAppReadBufferPtr->size() >= mReadBufferRemaining));

        mAppReadByteBuffer = ByteBuffer();
        if (mAppReadBufferPtr)
        {
            char * pch = & (*mAppReadBufferPtr)[mAppReadBufferPtr->size() - mReadBufferRemaining];
            mAppReadByteBuffer = ByteBuffer(pch, mReadBufferRemaining, mAppReadBufferPtr);
        }

        mTransportFilters.empty() ?
            read(mAppReadByteBuffer, mReadBufferRemaining) :
            mTransportFilters.front()->read(mAppReadByteBuffer, mReadBufferRemaining);
    }

    void AsioSessionState::beginWrite()
    {
        mSlicedWriteByteBuffers.resize(0);

        sliceByteBuffers(
            mSlicedWriteByteBuffers,
            mWriteByteBuffers,
            lengthByteBuffers(mWriteByteBuffers)-mWriteBufferRemaining);

        mTransportFilters.empty() ?
            write(mSlicedWriteByteBuffers) :
            mTransportFilters.front()->write(mSlicedWriteByteBuffers);
    }

    void AsioSessionState::beginAccept()
    {
        mState = Accepting;
        implAccept();
    }

    void AsioSessionState::onAcceptCompleted(
        const AsioErrorCode & error)
    {
        RCF_LOG_4()(error.value())
            << "AsioSessionState - onAccept().";

        if (mTransport.mStopFlag)
        {
            RCF_LOG_4()(error.value())
                << "AsioSessionState - onAccept(). Returning early, stop flag is set.";

            return;
        }

        //if (
        //    error == ASIO_NS::error::connection_aborted ||
        //    error == ASIO_NS::error::operation_aborted)
        //{
        //    beginAccept();
        //    return;
        //}

        // create a new SessionState, and do an accept on that
        mTransport.createSessionState()->beginAccept();

        if (!error)
        {
            // save the remote address in the SessionState object
            bool clientAddrAllowed = implOnAccept();
            mState = WritingData;

            // set current RCF session
            CurrentRcfSessionSentry guard(*mSessionPtr);

            mSessionPtr->touch();

            if (clientAddrAllowed)
            {
                // Check the connection limit.
                bool allowConnect = true;
                std::size_t connectionLimit = mTransport.getConnectionLimit();
                if (connectionLimit)
                {
                    Lock lock(mTransport.mSessionsMutex);
                    
                    RCF_ASSERT_LTEQ(
                        mTransport.mSessions.size() , 1+1+connectionLimit);

                    if (mTransport.mSessions.size() == 1+1+connectionLimit)
                    {
                        allowConnect = false;
                    }
                }

                if (allowConnect)
                {
                    time_t now = 0;
                    now = time(NULL);
                    mSessionPtr->setConnectedAtTime(now);

                    // start things rolling by faking a completed write operation
                    onAppReadWriteCompleted(0);
                }
                else
                {
                    sendServerError(RcfError_ConnectionLimitExceeded);
                }
            }
        }
    }

    void onError(
        AsioErrorCode & error1, 
        const AsioErrorCode & error2)
    {
        error1 = error2;
    }

    void AsioSessionState::sendServerError(int error)
    {
        mState = Ready;
        mCloseAfterWrite = true;
        std::vector<ByteBuffer> byteBuffers(1);
        encodeServerError(*mTransport.mpServer, byteBuffers.front(), error);
        mSessionPtr->getSessionState().postWrite(byteBuffers);
    }

    void AsioSessionState::doRegularFraming(size_t bytesTransferred)
    {
        RCF_ASSERT_LTEQ(bytesTransferred , mReadBufferRemaining);
        mReadBufferRemaining -= bytesTransferred;
        if (mReadBufferRemaining > 0)
        {
            beginRead();
        }
        else if (mReadBufferRemaining == 0 && mIssueZeroByteRead)
        {
            if (!mAppReadBufferPtr || !mAppReadBufferPtr.unique())
            {
                mAppReadBufferPtr = getObjectPool().getReallocBufferPtr();
            }
            mAppReadBufferPtr->resize(4);

            mReadBufferRemaining = 4;
            mIssueZeroByteRead = false;
            beginRead();
        }
        else
        {
            RCF_ASSERT_EQ(mReadBufferRemaining , 0);
            if (mState == ReadingDataCount)
            {
                ReallocBuffer & readBuffer = *mAppReadBufferPtr;
                RCF_ASSERT_EQ(readBuffer.size() , 4);

                unsigned int packetLength = 0;
                memcpy(&packetLength, &readBuffer[0], 4);
                networkToMachineOrder(&packetLength, 4, 1);

                if (    mTransport.getMaxMessageLength() 
                    &&  packetLength > mTransport.getMaxMessageLength())
                {
                    sendServerError(RcfError_ServerMessageLength);
                }
                else
                {
                    readBuffer.resize(packetLength);
                    mReadBufferRemaining = packetLength;
                    mState = ReadingData;
                    beginRead();
                }
            }
            else if (mState == ReadingData)
            {
                mState = Ready;

                mTransport.getSessionManager().onReadCompleted(
                    getSessionPtr());
            }
        }
    }

    void AsioSessionState::doCustomFraming(size_t bytesTransferred)
    {
        RCF_ASSERT_LTEQ(bytesTransferred , mReadBufferRemaining);
        mReadBufferRemaining -= bytesTransferred;
        if (mReadBufferRemaining > 0)
        {
            beginRead();
        }
        else if (mReadBufferRemaining == 0 && mIssueZeroByteRead)
        {
            if (!mAppReadBufferPtr || !mAppReadBufferPtr.unique())
            {
                mAppReadBufferPtr = getObjectPool().getReallocBufferPtr();
            }
            mAppReadBufferPtr->resize(4);

            mReadBufferRemaining = 4;
            mIssueZeroByteRead = false;
            beginRead();
        }
        else
        {
            RCF_ASSERT_EQ(mReadBufferRemaining , 0);
            if (mState == ReadingDataCount)
            {
                ReallocBuffer & readBuffer = *mAppReadBufferPtr;
                RCF_ASSERT_EQ(readBuffer.size() , 4);

                std::size_t messageLength = mTransportFilters[0]->getFrameSize();

                if (    mTransport.getMaxMessageLength() 
                    &&  messageLength > mTransport.getMaxMessageLength())
                {
                    sendServerError(RcfError_ServerMessageLength);
                }
                else
                {
                    RCF_ASSERT( messageLength > 4 );
                    readBuffer.resize(messageLength);
                    mReadBufferRemaining = messageLength - 4;
                    mState = ReadingData;
                    beginRead();
                }
            }
            else if (mState == ReadingData)
            {
                mState = Ready;

                mTransport.getSessionManager().onReadCompleted(
                    getSessionPtr());
            }
        }
    }

    void AsioSessionState::onAppReadWriteCompleted(
        size_t bytesTransferred)
    {
        RCF_ASSERT(!mReflecting);
        switch(mState)
        {
        case ReadingDataCount:
        case ReadingData:

            if (mTransport.mCustomFraming)
            {
                doCustomFraming(bytesTransferred);
            }
            else
            {
                doRegularFraming(bytesTransferred);
            }
            
            break;

        case WritingData:

            RCF_ASSERT_LTEQ(bytesTransferred , mWriteBufferRemaining);

            mWriteBufferRemaining -= bytesTransferred;
            if (mWriteBufferRemaining > 0)
            {
                beginWrite();
            }
            else
            {
                if (mCloseAfterWrite)
                {
                    // For TCP sockets, call shutdown() so client receives 
                    // the message before we close the connection.

                    implCloseAfterWrite();                        
                }
                else
                {
                    mState = Ready;

                    mSlicedWriteByteBuffers.resize(0);
                    mWriteByteBuffers.resize(0);

                    mTransport.getSessionManager().onWriteCompleted(
                        getSessionPtr());
                }
            }
            break;

        default:
            RCF_ASSERT(0);
        }
    }

    void AsioSessionState::onReflectedReadWriteCompleted(
        const AsioErrorCode & error,
        size_t bytesTransferred)
    {
        RCF_UNUSED_VARIABLE(error);

        RCF_ASSERT(
            mState == ReadingData ||
            mState == ReadingDataCount ||
            mState == WritingData)
            (mState);

        RCF_ASSERT(mReflecting);

        if (bytesTransferred == 0)
        {
            // Previous operation was aborted for some reason (probably because
            // of a thread exiting). Reissue the operation.

            mState = (mState == WritingData) ? ReadingData : WritingData;
        }

        if (mState == WritingData)
        {
            mState = ReadingData;
            ReallocBuffer & readBuffer = *mAppReadBufferPtr;
            readBuffer.resize(8*1024);

            char *buffer = &readBuffer[0];
            std::size_t bufferLen = readBuffer.size();

            Lock lock(mSessionPtr->mDisableIoMutex);
            if (!mSessionPtr->mDisableIo)
            {
                if (mSocketOpsMutexPtr)
                {
                    Lock lock(*mSocketOpsMutexPtr);
                    implRead(buffer, bufferLen);
                }
                else
                {
                    implRead(buffer, bufferLen);
                }
            }
        }
        else if (
            mState == ReadingData ||
            mState == ReadingDataCount)
        {
            mState = WritingData;
            ReallocBuffer & readBuffer = *mAppReadBufferPtr;

            char *buffer = &readBuffer[0];
            std::size_t bufferLen = bytesTransferred;

            // mReflecteePtr will be nulled in onWriteCompletion(), otherwise 
            // we could easily end up with a cycle
            RCF_ASSERT(!mReflecteePtr);
            mReflecteePtr = mReflecteeWeakPtr.lock();
            if (mReflecteePtr)
            {
                RCF_ASSERT(mReflecteePtr->mReflecting);

                Lock lock(mReflecteePtr->mSessionPtr->mDisableIoMutex);
                if (!mReflecteePtr->mSessionPtr->mDisableIo)
                {
                    // TODO: if this can throw, then we need a scope_guard
                    // to reset mReflecteePtr

                    if (mReflecteePtr->mSocketOpsMutexPtr)
                    {
                        Lock lock(*mReflecteePtr->mSocketOpsMutexPtr);
                        mReflecteePtr->implWrite(*this, buffer, bufferLen);
                    }
                    else
                    {
                        mReflecteePtr->implWrite(*this, buffer, bufferLen);
                    }
                }
            }
        }
    }

    // AsioServerTransport

    AsioSessionStatePtr AsioServerTransport::createSessionState()
    {
        AsioSessionStatePtr sessionStatePtr( implCreateSessionState() );
        sessionStatePtr->mSessionPtr = getSessionManager().createSession();
        sessionStatePtr->mSessionPtr->setSessionState( *sessionStatePtr );

        sessionStatePtr->mWeakThisPtr = sessionStatePtr;
        registerSession(sessionStatePtr->mWeakThisPtr);

        return sessionStatePtr;
    }

    // I_ServerTransportEx implementation

    ClientTransportAutoPtr AsioServerTransport::createClientTransport(
        const Endpoint &endpoint)
    {
        return implCreateClientTransport(endpoint);
    }

    SessionPtr AsioServerTransport::createServerSession(
        ClientTransportAutoPtr & clientTransportAutoPtr,
        StubEntryPtr stubEntryPtr,
        bool keepClientConnection)
    {
        AsioSessionStatePtr sessionStatePtr(createSessionState());
        SessionPtr sessionPtr(sessionStatePtr->getSessionPtr());
        sessionPtr->setIsCallbackSession(true);

        sessionStatePtr->implTransferNativeFrom(*clientTransportAutoPtr);

        if (stubEntryPtr)
        {
            sessionPtr->setDefaultStubEntryPtr(stubEntryPtr);
        }

        clientTransportAutoPtr.reset();
        if (keepClientConnection)
        {
            clientTransportAutoPtr.reset( createClientTransport(sessionPtr).release() );
        }

        sessionStatePtr->mState = AsioSessionState::WritingData;
        sessionStatePtr->onAppReadWriteCompleted(0);
        return sessionPtr;
    }

    ClientTransportAutoPtr AsioServerTransport::createClientTransport(
        SessionPtr sessionPtr)
    {
        AsioSessionState & sessionState = 
            dynamic_cast<AsioSessionState &>(sessionPtr->getSessionState());

        AsioSessionStatePtr sessionStatePtr = sessionState.sharedFromThis();

        ClientTransportAutoPtr clientTransportPtr =
            sessionStatePtr->implCreateClientTransport();

        ConnectionOrientedClientTransport & coClientTransport =
            static_cast<ConnectionOrientedClientTransport &>(
                *clientTransportPtr);

        coClientTransport.setRcfSession(sessionState.mSessionPtr);

        sessionState.mSocketOpsMutexPtr.reset( new Mutex() );
        coClientTransport.setSocketOpsMutex(sessionState.mSocketOpsMutexPtr);

        return clientTransportPtr;
    }

    bool AsioServerTransport::reflect(
        const SessionPtr &sessionPtr1, 
        const SessionPtr &sessionPtr2)
    {
        AsioSessionState & sessionState1 = 
            dynamic_cast<AsioSessionState &>(sessionPtr1->getSessionState());

        AsioSessionStatePtr sessionStatePtr1 = sessionState1.sharedFromThis();

        AsioSessionState & sessionState2 = 
            dynamic_cast<AsioSessionState &>(sessionPtr2->getSessionState());

        AsioSessionStatePtr sessionStatePtr2 = sessionState2.sharedFromThis();

        sessionStatePtr1->mReflecteeWeakPtr = sessionStatePtr2;
        sessionStatePtr2->mReflecteeWeakPtr = sessionStatePtr1;

        sessionStatePtr1->mReflecting = true;
        sessionStatePtr2->mReflecting = true;

        return true;
    }

    // I_Service implementation

    void AsioServerTransport::open()
    {
        mStopFlag = false;
        implOpen();
    }


    void AsioSessionState::close()
    {
        Lock lock(mSessionPtr->mDisableIoMutex);
        if (!mSessionPtr->mDisableIo)
        {
            implClose();
            mSessionPtr->mDisableIo = true;
        }
    }

    AsioErrorCode AsioSessionState::getLastError()
    {
        return mLastError;
    }

    void AsioServerTransport::close()
    {
        mAcceptorPtr.reset();
        mStopFlag = true;
        cancelOutstandingIo();
        mpIoService->reset();
        std::size_t items = mpIoService->poll();
        while (items)
        {
            mpIoService->reset();
            items = mpIoService->poll();
        }

        mpIoService = NULL;
        mpServer = NULL;
    }

    void AsioServerTransport::stop()
    {
        mpIoService->stop();
    }

    void AsioServerTransport::onServiceAdded(RcfServer &server)
    {
        setServer(server);
        mTaskEntries.clear();
        mTaskEntries.push_back(TaskEntry(Mt_Asio));
    }

    void AsioServerTransport::onServiceRemoved(RcfServer &)
    {}

    void AsioServerTransport::onServerStart(RcfServer & server)
    {
        open();

        mStopFlag = false;
        mpServer  = &server;
        mpIoService = mTaskEntries[0].getThreadPool().getIoService();        
    }

    void AsioServerTransport::startAcceptingThread(Exception & eRet)
    {
        try
        {
            std::size_t initialNumberOfConnections = getInitialNumberOfConnections();
            for (std::size_t i=0; i<initialNumberOfConnections; ++i)
            {
                createSessionState()->beginAccept();
            }
        }
        catch(const Exception & e)
        {
            eRet = e;
        }
        catch(const std::exception & e)
        {
            eRet = Exception(e.what());
        }
        catch(...)
        {
            eRet = Exception("Unknown exception type caught in AsioServerTransport::startAcceptingThread().");
        }
    }

    void AsioServerTransport::startAccepting()
    {
        bool runningOnWindowsVistaOrLater = false;

#ifdef BOOST_WINDOWS
        OSVERSIONINFO osvi = {0};
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&osvi);

        if (osvi.dwMajorVersion >= 6)
        {
            runningOnWindowsVistaOrLater = true;
        }
#endif

        Exception e;
        if (runningOnWindowsVistaOrLater)
        {
            // http://stackoverflow.com/questions/12047960/getqueuedcompletionstatus-cant-dequeue-io-from-iocp-if-the-thread-which-origina

            // Due to a bug in Windows 8, we need to make the async accept calls
            // on a thread that won't subsequently block on I/O. If we do it on
            // this thread, and the user then calls e.g. std::cin.get() after
            // starting the server, none of the accepts will ever complete.

            // To simplify testing, we apply this workaround from Vista onwards. 
            // We can't apply it on earlier Windows (XP, 2003), because async 
            // calls are canceled when the issuing thread terminates.

            Thread t( boost::bind(&AsioServerTransport::startAcceptingThread, this, boost::ref(e)) );
            t.join();
        }
        else
        {
            startAcceptingThread(e);
        }

        if (e.bad())
        {
            e.throwSelf();
        }
    }

    void AsioServerTransport::onServerStop(RcfServer &)
    {
        close();
    }

    void AsioServerTransport::setServer(RcfServer &server)
    {
        mpServer = &server;
    }

    RcfServer & AsioServerTransport::getServer()
    {
        return *mpServer;
    }

    RcfServer & AsioServerTransport::getSessionManager()
    {
        return *mpServer;
    }

    AsioServerTransport::AsioServerTransport() :
        mpIoService(),
        mAcceptorPtr(),
        mWireProtocol(Wp_None),
        mStopFlag(),
        mpServer()
    {
    }

    AsioServerTransport::~AsioServerTransport()
    {
    }

    void AsioServerTransport::registerSession(AsioSessionStateWeakPtr session)
    {
        Lock lock(mSessionsMutex);
        mSessions.insert(session);
    }

    void AsioServerTransport::unregisterSession(AsioSessionStateWeakPtr session)
    {
        Lock lock(mSessionsMutex);
        mSessions.erase(session);
    }

    void AsioServerTransport::cancelOutstandingIo()
    {
        Lock lock(mSessionsMutex);

        std::set<SessionStateWeakPtr>::iterator iter;
        for (iter = mSessions.begin(); iter != mSessions.end(); ++iter)
        {
            SessionStatePtr sessionStatePtr = iter->lock();
            if (sessionStatePtr)
            {
                AsioSessionStatePtr asioSessionStatePtr = 
                    boost::static_pointer_cast<AsioSessionState>(sessionStatePtr);

                asioSessionStatePtr->close();
            }
        }
    }

    AsioAcceptor & AsioServerTransport::getAcceptor()
    {
        return *mAcceptorPtr;
    }

    AsioIoService & AsioServerTransport::getIoService()
    {
        return *mpIoService;
    }

} // namespace RCF

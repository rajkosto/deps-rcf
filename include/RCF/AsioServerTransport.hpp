
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

#ifndef INCLUDE_RCF_ASIOSERVERTRANSPORT_HPP
#define INCLUDE_RCF_ASIOSERVERTRANSPORT_HPP

#include <set>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <RCF/Asio.hpp>
#include <RCF/AsioBuffers.hpp>
#include <RCF/Enums.hpp>
#include <RCF/Export.hpp>
#include <RCF/IpAddress.hpp>
#include <RCF/IpServerTransport.hpp>
#include <RCF/ServerTransport.hpp>
#include <RCF/Service.hpp>
#include <RCF/ThreadLibrary.hpp>

namespace RCF {

    class RcfServer;
    class TcpClientTransport;
    class AsioSessionState;
    class AsioServerTransport;

    typedef boost::shared_ptr<AsioSessionState>         AsioSessionStatePtr;
    typedef boost::weak_ptr<AsioSessionState>           AsioSessionStateWeakPtr;

    class AsioAcceptor
    {
    public:
        virtual ~AsioAcceptor()
        {}
    };

    typedef boost::scoped_ptr<AsioAcceptor>           AsioAcceptorPtr;

    class RCF_EXPORT AsioServerTransport :
        public ServerTransport,
        public ServerTransportEx,
        public I_Service
    {
    private:

        // Needs to call open().
        friend class TcpAsioTransportFactory;

        friend class Win32NamedPipeSessionState;

        typedef boost::weak_ptr<I_Session>              SessionWeakPtr;

        AsioSessionStatePtr createSessionState();

        

        // I_ServerTransportEx implementation
        ClientTransportAutoPtr  
                            createClientTransport(
                                const Endpoint &endpoint);

        SessionPtr          createServerSession(
                                ClientTransportAutoPtr & clientTransportAutoPtr, 
                                StubEntryPtr stubEntryPtr,
                                bool keepClientConnection);

        ClientTransportAutoPtr  
                            createClientTransport(
                                SessionPtr sessionPtr);

        bool                reflect(
                                const SessionPtr &sessionPtr1, 
                                const SessionPtr &sessionPtr2);

        // I_Service implementation
        void                open();
        void                close();
        void                stop();

        void                onServiceAdded(     RcfServer & server);
        void                onServiceRemoved(   RcfServer & server);

    protected:

        void                onServerStart(      RcfServer & server);
        void                onServerStop(       RcfServer & server);
        void                setServer(          RcfServer & server);
        
        void                startAccepting();

    private:

        void                startAcceptingThread(Exception & eRet);

        RcfServer &         getServer();
        RcfServer &         getSessionManager();

    private:

        void                registerSession(AsioSessionStateWeakPtr session);
        void                unregisterSession(AsioSessionStateWeakPtr session);
        void                cancelOutstandingIo();

        friend class AsioSessionState;
        friend class FilterAdapter;
        friend class ServerTcpFrame;
        friend class ServerHttpFrame;

    protected:

        AsioServerTransport();
        ~AsioServerTransport();
        
        AsioIoService *                 mpIoService;
        AsioAcceptorPtr                 mAcceptorPtr;

        WireProtocol                    mWireProtocol;

    private:
        
        volatile bool                   mStopFlag;
        RcfServer *                     mpServer;

    private:

        virtual AsioSessionStatePtr     implCreateSessionState() = 0;
        virtual void                    implOpen() = 0;

        virtual ClientTransportAutoPtr  implCreateClientTransport(
                                            const Endpoint &endpoint) = 0;

    public:

        AsioAcceptor &                getAcceptor();

        AsioIoService &                 getIoService();
    };

    class ReadHandler
    {
    public:
        ReadHandler(AsioSessionStatePtr sessionStatePtr);
        void operator()(AsioErrorCode err, std::size_t bytes);
        void * allocate(std::size_t size);
        AsioSessionStatePtr mSessionStatePtr;
    };

    class WriteHandler
    {
    public:
        WriteHandler(AsioSessionStatePtr sessionStatePtr);
        void operator()(AsioErrorCode err, std::size_t bytes);
        void * allocate(std::size_t size);
        AsioSessionStatePtr mSessionStatePtr;
    };

    void *  asio_handler_allocate(std::size_t size, ReadHandler * pHandler);
    void    asio_handler_deallocate(void * pointer, std::size_t size, ReadHandler * pHandler);
    void *  asio_handler_allocate(std::size_t size, WriteHandler * pHandler);
    void    asio_handler_deallocate(void * pointer, std::size_t size, WriteHandler * pHandler);

    class RCF_EXPORT AsioSessionState :
        public SessionState,
        boost::noncopyable
    {
    public:

        friend class ReadHandler;
        friend class WriteHandler;
        friend class ServerTcpFrame;
        friend class ServerHttpFrame;


        typedef boost::weak_ptr<AsioSessionState>       AsioSessionStateWeakPtr;
        typedef boost::shared_ptr<AsioSessionState>     AsioSessionStatePtr;

        AsioSessionState(
            AsioServerTransport &transport,
            AsioIoService & ioService);

        virtual ~AsioSessionState();

        AsioSessionStatePtr sharedFromThis();

        void            close();

        AsioErrorCode   getLastError();

    protected:
        AsioIoService &         mIoService;

        std::vector<char>       mReadHandlerBuffer;
        std::vector<char>       mWriteHandlerBuffer;

        AsioErrorCode           mLastError;

    private:

        // read()/write() are used to connect with the filter sequence.
        void            read(
                            const ByteBuffer &byteBuffer, 
                            std::size_t bytesRequested);

        void            write(
                            const std::vector<ByteBuffer> &byteBuffers);

        void            setTransportFilters(
                            const std::vector<FilterPtr> &filters);

        void            getTransportFilters(
                            std::vector<FilterPtr> &filters);

        void            beginAccept();
        void            beginRead();
        void            beginWrite();

        void            onAcceptCompleted(const AsioErrorCode & error);

        void            onNetworkReadCompleted(
                            AsioErrorCode error, 
                            size_t bytesTransferred);

        void            onNetworkWriteCompleted(
                            AsioErrorCode error, 
                            size_t bytesTransferred);

        void            onAppReadWriteCompleted(
                            size_t bytesTransferred);

        void            onReflectedReadWriteCompleted(
                            const AsioErrorCode & error, 
                            size_t bytesTransferred);

        void            sendServerError(int error);

        void            doCustomFraming(size_t bytesTransferred);
        void            doRegularFraming(size_t bytesTransferred);

        // TODO: too many friends
        friend class    AsioServerTransport;
        friend class    TcpAsioSessionState;
        friend class    UnixLocalSessionState;
        friend class    Win32NamedPipeSessionState;
        friend class    FilterAdapter;

        enum State
        {
            Ready,
            Accepting,
            ReadingData,
            ReadingDataCount,
            WritingData
        };

        State                       mState;
        bool                        mIssueZeroByteRead;
        std::size_t                 mReadBufferRemaining;
        std::size_t                 mWriteBufferRemaining;

        std::vector<FilterPtr>      mTransportFilters;
        std::vector<FilterPtr>      mWireFilters;        

        AsioServerTransport &       mTransport;

        std::vector<ByteBuffer>     mWriteByteBuffers;
        std::vector<ByteBuffer>     mSlicedWriteByteBuffers;

        ReallocBufferPtr            mAppReadBufferPtr;
        ByteBuffer                  mAppReadByteBuffer;
        
        ReallocBufferPtr            mNetworkReadBufferPtr;
        ByteBuffer                  mNetworkReadByteBuffer;

        // So we can connect our read()/write() functions to the filter sequence.
        FilterPtr                   mFilterAdapterPtr;

        bool                        mCloseAfterWrite;
        AsioSessionStateWeakPtr     mReflecteeWeakPtr;
        AsioSessionStatePtr         mReflecteePtr;
        bool                        mReflecting;

        AsioSessionStateWeakPtr     mWeakThisPtr;

        AsioBuffers                 mBufs;

        boost::shared_ptr<Mutex>    mSocketOpsMutexPtr;

        // I_SessionState

    private:
        
        void                    postRead();
        ByteBuffer              getReadByteBuffer();
        void                    postWrite(std::vector<ByteBuffer> &byteBuffers);
        void                    postClose();
        ServerTransport &     getServerTransport();
        const RemoteAddress & getRemoteAddress();
        bool                    isConnected();

    private:

        virtual const RemoteAddress & implGetRemoteAddress() = 0;
        virtual void implRead(char * buffer, std::size_t bufferLen) = 0;
        virtual void implWrite(const std::vector<ByteBuffer> & buffers) = 0;
        virtual void implWrite(AsioSessionState &toBeNotified, const char * buffer, std::size_t bufferLen) = 0;
        virtual void implAccept() = 0;
        virtual bool implOnAccept() = 0;
        virtual bool implIsConnected() = 0;
        virtual void implClose() = 0;
        virtual void implCloseAfterWrite() {}
        virtual void implTransferNativeFrom(ClientTransport & clientTransport) = 0;
        virtual ClientTransportAutoPtr implCreateClientTransport() = 0;
    };

} // namespace RCF


#endif // ! INCLUDE_RCF_ASIOSERVERTRANSPORT_HPP

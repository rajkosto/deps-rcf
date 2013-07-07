
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

#include <RCF/UnixLocalServerTransport.hpp>

#include <RCF/Asio.hpp>
#include <RCF/UnixLocalClientTransport.hpp>
#include <RCF/UnixLocalEndpoint.hpp>

#include <RCF/util/Platform/OS/BsdSockets.hpp>

// std::remove
#include <cstdio>

namespace RCF {

    using ASIO_NS::local::stream_protocol;

    class UnixLocalAcceptor : public AsioAcceptor
    {
    public:
        UnixLocalAcceptor(AsioIoService & ioService, const std::string & fileName) : 
            mFileName(fileName),
            mAcceptor(ioService, stream_protocol::endpoint(fileName))
            
        {}

        ~UnixLocalAcceptor()
        {
            mAcceptor.close();

            // Delete the underlying file as well.
            int ret = std::remove(mFileName.c_str());
            int err = Platform::OS::BsdSockets::GetLastError();

            if (ret != 0)
            {
                // Couldn't delete it, not a whole lot we can do about it.
                RCF_LOG_1()(mFileName)(err)(Platform::OS::GetErrorString(err)) 
                    << "Failed to delete underlying file of UNIX domain socket.";
            }
        }

        std::string mFileName;
        stream_protocol::acceptor mAcceptor;
    };

    typedef stream_protocol::socket                 UnixLocalSocket;
    typedef boost::shared_ptr<UnixLocalSocket>      UnixLocalSocketPtr;

    // UnixLocalSessionState

    UnixLocalSessionState::UnixLocalSessionState(
        UnixLocalServerTransport & transport,
        AsioIoService & ioService) :
            AsioSessionState(transport, ioService),
            mSocketPtr(new UnixLocalSocket(ioService))
    {}

    const RemoteAddress & UnixLocalSessionState::implGetRemoteAddress()
    {
        return mRemoteAddress;
    }

    void UnixLocalSessionState::implRead(char * buffer, std::size_t bufferLen)
    {
        RCF_LOG_4()(bufferLen) 
            << "UnixLocalSessionState - calling async_read_some().";

        mSocketPtr->async_read_some(
            ASIO_NS::buffer( buffer, bufferLen),
            ReadHandler(sharedFromThis()));
    }

    void UnixLocalSessionState::implWrite(const std::vector<ByteBuffer> & buffers)
    {
        RCF_LOG_4()(RCF::lengthByteBuffers(buffers))
            << "UnixLocalSessionState - calling async_write_some().";

        mBufs.mVecPtr->resize(0);
        for (std::size_t i=0; i<buffers.size(); ++i)
        {
            ByteBuffer buffer = buffers[i];

            mBufs.mVecPtr->push_back( 
                AsioConstBuffer(buffer.getPtr(), buffer.getLength()) );
        }

        mSocketPtr->async_write_some(
            mBufs,
            WriteHandler(sharedFromThis()));
    }

    void UnixLocalSessionState::implWrite(
        AsioSessionState &toBeNotified, 
        const char * buffer, 
        std::size_t bufferLen)
    {
        ASIO_NS::async_write(
            *mSocketPtr,
            ASIO_NS::buffer(buffer, bufferLen),
            WriteHandler(toBeNotified.sharedFromThis()));
    }

    void UnixLocalSessionState::implAccept()
    {
        RCF_LOG_4()<< "UnixLocalSessionState - calling async_accept().";

        UnixLocalAcceptor & unixLocalAcceptor = 
            static_cast<UnixLocalAcceptor &>(mTransport.getAcceptor());

        unixLocalAcceptor.mAcceptor.async_accept(
            *mSocketPtr,
            boost::bind(
                &AsioSessionState::onAcceptCompleted,
                sharedFromThis(),
                ASIO_NS::placeholders::error));
    }

    bool UnixLocalSessionState::implOnAccept()
    {
        return true;
    }

    bool UnixLocalSessionState::implIsConnected()
    {
        int fd = mSocketPtr->native();
        return isFdConnected(fd);
    }

    void UnixLocalSessionState::implClose()
    {
        mSocketPtr->close();
    }

    void UnixLocalSessionState::implCloseAfterWrite()
    {
        int fd = mSocketPtr->native();
        const int BufferSize = 8*1024;
        char buffer[BufferSize];
        while (recv(fd, buffer, BufferSize, 0) > 0);
#ifdef BOOST_WINDOWS
        int ret = shutdown(fd, SD_BOTH);
#else
        int ret = shutdown(fd, SHUT_RDWR);
#endif
        RCF_UNUSED_VARIABLE(ret);

        postRead();
    }

    ClientTransportAutoPtr UnixLocalSessionState::implCreateClientTransport()
    {
        std::auto_ptr<UnixLocalClientTransport> unixLocalClientTransport(
            new UnixLocalClientTransport(mSocketPtr, mRemoteFileName));

        return ClientTransportAutoPtr(unixLocalClientTransport.release());
    }

    void UnixLocalSessionState::implTransferNativeFrom(ClientTransport & clientTransport)
    {
        UnixLocalClientTransport *pUnixLocalClientTransport =
            dynamic_cast<UnixLocalClientTransport *>(&clientTransport);

        if (pUnixLocalClientTransport == NULL)
        {
            Exception e("incompatible client transport");
            RCF_THROW(e)(typeid(clientTransport));
        }

        UnixLocalClientTransport & unixLocalClientTransport = *pUnixLocalClientTransport;
        unixLocalClientTransport.associateWithIoService(mIoService);
        mSocketPtr = unixLocalClientTransport.releaseLocalSocket();
        mRemoteFileName = unixLocalClientTransport.getPipeName();
    }

    int UnixLocalSessionState::getNativeHandle()
    {
        return static_cast<int>(mSocketPtr->native());
    }

    // UnixLocalServerTransport

    std::string UnixLocalServerTransport::getPipeName() const
    {
        return mFileName;
    }

    UnixLocalServerTransport::UnixLocalServerTransport(
        const std::string &fileName) :
            mFileName(fileName)
    {}

    TransportType UnixLocalServerTransport::getTransportType()
    {
        return Tt_UnixNamedPipe;
    }

    ServerTransportPtr UnixLocalServerTransport::clone()
    {
        return ServerTransportPtr(new UnixLocalServerTransport(mFileName));
    }

    AsioSessionStatePtr UnixLocalServerTransport::implCreateSessionState()
    {
        return AsioSessionStatePtr( new UnixLocalSessionState(*this, *mpIoService) );
    }

    void UnixLocalServerTransport::implOpen()
    {
    }

    ClientTransportAutoPtr UnixLocalServerTransport::implCreateClientTransport(
        const Endpoint &endpoint)
    {
        const UnixLocalEndpoint & unixLocalEndpoint = 
            dynamic_cast<const UnixLocalEndpoint &>(endpoint);

        ClientTransportAutoPtr clientTransportAutoPtr(
            new UnixLocalClientTransport(unixLocalEndpoint.getPipeName()));

        return clientTransportAutoPtr;
    }

    void UnixLocalServerTransport::onServerStart(RcfServer & server)
    {
        AsioServerTransport::onServerStart(server);

        mpIoService = mTaskEntries[0].getThreadPool().getIoService();

        RCF_ASSERT(mAcceptorPtr.get() == NULL);

        if ( !mFileName.empty() )
        {
            mAcceptorPtr.reset(
                new UnixLocalAcceptor(*mpIoService, mFileName));

            startAccepting();
        }

        RCF_LOG_2()(mFileName) << "UnixLocalServerTransport - listening on local socket.";
    }

    void UnixLocalServerTransport::onServerStop(RcfServer & server)
    {
        AsioServerTransport::onServerStop(server);
    }

} // namespace RCF


//******************************************************************************
// RCF - Remote Call Framework
//
// Copyright (c) 2005 - 2020, Delta V Software. All rights reserved.
// http://www.deltavsoft.com
//
// RCF is distributed under dual licenses - closed source or GPL.
// Consult your particular license for conditions of use.
//
// If you have not purchased a commercial license, you are using RCF 
// under GPL terms.
//
// Version: 3.2
// Contact: support <at> deltavsoft.com 
//
//******************************************************************************

#include <RCF/UdpServerTransport.hpp>

#include <RCF/MethodInvocation.hpp>
#include <RCF/RcfServer.hpp>
#include <RCF/RcfSession.hpp>
#include <RCF/ThreadLocalData.hpp>
#include <RCF/Tools.hpp>
#include <RCF/Log.hpp>

#include <RCF/BsdSockets.hpp>

namespace RCF {

    UdpServerTransport::UdpServerTransport(
        const IpAddress & ipAddress,
        const IpAddress & multicastIpAddress) :
            mpRcfServer(),
            mIpAddress(ipAddress),
            mMulticastIpAddress(multicastIpAddress),
            mFd(-1),
            mPollingDelayMs(),
            mEnableSharedAddressBinding()
    {
    }

    TransportType UdpServerTransport::getTransportType()
    {
        return Tt_Udp;
    }

    UdpServerTransport & UdpServerTransport::enableSharedAddressBinding()
    {
        mEnableSharedAddressBinding = true;
        return *this;
    }

    ServerTransportPtr UdpServerTransport::clone()
    {
        return ServerTransportPtr( new UdpServerTransport(
            mIpAddress, 
            mMulticastIpAddress) );
    }

    void UdpServerTransport::setSessionManager(RcfServer & rcfServer)
    {
        mpRcfServer = &rcfServer;
    }

    RcfServer & UdpServerTransport::getSessionManager()
    {
        RCF_ASSERT(mpRcfServer);
        return *mpRcfServer;
    }

    int UdpServerTransport::getPort() const
    {
        return mIpAddress.getPort();
    }

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4996 )  // warning C4996: 'ctime' was declared deprecated
#endif

    void UdpServerTransport::open()
    {
        RCF_LOG_4()(mIpAddress.string()) << "UdpServerTransport - creating server socket.";

        int mPort = mIpAddress.getPort();

        // create and bind a socket for receiving UDP messages
        if (mFd == -1 && mPort >= 0)
        {
            int ret = 0;
            int err = 0;

            mIpAddress.resolve();
            mFd = mIpAddress.createSocket(SOCK_DGRAM, IPPROTO_UDP);

            // enable reception of broadcast messages
            int enable = 1;
            ret = setsockopt(mFd, SOL_SOCKET, SO_BROADCAST, (char *) &enable, sizeof(enable));
            err = Platform::OS::BsdSockets::GetLastError();
            if (ret)
            {
                RCF_LOG_1()(ret)(err) << "setsockopt() - failed to set SO_BROADCAST on listening udp socket.";
            }

            // Share the address binding, if appropriate.
            if (mEnableSharedAddressBinding)
            {
                enable = 1;

                // Set SO_REUSEADDR socket option.
                ret = setsockopt(mFd, SOL_SOCKET, SO_REUSEADDR, (char *) &enable, sizeof(enable));
                err = Platform::OS::BsdSockets::GetLastError();
                if (ret)
                {
                    RCF_LOG_1()(ret)(err) << "setsockopt() - failed to set SO_REUSEADDR on listening udp multicast socket.";
                }

                // On OS X and BSD variants, need to set SO_REUSEPORT as well.

#if (defined(__MACH__) && defined(__APPLE__)) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) 

                ret = setsockopt(mFd, SOL_SOCKET, SO_REUSEPORT, (char *) &enable, sizeof(enable));
                err = Platform::OS::BsdSockets::GetLastError();
                if (ret)
                {
                    RCF_LOG_1()(ret)(err) << "setsockopt() - failed to set SO_REUSEPORT on listening udp multicast socket.";
                }
#endif

            }
            
            sockaddr * pServerAddr = NULL;
            Platform::OS::BsdSockets::socklen_t serverAddrSize = 0;

#if defined(__MACH__) && defined(__APPLE__)
            // On OS X, need to bind to all interfaces, before subscribing to a particular multicast group.
            std::string strIp = (mIpAddress.getType() == IpAddress::V4) ? "0.0.0.0" : "::0";
            IpAddress bindToAllInt(strIp, mIpAddress.getPort());
            bindToAllInt.resolve();
            bindToAllInt.getSockAddr(pServerAddr, serverAddrSize);
#else
            mIpAddress.getSockAddr(pServerAddr, serverAddrSize);
#endif

            // bind the socket
            ret = ::bind(mFd, pServerAddr, serverAddrSize);
            if (ret < 0)
            {
                err = Platform::OS::BsdSockets::GetLastError();
                Exception e(RcfError_Socket, "bind()", osError(err));
                RCF_THROW(e);
            }
            RCF_ASSERT( mFd != -1 );

            if (!mMulticastIpAddress.empty())
            {
                // set socket option for receiving multicast messages

                mMulticastIpAddress.resolve();

                std::string ip = mMulticastIpAddress.getIp();

                sockaddr * pAddr = NULL;
                Platform::OS::BsdSockets::socklen_t addrSize = 0;
                mMulticastIpAddress.getSockAddr(pAddr, addrSize);

                if ( mIpAddress.getType() == IpAddress::V4 )
                {
                    sockaddr_in * pAddrV4 = (sockaddr_in *)pAddr;
                    
                    ip_mreq imr;
                    memset(&imr, 0, sizeof(imr));

                    memcpy(
                        &imr.imr_multiaddr,
                        &pAddrV4->sin_addr,
                        sizeof(imr.imr_multiaddr));

                     if (mIpAddress.getIp().compare("0.0.0.0") == 0 )
                     {
                         imr.imr_interface.s_addr = INADDR_ANY;
                     }
                     else
                     {
                         imr.imr_interface.s_addr = inet_addr(mIpAddress.getIp().c_str());
                     }


                    ret = setsockopt(mFd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&imr, sizeof(imr));
                    err = Platform::OS::BsdSockets::GetLastError();

                    RCF_VERIFY(
                        ret == 0,
                        Exception(RcfError_Socket, "setsockopt() with IPPROTO_IP/IP_ADD_MEMBERSHIP", osError(err)));
                }
#if RCF_FEATURE_IPV6==1
                else if ( mIpAddress.getType() == IpAddress::V6 )
                {
                    SockAddrIn6 * pAddrV6 = (SockAddrIn6 *) pAddr;
                    
                    ipv6_mreq imr = { 0 };
                    memset(&imr, 0, sizeof(imr));

                    memcpy(
                        &imr.ipv6mr_multiaddr,
                        &pAddrV6->sin6_addr,
                        sizeof(imr.ipv6mr_multiaddr));

                     // Specifying source IP address with interface index.
                     sockaddr * pSockAddrSrc = NULL;
                     Platform::OS::BsdSockets::socklen_t sockAddrSrcSize = 0;
                     mIpAddress.getSockAddr(pSockAddrSrc, sockAddrSrcSize);
                     SockAddrIn6 * pAddrV6Src = (SockAddrIn6 *) pSockAddrSrc;
                     
                     imr.ipv6mr_interface = pAddrV6Src->sin6_scope_id;


                    ret = setsockopt(mFd, IPPROTO_IPV6, IP_ADD_MEMBERSHIP, (const char*)&imr, sizeof(imr));
                    err = Platform::OS::BsdSockets::GetLastError();

                    RCF_VERIFY(
                        ret == 0,
                        Exception(RcfError_Socket, "setsockopt() with IPPROTO_IPV6/IP_ADD_MEMBERSHIP", osError(err)));
                }
#endif
                // TODO: enable source-filtered multicast messages
                //ip_mreq_source imr;
                //imr.imr_multiaddr.s_addr = inet_addr("232.5.6.7");
                //imr.imr_sourceaddr.s_addr = INADDR_ANY;//inet_addr("10.1.1.2");
                //imr.imr_interface.s_addr = INADDR_ANY;
                //int ret = setsockopt(mFd,IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (const char*) &imr, sizeof(imr));
                //int err = Platform::OS::BsdSockets::GetLastError();
            }

            // set the socket to nonblocking mode
            Platform::OS::BsdSockets::setblocking(mFd, false);

            // retrieve the port number, if it's generated by the system
            if (mPort == 0)
            {
                IpAddress ip(mFd, mIpAddress.getType());
                mPort = ip.getPort();
                mIpAddress.setPort(mPort);
            }

            RCF_LOG_2() << "UdpServerTransport - listening on port " << mPort << ".";
        }
    }

#ifdef _MSC_VER
#pragma warning( pop )
#endif

    void UdpServerTransport::close()
    {
        if (mFd != -1)
        {
            int ret = Platform::OS::BsdSockets::closesocket(mFd);
            int err = Platform::OS::BsdSockets::GetLastError();
            mFd = -1;
            RCF_VERIFY(
                ret == 0,
                Exception(RcfError_SocketClose, osError(err)));
        }
    }

    void discardPacket(int fd)
    {
        char buffer[1];
        int len = recvfrom(fd, buffer, 1, 0, NULL, NULL);
        int err = Platform::OS::BsdSockets::GetLastError();
        RCF_VERIFY(
            len == 1 ||
            (len == -1 && err == Platform::OS::BsdSockets::ERR_EMSGSIZE) ||
            (len == -1 && err == Platform::OS::BsdSockets::ERR_ECONNRESET),
            Exception(RcfError_Socket, "recvfrom()", osError(err)));
    }

    void UdpServerTransport::cycle(int timeoutMs)
    {
        RCF::ThreadInfoPtr tiPtr = getTlsThreadInfoPtr();
        RCF::ThreadPool & threadPool = tiPtr->getThreadPool();

        if (threadPool.shouldStop())
        {
            return;
        }

        // poll the UDP socket for messages, and read a message if one is available

        fd_set fdSet;
        FD_ZERO(&fdSet);
        FD_SET( static_cast<SOCKET>(mFd), &fdSet);
        timeval timeout;
        timeout.tv_sec = timeoutMs/1000;
        timeout.tv_usec = 1000*(timeoutMs%1000);

        int ret = Platform::OS::BsdSockets::select(
            mFd+1,
            &fdSet,
            NULL,
            NULL,
            timeoutMs < 0 ? NULL : &timeout);

        int err = Platform::OS::BsdSockets::GetLastError();
        if (ret == 1)
        {
            NetworkSessionPtr networkSessionPtr = getTlsUdpNetworkSessionPtr();
            if (networkSessionPtr.get() == NULL)
            {
                networkSessionPtr = NetworkSessionPtr(new NetworkSession(*this));
                networkSessionPtr->mRcfSessionPtr = getSessionManager().createSession();
                networkSessionPtr->mRcfSessionPtr->setNetworkSession(*networkSessionPtr);
                setTlsUdpNetworkSessionPtr(networkSessionPtr);
            }

            tryReadMessage(networkSessionPtr);
        }
        else if (ret == 0)
        {
            //RCF_LOG_4()(mFd)(mPort)(timeoutMs) << "UdpServerTransport - no messages received within polling interval.";
        }
        else if (ret == -1)
        {
            Exception e(RcfError_Socket, "select()", osError(err));
            RCF_THROW(e);
        }

    }

    void UdpNetworkSession::postWrite(
        std::vector<ByteBuffer> &byteBuffers)
    {
        // prepend data length and send the data

        ReallocBufferPtr &writeVecPtr = mWriteVecPtr;

        if (writeVecPtr.get() == NULL || writeVecPtr.use_count() != 1)
        {
            writeVecPtr.reset( new ReallocBuffer());
        }

        ReallocBuffer &writeBuffer = *writeVecPtr;

        unsigned int dataLength = static_cast<unsigned int>(
            lengthByteBuffers(byteBuffers));

        writeBuffer.resize(4+dataLength);
        memcpy( &writeBuffer[0], &dataLength, 4);
        machineToNetworkOrder(&writeBuffer[0], 4, 1);
        copyByteBuffers(byteBuffers, &writeBuffer[4]);
        byteBuffers.resize(0);

        sockaddr * pRemoteAddr = NULL;
        Platform::OS::BsdSockets::socklen_t remoteAddrSize = 0;
        mRemoteAddress.getSockAddr(pRemoteAddr, remoteAddrSize);
       
        int len = sendto(
            mTransport.mFd,
            &writeBuffer[0],
            static_cast<int>(writeBuffer.size()),
            0,
            pRemoteAddr,
            remoteAddrSize);

        if (len != static_cast<int>(writeBuffer.size()))
        {
            int err = Platform::OS::BsdSockets::GetLastError();
            Exception e(RcfError_Socket, "sendto()", osError(err));
            RCF_THROW(e);
        }

        NetworkSessionPtr networkSessionPtr = getTlsUdpNetworkSessionPtr();

        SessionPtr sessionPtr = networkSessionPtr->mRcfSessionPtr;
    }

    void UdpNetworkSession::postRead()
    {
    }

    void UdpServerTransport::cycleTransportAndServer(int timeoutMs)
    {
        cycle(timeoutMs);
    }

    void UdpServerTransport::onServiceAdded(RcfServer &server)
    {
        setSessionManager(server);

        mTaskEntries.clear();

        mTaskEntries.push_back(
            TaskEntry(
                std::bind(
                    &UdpServerTransport::cycleTransportAndServer,
                    this,
                    std::placeholders::_1),
                StopFunctor(),
                "RCF UDP server"));
    }

    void UdpServerTransport::onServiceRemoved(RcfServer &)
    {}

    UdpNetworkSession::UdpNetworkSession(UdpServerTransport & transport) :
        mTransport(transport)
    {}

    void UdpServerTransport::onServerStart(RcfServer &)
    {
        open();
    }

    void UdpServerTransport::onServerStop(RcfServer &)
    {
        close();
    }

    void UdpServerTransport::tryReadMessage(NetworkSessionPtr networkSessionPtr)
    {
        // Try to read a message from the UDP socket.

        int err = 0;

        ReallocBufferPtr &readVecPtr =
        networkSessionPtr->mReadVecPtr;

        if (readVecPtr.get() == NULL || readVecPtr.use_count() != 1)
        {
            readVecPtr.reset(new ReallocBuffer());
        }
        ReallocBuffer &buffer = *readVecPtr;

        SockAddrStorage from;
        int fromlen = sizeof(from);
        memset(&from, 0, sizeof(from));
        buffer.resize(4);

        // Peek at the first 4 bytes to see how long the message is.
        int len = Platform::OS::BsdSockets::recvfrom(
            mFd,
            &buffer[0],
            4,
            MSG_PEEK,
            (sockaddr *)&from,
            &fromlen);

        err = 0;
        if (len < 0)
        {
            err = Platform::OS::BsdSockets::GetLastError();
        }
        if (err != Platform::OS::BsdSockets::ERR_EWOULDBLOCK)
        {
            networkSessionPtr->mRemoteAddress.init(
                (sockaddr&)from,
                fromlen,
                mIpAddress.getType());

            if (!isIpAllowed(networkSessionPtr->mRemoteAddress))
            {
                RCF_LOG_2()(networkSessionPtr->mRemoteAddress.getIp())
                    << "Client IP does not match server's IP access rules. Closing connection.";

                discardPacket(mFd);
            }
            else if (len == 4
                || (len == -1 && err == Platform::OS::BsdSockets::ERR_EMSGSIZE))
            {
                unsigned int dataLength = 0;
                memcpy(&dataLength, &buffer[0], 4);
                networkToMachineOrder(&dataLength, 4, 1);

                if ( getMaxIncomingMessageLength() && dataLength > getMaxIncomingMessageLength() )
                {
                    // Message too long - send an error message back.

                    ByteBuffer byteBuffer;
                    encodeServerError(getSessionManager(), byteBuffer, RcfError_ServerMessageLength_Id);
                    byteBuffer.expandIntoLeftMargin(4);

                    *(std::uint32_t *) (byteBuffer.getPtr()) =
                        static_cast<std::uint32_t>(byteBuffer.getLength() - 4);

                    RCF::machineToNetworkOrder(byteBuffer.getPtr(), 4, 1);

                    char * szBuffer = byteBuffer.getPtr();
                    std::size_t bufferLen = byteBuffer.getLength();

                    sockaddr * pRemoteAddr = NULL;
                    Platform::OS::BsdSockets::socklen_t remoteAddrSize = 0;
                    networkSessionPtr->mRemoteAddress.getSockAddr(pRemoteAddr, remoteAddrSize);

                    len = sendto(
                        mFd,
                        szBuffer,
                        static_cast<int>(bufferLen),
                        0,
                        pRemoteAddr,
                        remoteAddrSize);

                    RCF_UNUSED_VARIABLE(len);
                    discardPacket(mFd);
                }
                else
                {
                    // Read the message and pass it on to RcfServer.

                    buffer.resize(4 + dataLength);
                    memset(&from, 0, sizeof(from));
                    fromlen = sizeof(from);

                    len = Platform::OS::BsdSockets::recvfrom(
                        mFd,
                        &buffer[0],
                        4 + dataLength,
                        0,
                        (sockaddr *)&from,
                        &fromlen);

                    if (static_cast<unsigned int>(len) == 4 + dataLength)
                    {
                        getSessionManager().onReadCompleted(networkSessionPtr->mRcfSessionPtr);
                    }
                }
            }
            else
            {
                discardPacket(mFd);
            }
        }
    }

    const RemoteAddress &UdpNetworkSession::getRemoteAddress() const
    {
        return mRemoteAddress;
    }

    ServerTransport & UdpNetworkSession::getServerTransport()
    {
        return mTransport;
    }

    const RemoteAddress & UdpNetworkSession::getRemoteAddress()
    {
        return mRemoteAddress;
    }

    void UdpNetworkSession::setTransportFilters(const std::vector<FilterPtr> &filters)
    {
        RCF_UNUSED_VARIABLE(filters);
        RCF_ASSERT_ALWAYS("");
    }

    void UdpNetworkSession::getTransportFilters(std::vector<FilterPtr> &filters)
    {
        RCF_UNUSED_VARIABLE(filters);
        RCF_ASSERT_ALWAYS("");
    }

    ByteBuffer UdpNetworkSession::getReadByteBuffer()
    {
        return ByteBuffer(
            &(*mReadVecPtr)[0] + 4,
            mReadVecPtr->size() - 4,
            4,
            mReadVecPtr);
    }

    void UdpNetworkSession::postClose()
    {
    }

    int UdpNetworkSession::getNativeHandle() const
    {
        return mTransport.mFd;
    }

    bool UdpNetworkSession::isConnected()
    {
        return true;
    }

} // namespace RCF


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

#ifndef INCLUDE_RCF_TEST_TRANSPORTFACTORIES_HPP
#define INCLUDE_RCF_TEST_TRANSPORTFACTORIES_HPP

#include <iostream>
#include <typeinfo>
#include <utility>
#include <vector>

#include <sys/stat.h>

#include <boost/config.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/version.hpp>

#include <RCF/ClientStub.hpp>
#include <RCF/InitDeinit.hpp>
#include <RCF/ThreadLibrary.hpp>

#include <RCF/TcpAsioServerTransport.hpp>
#include <RCF/Asio.hpp>
#include <RCF/Config.hpp>

#ifdef RCF_HAS_LOCAL_SOCKETS
#include <RCF/UnixLocalClientTransport.hpp>
#include <RCF/UnixLocalServerTransport.hpp>
#endif

#if RCF_FEATURE_NAMEDPIPE==1
#include <RCF/Win32NamedPipeClientTransport.hpp>
#include <RCF/Win32NamedPipeEndpoint.hpp>
#include <RCF/Win32NamedPipeServerTransport.hpp>
#endif

#include <RCF/TcpClientTransport.hpp>
#include <RCF/UdpClientTransport.hpp>
#include <RCF/UdpServerTransport.hpp>

#include <RCF/ObjectFactoryService.hpp>

template<typename Interface>
inline bool tryCreateRemoteObject(
    RCF::I_RcfClient &rcfClient,
    std::string objectName = "")
{
    try
    {
        rcfClient.getClientStub().createRemoteObject(objectName);
        return true;
    }
    catch (const RCF::Exception &e)
    {
        RCF_LOG_1()(e);
        return false;
    }
}


namespace RCF {

    typedef boost::shared_ptr<ClientTransportAutoPtr> ClientTransportAutoPtrPtr;

    typedef std::pair<ServerTransportPtr, ClientTransportAutoPtrPtr> TransportPair;

    class I_TransportFactory
    {
    public:
        virtual ~I_TransportFactory() {}
        virtual TransportPair createTransports() = 0;
        virtual TransportPair createNonListeningTransports() = 0;
        virtual bool isConnectionOriented() = 0;
        virtual bool supportsTransportFilters() = 0;
        virtual std::string desc() = 0;
    };

    typedef boost::shared_ptr<I_TransportFactory> TransportFactoryPtr;

    typedef std::vector<TransportFactoryPtr> TransportFactories;

    static TransportFactories &getTransportFactories()
    {
        static TransportFactories transportFactories;
        return transportFactories;
    }

    static TransportFactories &getIpTransportFactories()
    {
        static TransportFactories ipTransportFactories;
        return ipTransportFactories;
    }

    //**************************************************
    // transport factories

    static std::string loopBackV4 = "127.0.0.1";
    static std::string loopBackV6 = "::1";

#if RCF_FEATURE_NAMEDPIPE==1

    class Win32NamedPipeTransportFactory : public I_TransportFactory
    {
    public:
        TransportPair createTransports()
        {
            typedef boost::shared_ptr<Win32NamedPipeServerTransport> Win32NamedPipeServerTransportPtr;
            Win32NamedPipeServerTransportPtr serverTransportPtr(
                new Win32NamedPipeServerTransport(RCF_T("")));

            tstring pipeName = serverTransportPtr->getPipeName();

            ClientTransportAutoPtrPtr clientTransportAutoPtrPtr(
                new ClientTransportAutoPtr(
                    new Win32NamedPipeClientTransport(pipeName)));

            return std::make_pair(
                ServerTransportPtr(serverTransportPtr), 
                clientTransportAutoPtrPtr);

        }

        TransportPair createNonListeningTransports()
        {
            return std::make_pair(
                ServerTransportPtr( new Win32NamedPipeServerTransport( RCF_T("")) ),
                ClientTransportAutoPtrPtr());

        }

        bool isConnectionOriented()
        {
            return true;
        }

        bool supportsTransportFilters()
        {
            return true;
        }

        std::string desc()
        {
            return "Win32NamedPipeTransportFactory";
        }
    };

#endif

#if RCF_FEATURE_TCP==1

    class TcpAsioTransportFactory : public I_TransportFactory
    {
    public:
        
        TcpAsioTransportFactory(IpAddress::Type type = IpAddress::V4)
        {
            switch (type)
            {
            case IpAddress::V4: mLoopback = loopBackV4; break;
            case IpAddress::V6: mLoopback = loopBackV6; break;
            default: RCF_ASSERT(0);
            }
        }

        TransportPair createTransports()
        {
            typedef boost::shared_ptr<TcpAsioServerTransport> TcpAsioServerTransportPtr;
            TcpAsioServerTransportPtr tcpServerTransportPtr(
                new TcpAsioServerTransport( IpAddress(mLoopback, 0)));

            tcpServerTransportPtr->open();
            int port = tcpServerTransportPtr->getPort();

            ClientTransportAutoPtrPtr clientTransportAutoPtrPtr(
                new ClientTransportAutoPtr(
                    new TcpClientTransport( IpAddress(mLoopback, port))));

            return std::make_pair(
                ServerTransportPtr(tcpServerTransportPtr), 
                clientTransportAutoPtrPtr);
        }

        TransportPair createNonListeningTransports()
        {
            return std::make_pair(
                ServerTransportPtr( new TcpAsioServerTransport( IpAddress(mLoopback, 0)) ),
                ClientTransportAutoPtrPtr());
        }

        bool isConnectionOriented()
        {
            return true;
        }

        bool supportsTransportFilters()
        {
            return true;
        }

        std::string desc()
        {
            return "TcpAsioTransportFactory (" + mLoopback + ")";
        }

    private:

        std::string mLoopback;

    };

#endif

#if RCF_FEATURE_LOCALSOCKET==1

    class UnixLocalTransportFactory : public I_TransportFactory
    {
    public:

        UnixLocalTransportFactory() : mIndex(0)
        {
        }

    private:

        TransportPair createTransports()
        {
            std::string pipeName = generateNewPipeName();

            RCF_LOG_2()(pipeName) << "Creating unix local socket transport pair";

            return std::make_pair(
                ServerTransportPtr( new UnixLocalServerTransport(pipeName) ),
                ClientTransportAutoPtrPtr(
                    new ClientTransportAutoPtr(
                        new UnixLocalClientTransport(pipeName))));
        }

        TransportPair createNonListeningTransports()
        {
            return std::make_pair(
                ServerTransportPtr( new UnixLocalServerTransport("") ),
                ClientTransportAutoPtrPtr());
        }

        bool isConnectionOriented()
        {
            return true;
        }

        bool supportsTransportFilters()
        {
            return true;
        }

    private:

        bool fileExists(const std::string & path)
        {
            struct stat stFileInfo = {};
            int ret = stat(path.c_str(), &stFileInfo);
            return ret == 0;
        }

        std::string generateNewPipeName()
        {
            std::string tempDir = RCF::getRelativeTestDataPath();

            std::string candidate;

            while (candidate.empty() || fileExists(candidate))
            {
                std::ostringstream os;
                os 
                    << tempDir 
                    << "TestPipe_" 
                    << ++mIndex;

                candidate = os.str();
            }

            return candidate;
        }

        std::string desc()
        {
            return "UnixLocalTransportFactory";
        }

        int mIndex;

    };

#endif // RCF_HAS_LOCAL_SOCKETS

#if RCF_FEATURE_UDP==1

    class UdpTransportFactory : public I_TransportFactory
    {
    public:

        UdpTransportFactory(IpAddress::Type type = IpAddress::V4)
        {
            switch (type)
            {
            case IpAddress::V4: mLoopback = loopBackV4; break;
            case IpAddress::V6: mLoopback = loopBackV6; break;
            default: RCF_ASSERT(0);
            }
        }

        TransportPair createTransports()
        {
            typedef boost::shared_ptr<UdpServerTransport> UdpServerTransportPtr;
            UdpServerTransportPtr udpServerTransportPtr(
                new UdpServerTransport( IpAddress(mLoopback, 0) ));

            udpServerTransportPtr->open();
            int port = udpServerTransportPtr->getPort();

            ClientTransportAutoPtrPtr clientTransportAutoPtrPtr(
                new ClientTransportAutoPtr(
                    new UdpClientTransport( IpAddress(mLoopback, port) )));

            return std::make_pair(
                ServerTransportPtr(udpServerTransportPtr), 
                clientTransportAutoPtrPtr);
        }

        TransportPair createNonListeningTransports()
        {
            return std::make_pair(
                ServerTransportPtr( new UdpServerTransport( IpAddress(mLoopback, 0) ) ),
                ClientTransportAutoPtrPtr());
        }

        bool isConnectionOriented()
        {
            return false;
        }

        bool supportsTransportFilters()
        {
            return false;
        }

        std::string desc()
        {
            return "UdpTransportFactory (" + mLoopback + ")";
        }

    private:

        std::string mLoopback;
    };

#endif

    typedef TcpAsioTransportFactory TcpTransportFactory;

    void initializeTransportFactories()
    {

#if RCF_FEATURE_IPV6==1
        const bool compileTimeIpv6 = true;
        ExceptionPtr ePtr;
        IpAddress("::1").resolve(ePtr);
        const bool runTimeIpv6 = (ePtr.get() == NULL);
#else
        const bool compileTimeIpv6 = false;
        const bool runTimeIpv6 = false;
#endif

#if RCF_FEATURE_NAMEDPIPE==1

        getTransportFactories().push_back(
            TransportFactoryPtr( new Win32NamedPipeTransportFactory()));

#endif

#if RCF_FEATURE_TCP==1

        getTransportFactories().push_back(
            TransportFactoryPtr( new TcpAsioTransportFactory(IpAddress::V4)));

        getIpTransportFactories().push_back(
            TransportFactoryPtr( new TcpAsioTransportFactory(IpAddress::V4)));

        if (compileTimeIpv6 && runTimeIpv6)
        {
            getTransportFactories().push_back(
                TransportFactoryPtr( new TcpAsioTransportFactory(IpAddress::V6)));

            getIpTransportFactories().push_back(
                TransportFactoryPtr( new TcpAsioTransportFactory(IpAddress::V6)));
        }

#endif

#if RCF_FEATURE_LOCALSOCKET==1

        getTransportFactories().push_back(
            TransportFactoryPtr( new UnixLocalTransportFactory()));

#endif

#if RCF_FEATURE_UDP==1

        getTransportFactories().push_back(
            TransportFactoryPtr( new UdpTransportFactory(IpAddress::V4)));

        getIpTransportFactories().push_back(
            TransportFactoryPtr( new UdpTransportFactory(IpAddress::V4)));

        if (compileTimeIpv6 && runTimeIpv6)
        {
            getTransportFactories().push_back(
                TransportFactoryPtr( new UdpTransportFactory(IpAddress::V6)));

            getIpTransportFactories().push_back(
                TransportFactoryPtr( new UdpTransportFactory(IpAddress::V6)));
        }

#endif

    }
    
} // namespace RCF

#endif // ! INCLUDE_RCF_TEST_TRANSPORTFACTORIES_HPP

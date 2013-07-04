
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

#include <RCF/HttpsClientTransport.hpp>

#ifdef RCF_USE_OPENSSL
#include <RCF/OpenSslEncryptionFilter.hpp>
#endif

#if defined(BOOST_WINDOWS)
#include <RCF/Schannel.hpp>
#endif

#include <RCF/ClientStub.hpp>
#include <RCF/HttpConnectFilter.hpp>
#include <RCF/HttpFrameFilter.hpp>
#include <RCF/ThreadLocalData.hpp>

namespace RCF {

    HttpsClientTransport::HttpsClientTransport(const HttpsEndpoint & httpsEndpoint) : 
        TcpClientTransport(httpsEndpoint.getIp(), httpsEndpoint.getPort())
    {
        std::vector<FilterPtr> wireFilters;

        // HTTP framing.
        wireFilters.push_back( FilterPtr( new HttpFrameFilter(
            getRemoteAddr().getIp(), 
            getRemoteAddr().getPort())));

        // SSL.
        ClientStub * pClientStub = getTlsClientStubPtr();
        RCF_ASSERT(pClientStub);

        FilterPtr sslFilterPtr;

#if defined(BOOST_WINDOWS) && defined(RCF_USE_OPENSSL)

        if (pClientStub->getPreferSchannel())
        {
            sslFilterPtr.reset( new SchannelFilter(pClientStub) );
        }
        else
        {
            sslFilterPtr.reset( new OpenSslEncryptionFilter(pClientStub) );
        }

#elif defined(BOOST_WINDOWS)

        sslFilterPtr.reset( new SchannelFilter(pClientStub) );

#elif defined(RCF_USE_OPENSSL)

        sslFilterPtr.reset( new OpenSslEncryptionFilter(pClientStub) );

#endif

        if (!sslFilterPtr)
        {
            RCF_THROW( Exception(_RcfError_SslNotSupported()) );
        }

        wireFilters.push_back(sslFilterPtr);

        // HTTP CONNECT filter for passing through a proxy.
        wireFilters.push_back( FilterPtr( new HttpConnectFilter(
            getRemoteAddr().getIp(), 
            getRemoteAddr().getPort())));

        setWireFilters(wireFilters);
    }

    TransportType HttpsClientTransport::getTransportType()
    {
        return Tt_Https;
    }

} // namespace RCF

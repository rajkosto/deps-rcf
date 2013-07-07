
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

#include <RCF/HttpEndpoint.hpp>

#include <RCF/HttpsClientTransport.hpp>
#include <RCF/HttpsServerTransport.hpp>

namespace RCF {

    HttpsEndpoint::HttpsEndpoint(int port) : 
        TcpEndpoint(port)
    {
    }

    HttpsEndpoint::HttpsEndpoint(const std::string & ip, int port) : 
        TcpEndpoint(ip, port)
    {
    }

    std::string HttpsEndpoint::asString() const
    {
        std::ostringstream os;
        std::string ip = getIp();
        if (ip.empty())
        {
            ip = "127.0.0.1";
        }
        os << "HTTPS endpoint " << ip << ":" << getPort();
        return os.str();
    }

    ServerTransportAutoPtr HttpsEndpoint::createServerTransport() const
    {
        return ServerTransportAutoPtr( new HttpsServerTransport(*this) );
    }

    ClientTransportAutoPtr HttpsEndpoint::createClientTransport() const
    {
        return ClientTransportAutoPtr( new HttpsClientTransport(*this) );
    }

    EndpointPtr HttpsEndpoint::clone() const
    {
        return EndpointPtr( new HttpsEndpoint(*this) );
    }

} // namespace RCF


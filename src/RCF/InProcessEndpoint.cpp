
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

#include <RCF/InProcessEndpoint.hpp>
#include <RCF/InProcessTransport.hpp>

namespace RCF {

    InProcessEndpoint::InProcessEndpoint(RCF::RcfServer & server) :
        mServer(server),
        mpCallbackServer(NULL)
    {
    }

    InProcessEndpoint::InProcessEndpoint(
        RCF::RcfServer & server, 
        RCF::RcfServer & callbackServer) :
            mServer(server),
            mpCallbackServer(&callbackServer)
    {
    }

    std::auto_ptr<ServerTransport> InProcessEndpoint::createServerTransport() const
    {
        RCF_ASSERT(0);
        return std::auto_ptr<ServerTransport>();
    }

    std::auto_ptr<ClientTransport> InProcessEndpoint::createClientTransport() const
    {
        return std::auto_ptr<ClientTransport>( 
            new InProcessTransport(mServer, mpCallbackServer) );
    }

    EndpointPtr InProcessEndpoint::clone() const
    {
        if (mpCallbackServer)
        {
            return EndpointPtr( new InProcessEndpoint(mServer, *mpCallbackServer) );
        }
        else
        {
            return EndpointPtr( new InProcessEndpoint(mServer) );
        }
    }

    std::string InProcessEndpoint::asString() const
    {
        std::ostringstream os;
        os << "In-process endpoint. RcfServer: " << &mServer;
        std::string s = os.str();
        return s;
    }

} // namespace RCF

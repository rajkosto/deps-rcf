
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

#ifndef INCLUDE_RCF_INPROCESSENDPOINT_HPP
#define INCLUDE_RCF_INPROCESSENDPOINT_HPP

#include <RCF/Endpoint.hpp>
#include <RCF/Export.hpp>

namespace RCF {

    class RcfServer;

    class RCF_EXPORT InProcessEndpoint : public Endpoint
    {
    public:
        InProcessEndpoint(RCF::RcfServer & server);
        InProcessEndpoint(RCF::RcfServer & server, RCF::RcfServer & callbackServer);

        std::auto_ptr<ServerTransport>    createServerTransport() const;
        std::auto_ptr<ClientTransport>    createClientTransport() const;
        EndpointPtr                         clone() const;
        std::string                         asString() const;

    private:
        RcfServer & mServer;
        RcfServer * mpCallbackServer;
    };

} // namespace RCF

#endif // ! INCLUDE_RCF_INPROCESSENDPOINT_HPP

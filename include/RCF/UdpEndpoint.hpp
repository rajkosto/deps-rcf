
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

#ifndef INCLUDE_RCF_UDPENDPOINT_HPP
#define INCLUDE_RCF_UDPENDPOINT_HPP

#include <memory>
#include <string>

#include <boost/shared_ptr.hpp>

#include <RCF/Endpoint.hpp>
#include <RCF/Export.hpp>
#include <RCF/InitDeinit.hpp>
#include <RCF/IpAddress.hpp>
#include <RCF/SerializationProtocol.hpp>
#include <RCF/TypeTraits.hpp>

#ifdef RCF_USE_SF_SERIALIZATION
#include <SF/SfNew.hpp>
#endif

#include <SF/SerializeParent.hpp>

namespace RCF {

    class I_ServerTransport;
    class I_ClientTransport;

    class RCF_EXPORT UdpEndpoint : public I_Endpoint
    {
    public:

        UdpEndpoint();
        UdpEndpoint(int port);
        UdpEndpoint(const std::string &ip, int port);
        UdpEndpoint(const IpAddress & ipAddress);
        UdpEndpoint(const UdpEndpoint &rhs);
       
        std::auto_ptr<I_ServerTransport>    createServerTransport() const;
        std::auto_ptr<I_ClientTransport>    createClientTransport() const;
        EndpointPtr                         clone() const;
        std::string                         getIp() const;
        int                                 getPort() const;
        std::string                         asString() const;

        UdpEndpoint &       enableSharedAddressBinding(bool enable = true);
        UdpEndpoint &       listenOnMulticast(const IpAddress & multicastIp);
        UdpEndpoint &       listenOnMulticast(const std::string & multicastIp);
       
#ifdef RCF_USE_SF_SERIALIZATION

        void serialize(SF::Archive &ar);

#endif

    private:
        IpAddress           mIp;
        IpAddress           mMulticastIp;
        bool                mEnableSharedAddressBinding;
    };

} // namespace RCF

#endif // ! INCLUDE_RCF_UDPENDPOINT_HPP

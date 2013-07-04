
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

#include <RCF/UnixLocalEndpoint.hpp>

#include <RCF/InitDeinit.hpp>

#ifdef RCF_USE_SF_SERIALIZATION
#include <SF/Registry.hpp>
#endif

#include <RCF/UnixLocalServerTransport.hpp>
#include <RCF/UnixLocalClientTransport.hpp>

#include <RCF/Asio.hpp>

#ifndef RCF_HAS_LOCAL_SOCKETS
#error Unix domain sockets not supported by this version of Boost.Asio.
#endif

namespace RCF {

    UnixLocalEndpoint::UnixLocalEndpoint()
    {}

    UnixLocalEndpoint::UnixLocalEndpoint(const std::string & pipeName) :
            mPipeName(pipeName)
    {}

    ServerTransportAutoPtr UnixLocalEndpoint::createServerTransport() const
    {
        return ServerTransportAutoPtr(new UnixLocalServerTransport(mPipeName));
    }

    ClientTransportAutoPtr UnixLocalEndpoint::createClientTransport() const
    {            
        return ClientTransportAutoPtr(new UnixLocalClientTransport(mPipeName));
    }

    EndpointPtr UnixLocalEndpoint::clone() const
    {
        return EndpointPtr( new UnixLocalEndpoint(*this) );
    }

    std::string UnixLocalEndpoint::asString() const
    {
        std::ostringstream os;
        os << "Named pipe endpoint \"" << mPipeName << "\"";
        return os.str();
    }

#ifdef RCF_USE_SF_SERIALIZATION

    void UnixLocalEndpoint::serialize(SF::Archive &ar)
    {
        serializeParent( (I_Endpoint*) 0, ar, *this);
        ar & mPipeName;
    }

    void initUnixLocalEndpointSerialization()
    {
        SF::registerType( (UnixLocalEndpoint *) 0, "RCF::UnixLocalEndpoint");
        SF::registerBaseAndDerived( (I_Endpoint *) 0, (UnixLocalEndpoint *) 0);
    }

#endif

} // namespace RCF

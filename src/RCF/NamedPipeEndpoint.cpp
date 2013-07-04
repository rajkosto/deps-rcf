
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

#include <RCF/NamedPipeEndpoint.hpp>

#include <RCF/InitDeinit.hpp>
#include <RCF/util/Tchar.hpp>

#ifdef RCF_USE_SF_SERIALIZATION
#include <SF/Registry.hpp>
#endif

#if defined(BOOST_WINDOWS)
#include <RCF/Win32NamedPipeClientTransport.hpp>
#include <RCF/Win32NamedPipeServerTransport.hpp>
#else
#include <RCF/UnixLocalServerTransport.hpp>
#include <RCF/UnixLocalClientTransport.hpp>
#endif

namespace RCF {

    NamedPipeEndpoint::NamedPipeEndpoint()
    {}

    NamedPipeEndpoint::NamedPipeEndpoint(const tstring & pipeName) :
        mPipeName(pipeName)
    {}

#if defined(BOOST_WINDOWS)

    ServerTransportAutoPtr NamedPipeEndpoint::createServerTransport() const
    {
        return ServerTransportAutoPtr(
            new Win32NamedPipeServerTransport(mPipeName));
    }

    ClientTransportAutoPtr NamedPipeEndpoint::createClientTransport() const
    {            
        return ClientTransportAutoPtr(
            new Win32NamedPipeClientTransport(mPipeName));
    }

#else
        
    ServerTransportAutoPtr NamedPipeEndpoint::createServerTransport() const
    {
        return ServerTransportAutoPtr(new UnixLocalServerTransport(mPipeName));
    }

    ClientTransportAutoPtr NamedPipeEndpoint::createClientTransport() const
    {
        return ClientTransportAutoPtr(new UnixLocalClientTransport(mPipeName));
    }

#endif

    EndpointPtr NamedPipeEndpoint::clone() const
    {
        return EndpointPtr( new NamedPipeEndpoint(*this) );
    }

    std::string NamedPipeEndpoint::asString() const
    {
        std::ostringstream os;
        os << "Named pipe endpoint \"" << RCF::toAstring(mPipeName) << "\"";
        return os.str();
    }

#ifdef RCF_USE_SF_SERIALIZATION

    void NamedPipeEndpoint::serialize(SF::Archive & ar)
    {
        serializeParent( (I_Endpoint*) 0, ar, *this);
        ar & mPipeName;
    }

#endif

    void initNamedPipeEndpointSerialization()
    {
#ifdef RCF_USE_SF_SERIALIZATION
        SF::registerType( (NamedPipeEndpoint *) 0, "RCF::NamedPipeEndpoint");
        SF::registerBaseAndDerived( (I_Endpoint *) 0, (NamedPipeEndpoint *) 0);
#endif
    }

} // namespace RCF

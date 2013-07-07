
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

#include <RCF/CallbackConnectionService.hpp>

#include <RCF/Marshal.hpp>
#include <RCF/RcfServer.hpp>
#include <RCF/ServerInterfaces.hpp>

namespace RCF {

    CallbackConnectionService::CallbackConnectionService() : mpServer(NULL)
    {
    }

    void CallbackConnectionService::onServiceAdded(RcfServer & server)
    {
        server.bind<I_CreateCallbackConnection>(*this);
    }

    void CallbackConnectionService::onServiceRemoved(RcfServer & server)
    {
        server.unbind<I_CreateCallbackConnection>();
    }

    void CallbackConnectionService::onServerStart(RcfServer & server)
    {
        mOnCallbackConnectionCreated = server.getOnCallbackConnectionCreated();
    }

    void CallbackConnectionService::CreateCallbackConnection()
    {
        // TODO: regular error message.
        // ...
        RCF_ASSERT( mOnCallbackConnectionCreated );

        RCF::convertRcfSessionToRcfClient( mOnCallbackConnectionCreated );
    }

} // namespace RCF

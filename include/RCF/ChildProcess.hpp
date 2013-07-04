
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

#ifndef INCLUDE_RCF_CHILDPROCESS_HPP
#define INCLUDE_RCF_CHILDPROCESS_HPP

#include <RCF/Export.hpp>
#include <RCF/Idl.hpp>
#include <RCF/NamedPipeEndpoint.hpp>
#include <RCF/ServerInterfaces.hpp>
#include <RCF/Timer.hpp>
#include <RCF/Tools.hpp>

namespace RCF {

    class RCF_EXPORT ChildProcess
    {
    public:
        ChildProcess(const tstring & pipeName);
        ~ChildProcess();

        bool isStarted();
        bool shouldStop();
        void start();
        void Ping();
        void onParentDisconnect();

    protected:
        RCF::RcfServer mServer;

    private:
        bool mStarted;
        bool mStopped;
    };

    class RCF_EXPORT ChildProcessMonitor
    {
    public:

        ChildProcessMonitor(const tstring & pipeName);
        virtual ~ChildProcessMonitor();

        virtual void onDisconnect() = 0;
        virtual void onConnect() = 0;

        void start();
        void stop();

        bool isConnected() { return mConnected; }

        template<typename RcfClientT>
        void createClient(RcfClientT & client)
        {
            client = RcfClientT( RCF::NamedPipeEndpoint(mPipeName) );
        }

        tstring getPipeName() { return mPipeName; }

    private:

        void disconnect(bool notify = true);
        void onWaitCompleted();
        void onPingCompleted();

        tstring                     mPipeName;
        RcfClient<I_ParentToChild>  mParentToChild;
        bool                        mConnected;
        boost::uint32_t             mAsyncPingIntervalMs;
    };

} // namespace RCF

#endif // ! INCLUDE_RCF_CHILDPROCESS_HPP

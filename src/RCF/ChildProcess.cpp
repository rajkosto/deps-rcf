
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

#include <RCF/ChildProcess.hpp>

namespace RCF {

    // ChildProcess

    ChildProcess::ChildProcess(const tstring & pipeName) : 
        mServer(RCF::NamedPipeEndpoint(pipeName)), 
        mStarted(false), 
        mStopped(false)
    {
        mServer.bind<I_ParentToChild>(*this);
    }

    ChildProcess::~ChildProcess()
    {
    }

    bool ChildProcess::isStarted()
    {
        return mStarted && !mStopped;
    }

    bool ChildProcess::shouldStop()
    {
        return mStopped;
    }

    void ChildProcess::start()
    {
        mServer.start();

        Timer waitTimer;
        while ( !isStarted() && !shouldStop() && !waitTimer.elapsed(30*1000))
        {
            RCF::sleepMs(1000);
        }

        if (!isStarted() && !shouldStop())
        {
            RCF_THROW( Exception(
                RcfError_User, 
                "Parent process did not connect within the expected time interval."));
        }
    }

    void ChildProcess::Ping()
    {
        if (!mStopped && !mStarted)
        {
            // First ping received.
            mStarted = true;

            RCF::RcfSession& session = RCF::getTlsRcfSession();

            session.setOnDestroyCallback( boost::bind(
              &ChildProcess::onParentDisconnect, 
              this) );
        }

    }

    void ChildProcess::onParentDisconnect()
    {
        mStopped = true;
    }

    // ChildProcessMonitor

    ChildProcessMonitor::ChildProcessMonitor(
        const tstring & pipeName) :
            mPipeName(pipeName),
            mParentToChild( RCF::NamedPipeEndpoint(pipeName) ),
            mConnected(false),
            mAsyncPingIntervalMs(500)
    {
    }

    ChildProcessMonitor::~ChildProcessMonitor()
    {
        stop();
    }

    void ChildProcessMonitor::start()
    {
        stop();
        onDisconnect();
        onWaitCompleted();
    }

    void ChildProcessMonitor::stop()
    {
        disconnect(false);
    }

    void ChildProcessMonitor::disconnect(bool notify)
    {
        mParentToChild.getClientStub().disconnect();

        if (mConnected)
        {
            mConnected = false;
            if (notify)
            {
                onDisconnect();
            }
        }
    }

    void ChildProcessMonitor::onWaitCompleted()
    {
        mParentToChild.Ping( 
            RCF::AsyncTwoway( boost::bind(
                &ChildProcessMonitor::onPingCompleted,
                this)) );
    }

    void ChildProcessMonitor::onPingCompleted()
    {
        // Keep on pinging until we get a successful response.

        std::auto_ptr<RCF::Exception> ePtr(
            mParentToChild.getClientStub().getAsyncException());

        if (ePtr.get())
        {
            disconnect();

            // Wait a bit and then try again.
            mParentToChild.getClientStub().wait(
                boost::bind(
                &ChildProcessMonitor::onWaitCompleted,
                this),
                mAsyncPingIntervalMs);
        }
        else
        {
            if (!mConnected)
            {
                mConnected = true;
                onConnect();
            }
        }
    }

} // namespace RCF
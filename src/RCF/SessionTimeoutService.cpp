
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

#include <RCF/SessionTimeoutService.hpp>

#include <RCF/RcfServer.hpp>
#include <RCF/RcfSession.hpp>

namespace RCF {

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4355 ) // warning C4355: 'this' : used in base member initializer list
#endif

    SessionTimeoutService::SessionTimeoutService() : 
            mSessionTimeoutMs(0),
            mReapingIntervalMs(0),
            mpRcfServer(),
            mPeriodicTimer(*this, 0)
    {
    }

#ifdef _MSC_VER
#pragma warning( pop )
#endif

    void SessionTimeoutService::onServerStart(RcfServer &server)
    {
        mpRcfServer = & server;
        
        mSessionTimeoutMs = mpRcfServer->getSessionTimeoutMs();
        mReapingIntervalMs = mpRcfServer->getSessionHarvestingIntervalMs();

        if (mSessionTimeoutMs)
        {
            mPeriodicTimer.setIntervalMs(mReapingIntervalMs);
            mPeriodicTimer.start();
        }
    }

    void SessionTimeoutService::onServerStop(RcfServer &server)
    {
        RCF_UNUSED_VARIABLE(server);
        mpRcfServer = NULL;
        mPeriodicTimer.stop();
    }

    void SessionTimeoutService::onTimer()
    {
        mSessionsTemp.resize(0);

        mpRcfServer->enumerateSessions(std::back_inserter(mSessionsTemp));

        for (std::size_t i=0; i<mSessionsTemp.size(); ++i)
        {
            SessionStatePtr sessionStatePtr = mSessionsTemp[i].lock();
            if (sessionStatePtr)
            {
                RcfSessionPtr rcfSessionPtr = sessionStatePtr->getSessionPtr();
                if (rcfSessionPtr)
                {
                    boost::uint32_t lastTouched = rcfSessionPtr->getTouchTimestamp();
                    if (lastTouched)
                    {
                        RCF::Timer lastTouchedTimer( lastTouched );
                        if (lastTouchedTimer.elapsed(mSessionTimeoutMs))
                        {
                            rcfSessionPtr->disconnect();
                        }
                    }
                }
            }
        }
    }

} //namespace RCF

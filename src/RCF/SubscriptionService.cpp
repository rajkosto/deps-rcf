
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

#include <RCF/SubscriptionService.hpp>

#include <boost/bind.hpp>

#include <typeinfo>

#include <RCF/AsioFwd.hpp>
#include <RCF/AsioServerTransport.hpp>
#include <RCF/ClientStub.hpp>
#include <RCF/ClientTransport.hpp>
#include <RCF/RcfServer.hpp>
#include <RCF/RcfSession.hpp>
#include <RCF/ServerInterfaces.hpp>


namespace RCF {

    SubscriptionParms::SubscriptionParms() : mClientStub("")
    {
    }

    void SubscriptionParms::setTopicName(const std::string & publisherName)
    {
        mPublisherName = publisherName;
    }

    std::string SubscriptionParms::getTopicName() const
    {
        return mPublisherName;
    }

    void SubscriptionParms::setPublisherEndpoint(const Endpoint & publisherEp)
    {
        mClientStub.setEndpoint(publisherEp);
    }

    void SubscriptionParms::setPublisherEndpoint(I_RcfClient & rcfClient)
    {
        mClientStub = rcfClient.getClientStub();
        mClientStub.setTransport( rcfClient.getClientStub().releaseTransport() );
    }

    void SubscriptionParms::setOnSubscriptionDisconnect(OnSubscriptionDisconnect onSubscriptionDisconnect)
    {
        mOnDisconnect = onSubscriptionDisconnect;
    }

    void SubscriptionParms::setOnAsyncSubscribeCompleted(OnAsyncSubscribeCompleted onAsyncSubscribeCompleted)
    {
        mOnAsyncSubscribeCompleted = onAsyncSubscribeCompleted;
    }

    Subscription::~Subscription()
    {
        close();
    }

    void Subscription::setWeakThisPtr(SubscriptionWeakPtr thisWeakPtr)
    {
        mThisWeakPtr = thisWeakPtr;
    }

    bool Subscription::isConnected()
    {
        RecursiveLock lock(mMutex);
        return mConnectionPtr->getClientStub().isConnected();
    }

    unsigned int Subscription::getPingTimestamp()
    {
        RcfSessionPtr rcfSessionPtr;
        {
            RecursiveLock lock(mMutex);
            rcfSessionPtr = mRcfSessionWeakPtr.lock();
        }
        if (rcfSessionPtr)
        {
            return rcfSessionPtr->getPingTimestamp();
        }
        return 0;
    }

    void Subscription::close()
    {
        RCF_ASSERT(mThisWeakPtr != SubscriptionWeakPtr());

        {
            RecursiveLock lock(mMutex);

            if (mClosed)
            {
                return;
            }

            RcfSessionPtr rcfSessionPtr(mRcfSessionWeakPtr.lock());
            if (rcfSessionPtr)
            {

                // When this function returns, the caller is allowed to delete
                // the object that this subscription refers to. So we need to
                // make sure there are no calls in progress.

                Lock lock(rcfSessionPtr->mStopCallInProgressMutex);
                rcfSessionPtr->mStopCallInProgress = true;

                // Remove subscription binding.
                rcfSessionPtr->setDefaultStubEntryPtr(StubEntryPtr());

                // Clear the destroy callback.
                // TODO: how do we know that we're not clearing someone else's callback?
                rcfSessionPtr->setOnDestroyCallback(
                    RcfSession::OnDestroyCallback());
            }

            mRcfSessionWeakPtr.reset();
            mConnectionPtr->getClientStub().disconnect();
            mClosed = true;
        }

        mSubscriptionService.closeSubscription(mThisWeakPtr);
    }

    RcfSessionPtr Subscription::getRcfSessionPtr()
    {
        RecursiveLock lock(mMutex);
        return mRcfSessionWeakPtr.lock();
    }

    void Subscription::onDisconnect(SubscriptionWeakPtr subWeakPtr, RcfSession & session)
    {
        SubscriptionPtr subPtr = subWeakPtr.lock();
        if (subPtr)
        {
            OnSubscriptionDisconnect f = subPtr->mOnDisconnect;

            subPtr->close();

            if (f)
            {
                f(session);
            }
        }
    }

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4355 ) // warning C4355: 'this' : used in base member initializer list
#endif

    SubscriptionService::SubscriptionService(boost::uint32_t pingIntervalMs) :
        mpServer(),
        mPingIntervalMs(pingIntervalMs),
        mPeriodicTimer(*this, pingIntervalMs)
    {}

#ifdef _MSC_VER
#pragma warning( pop )
#endif

    SubscriptionService::~SubscriptionService()
    {
    }

    SubscriptionPtr SubscriptionService::onRequestSubscriptionCompleted(
        boost::int32_t                      ret,
        const std::string &                 publisherName,
        RcfClient<I_RequestSubscription> &  client,
        RcfClientPtr                        rcfClientPtr,
        OnSubscriptionDisconnect            onDisconnect,
        boost::uint32_t                     pubToSubPingIntervalMs,
        bool                                pingsEnabled)
    {
        bool ok = (ret == RcfError_Ok);
        if (ok)
        {
            ClientTransportAutoPtr clientTransportAutoPtr(
                client.getClientStub().releaseTransport());

            ServerTransport * pTransport = NULL;
            ServerTransportEx * pTransportEx = NULL;

            if (clientTransportAutoPtr->isInProcess())
            {
                pTransport = dynamic_cast<ServerTransport *>(
                    clientTransportAutoPtr.get());
            }
            else
            {
                pTransport = & mpServer->findTransportCompatibleWith(
                    *clientTransportAutoPtr);
            }

            pTransportEx = dynamic_cast<ServerTransportEx *>(pTransport);

            ServerTransportEx & serverTransportEx = * pTransportEx; 

            SessionPtr sessionPtr = serverTransportEx.createServerSession(
                clientTransportAutoPtr,
                StubEntryPtr(new StubEntry(rcfClientPtr)),
                true);

            RCF_ASSERT( sessionPtr );

            RcfSessionPtr rcfSessionPtr = sessionPtr;

            rcfSessionPtr->setUserData(client.getClientStub().getUserData());
            rcfSessionPtr->setPingTimestamp();

            std::string publisherUrl;
            EndpointPtr epPtr = client.getClientStub().getEndpoint();
            if (epPtr)
            {
                publisherUrl = epPtr->asString();
            }

            if (    !clientTransportAutoPtr->isInProcess() 
                &&  !clientTransportAutoPtr->isAssociatedWithIoService())
            {
                AsioServerTransport & asioTransport = dynamic_cast<AsioServerTransport &>(
                    mpServer->getServerTransport());

                clientTransportAutoPtr->associateWithIoService(asioTransport.getIoService());
            }

            SubscriptionPtr subscriptionPtr( new Subscription(
                *this,
                clientTransportAutoPtr, 
                rcfSessionPtr, 
                pubToSubPingIntervalMs, 
                publisherUrl,
                publisherName,
                onDisconnect));

            rcfSessionPtr->setOnDestroyCallback( boost::bind(
                &Subscription::onDisconnect,
                SubscriptionWeakPtr(subscriptionPtr),
                _1));

            subscriptionPtr->setWeakThisPtr(subscriptionPtr);

            subscriptionPtr->mPingsEnabled = pingsEnabled;

            Lock lock(mSubscriptionsMutex);
            mSubscriptions.insert(subscriptionPtr);

            return subscriptionPtr;                
        }
        return SubscriptionPtr();
    }

    SubscriptionPtr SubscriptionService::createSubscriptionImpl(
        RcfClientPtr rcfClientPtr, 
        const SubscriptionParms & parms,
        const std::string & defaultPublisherName)
    {
        if (parms.mOnAsyncSubscribeCompleted)
        {
            // Async code path.
            createSubscriptionImplBegin(rcfClientPtr, parms, defaultPublisherName);
            return SubscriptionPtr();
        }

        ClientStub & clientStub = const_cast<ClientStub &>(parms.mClientStub);
        OnSubscriptionDisconnect onDisconnect = parms.mOnDisconnect;
        std::string publisherName = parms.mPublisherName;
        if (publisherName.empty())
        {
            publisherName = defaultPublisherName;
        }

        RcfClient<I_RequestSubscription> client(clientStub);
        client.getClientStub().setTransport(clientStub.releaseTransport());
        boost::uint32_t subToPubPingIntervalMs = mPingIntervalMs;
        boost::uint32_t pubToSubPingIntervalMs = 0;

        bool pingsEnabled = true;

        boost::int32_t ret = 0;
        if (clientStub.getRuntimeVersion() < 8)
        {
            pingsEnabled = false;

            ret = client.RequestSubscription(
                Twoway, 
                publisherName);
        }
        else
        {
            ret = client.RequestSubscription(
                Twoway, 
                publisherName, 
                subToPubPingIntervalMs, 
                pubToSubPingIntervalMs);
        }

        SubscriptionPtr subscriptionPtr = onRequestSubscriptionCompleted(
            ret,
            publisherName,
            client,
            rcfClientPtr,
            onDisconnect,
            pubToSubPingIntervalMs,
            pingsEnabled);

        return subscriptionPtr;
    }

    void SubscriptionService::createSubscriptionImplEnd(
        Subscription::AsyncClientPtr    clientPtr,
        Future<boost::int32_t>          fRet,
        const std::string &             publisherName,
        RcfClientPtr                    rcfClientPtr,
        OnSubscriptionDisconnect        onDisconnect,
        OnAsyncSubscribeCompleted       onCompletion,
        Future<boost::uint32_t>         incomingPingIntervalMs,
        bool                            pingsEnabled)
    {
        SubscriptionPtr subscriptionPtr;

        ExceptionPtr exceptionPtr(
            clientPtr->getClientStub().getAsyncException().release());

        boost::int32_t ret = fRet;

        if (!exceptionPtr && ret != RcfError_Ok)
        {
            exceptionPtr.reset( new Exception( Error(ret) ) );
        }

        if (!exceptionPtr)
        {
            subscriptionPtr = onRequestSubscriptionCompleted(
                ret,
                publisherName,
                *clientPtr,
                rcfClientPtr,
                onDisconnect,
                incomingPingIntervalMs,
                pingsEnabled);
        }

        onCompletion(subscriptionPtr, exceptionPtr);
    }

    void SubscriptionService::createSubscriptionImplBegin(
        RcfClientPtr rcfClientPtr, 
        const SubscriptionParms & parms,
        const std::string & defaultPublisherName)
    {

        ClientStub & clientStub = const_cast<ClientStub &>(parms.mClientStub);
        OnSubscriptionDisconnect onDisconnect = parms.mOnDisconnect;
        std::string publisherName = parms.mPublisherName;
        OnAsyncSubscribeCompleted onCompletion = parms.mOnAsyncSubscribeCompleted;

        if (publisherName.empty())
        {
            publisherName = defaultPublisherName;
        }
        
        RCF_ASSERT(onCompletion);

        Subscription::AsyncClientPtr asyncClientPtr( 
            new Subscription::AsyncClient(clientStub));

        asyncClientPtr->getClientStub().setTransport(
            clientStub.releaseTransport());

        asyncClientPtr->getClientStub().setAsyncDispatcher(*mpServer);
      
        Future<boost::int32_t>      ret;
        boost::uint32_t             outgoingPingIntervalMs = mPingIntervalMs;
        Future<boost::uint32_t>     incomingPingIntervalMs;

        bool pingsEnabled = true;

        if (clientStub.getRuntimeVersion() < 8)
        {
            pingsEnabled = false;

            ret = asyncClientPtr->RequestSubscription(

                AsyncTwoway( boost::bind( 
                    &SubscriptionService::createSubscriptionImplEnd, 
                    this,
                    asyncClientPtr,
                    ret,
                    publisherName,
                    rcfClientPtr,
                    onDisconnect,
                    onCompletion,
                    incomingPingIntervalMs,
                    pingsEnabled)),

                publisherName);
        }
        else
        {
            ret = asyncClientPtr->RequestSubscription(

                AsyncTwoway( boost::bind( 
                    &SubscriptionService::createSubscriptionImplEnd, 
                    this,
                    asyncClientPtr,
                    ret,
                    publisherName,
                    rcfClientPtr,
                    onDisconnect,
                    onCompletion,
                    incomingPingIntervalMs,
                    pingsEnabled)),

                publisherName,
                outgoingPingIntervalMs,
                incomingPingIntervalMs);

        }
    }

    void SubscriptionService::closeSubscription(
        SubscriptionWeakPtr subscriptionWeakPtr)
    {
        {
            Lock lock(mSubscriptionsMutex);
            mSubscriptions.erase(subscriptionWeakPtr);
        }
    }

    void SubscriptionService::setPingIntervalMs(boost::uint32_t pingIntervalMs)
    {
        mPingIntervalMs = pingIntervalMs;
    }

    boost::uint32_t SubscriptionService::getPingIntervalMs() const
    {
        return mPingIntervalMs;
    }

    void SubscriptionService::onServerStart(RcfServer &server)
    {
        mpServer = &server;
        mPeriodicTimer.setIntervalMs(mPingIntervalMs);
        mPeriodicTimer.start();
    }

    void SubscriptionService::onServerStop(RcfServer &server)
    {
        RCF_UNUSED_VARIABLE(server);

        mPeriodicTimer.stop();

        Subscriptions subs;

        {
            Lock writeLock(mSubscriptionsMutex);
            subs = mSubscriptions;
        }

        for (Subscriptions::iterator iter = subs.begin();
            iter != subs.end();
            ++iter)
        {
            SubscriptionPtr subscriptionPtr = iter->lock();
            if (subscriptionPtr)
            {
                subscriptionPtr->close();
            }
        }

        {
            Lock writeLock(mSubscriptionsMutex);
            RCF_ASSERT(mSubscriptions.empty());
        }

        mSubscriptions.clear();
        subs.clear();

        mpServer = NULL;
    }

    void SubscriptionService::onTimer()
    {
        pingAllSubscriptions();
        harvestExpiredSubscriptions();
    }

    void SubscriptionService::sOnPingCompleted(RecursiveLockPtr lockPtr)
    {
        lockPtr->unlock();
        lockPtr.reset();
    }

    void SubscriptionService::pingAllSubscriptions()
    {
        // Send oneway pings on all our subscriptions, so the publisher
        // knows we're still alive.

        Subscriptions subs;
        {
            Lock lock(mSubscriptionsMutex);
            subs = mSubscriptions;
        }

        Subscriptions::iterator iter;
        for (iter = subs.begin(); iter != subs.end(); ++iter)
        {
            SubscriptionPtr subPtr = iter->lock();
            if (subPtr)
            {
                Subscription & sub = * subPtr;
                if (sub.mPingsEnabled && sub.isConnected())
                {
                    // Lock will be unlocked when the asynchronous send completes.
                    // Using recursive lock here because the ping may result in a 
                    // disconnect, which will then automatically close the connection
                    // and close the subscription, which requires the lock to be taken again.
                    boost::shared_ptr<RecursiveLock> lockPtr( new RecursiveLock(sub.mMutex) );

                    // TODO: async pings
                    bool asyncPings = false;
                    if (asyncPings)
                    {
                        AsioErrorCode ecDummy;

                        sub.mConnectionPtr->getClientStub().ping(
                            RCF::AsyncOneway(boost::bind(
                                &SubscriptionService::sOnPingCompleted, 
                                lockPtr)));
                    }
                    else
                    {
                        try
                        {
                            sub.mConnectionPtr->getClientStub().ping(RCF::Oneway);
                        }
                        catch(const RCF::Exception & e)
                        {
                            std::string errMsg = e.getErrorString();
                            RCF_UNUSED_VARIABLE(errMsg);
                        }
                    }
                }
            }
        }
    }

    void SubscriptionService::harvestExpiredSubscriptions()
    {
        // Kill off subscriptions that haven't received any recent pings.

        Subscriptions subsToDrop;

        {
            Lock lock(mSubscriptionsMutex);

            Subscriptions::iterator iter;
            for (iter = mSubscriptions.begin(); iter != mSubscriptions.end(); ++iter)
            {
                SubscriptionPtr subPtr = iter->lock();
                if (subPtr)
                {
                    Subscription & sub = * subPtr;

                    RecursiveLock lock(sub.mMutex);
                    RcfSessionPtr sessionPtr = sub.mRcfSessionWeakPtr.lock();

                    if (!sessionPtr)
                    {
                        RCF_LOG_2()(sub.mPublisherUrl)(sub.mTopic) << "Dropping subscription. Publisher has closed connection.";
                        subsToDrop.insert(*iter);
                    }
                    else if (sub.mPingsEnabled)
                    {
                        boost::uint32_t pingIntervalMs = sub.mPingIntervalMs;
                        if (pingIntervalMs)
                        {
                            RCF::Timer pingTimer(sessionPtr->getPingTimestamp());
                            if (pingTimer.elapsed(5000 + 2*pingIntervalMs))
                            {
                                RCF_LOG_2()(sub.mPublisherUrl)(sub.mTopic)(sub.mPingIntervalMs) << "Dropping subscription. Publisher has not sent pings.";
                                subsToDrop.insert(*iter);
                            }
                        }
                    }
                }
            }

            for (iter = subsToDrop.begin(); iter != subsToDrop.end(); ++iter)
            {
                mSubscriptions.erase(*iter);
            }
        }

        subsToDrop.clear();
    }

    Subscription::Subscription(
        SubscriptionService & subscriptionService,
        ClientTransportAutoPtr clientTransportAutoPtr,
        RcfSessionWeakPtr rcfSessionWeakPtr,
        boost::uint32_t incomingPingIntervalMs,
        const std::string & publisherUrl,
        const std::string & topic,
        OnSubscriptionDisconnect onDisconnect) :
            mSubscriptionService(subscriptionService),
            mRcfSessionWeakPtr(rcfSessionWeakPtr),
            mConnectionPtr( new RcfClient<I_Null>(clientTransportAutoPtr) ),
            mPingIntervalMs(incomingPingIntervalMs),
            mPublisherUrl(publisherUrl),
            mTopic(topic),
            mOnDisconnect(onDisconnect),
            mClosed(false)
    {
        mConnectionPtr->getClientStub().setAutoReconnect(false);
    }
   
} // namespace RCF

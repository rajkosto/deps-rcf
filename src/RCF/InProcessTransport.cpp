
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

#include <RCF/InProcessTransport.hpp>

#include <RCF/ClientStub.hpp>
#include <RCF/CurrentSession.hpp>
#include <RCF/Endpoint.hpp>
#include <RCF/InProcessEndpoint.hpp>
#include <RCF/ServerTransport.hpp>

namespace RCF {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4355) // warning C4355: 'this' : used in base member initializer list
#endif

    InProcessTransport::InProcessTransport(RcfServer & server, RcfServer * pCallbackServer) : 
        mServer(server), 
        mpCallbackServer(pCallbackServer),
        mSessionPtr( new RcfSession(server) ),
        mRemoteAddress(*this)
    {
        mSessionPtr->mInProcess = true;
        mSessionPtr->setSessionState(*this);
    }

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    InProcessTransport::~InProcessTransport()
    {
    }

    TransportType InProcessTransport::getTransportType()
    {
        return Tt_InProcess;
    }

    bool InProcessTransport::isInProcess()
    {
        return true;
    }

    void InProcessTransport::doInProcessCall(ClientStub & clientStub)
    {
        CurrentRcfSessionSentry sessionSentry(*mSessionPtr);
        mSessionPtr->mRequest.init(clientStub.mRequest);
        mSessionPtr->mpInProcessParameters = clientStub.mpParameters;
        mSessionPtr->invokeServant();
    }

    std::auto_ptr<ClientTransport> InProcessTransport::clone() const
    {
        RCF_ASSERT(0);
        return std::auto_ptr<ClientTransport>();
    }

    EndpointPtr InProcessTransport::getEndpointPtr() const
    {
        RCF_ASSERT(0);
        return EndpointPtr();
    }

    int InProcessTransport::send(
        ClientTransportCallback &     clientStub, 
        const std::vector<ByteBuffer> & data, 
        unsigned int                    timeoutMs)
    {
        RCF_ASSERT(0);
        RCF_UNUSED_VARIABLE(clientStub);
        RCF_UNUSED_VARIABLE(data);
        RCF_UNUSED_VARIABLE(timeoutMs);
        return 0;
    }

    int InProcessTransport::receive(
        ClientTransportCallback &     clientStub, 
        ByteBuffer &                    byteBuffer, 
        unsigned int                    timeoutMs)
    {
        RCF_ASSERT(0);
        RCF_UNUSED_VARIABLE(clientStub);
        RCF_UNUSED_VARIABLE(byteBuffer);
        RCF_UNUSED_VARIABLE(timeoutMs);
        return 0;
    }

    bool InProcessTransport::isConnected()
    {
        //RCF_ASSERT(0);
        return true;
    }

    void InProcessTransport::connect(
        ClientTransportCallback &     clientStub, 
        unsigned int                    timeoutMs)
    {
        //RCF_ASSERT(0);
        RCF_UNUSED_VARIABLE(clientStub);
        RCF_UNUSED_VARIABLE(timeoutMs);
    }

    void InProcessTransport::disconnect(
        unsigned int                    timeoutMs)
    {
        //RCF_ASSERT(0);
        RCF_UNUSED_VARIABLE(timeoutMs);
    }

    void InProcessTransport::setTransportFilters(
        const std::vector<FilterPtr> &  filters)
    {
        RCF_ASSERT(0);
        RCF_UNUSED_VARIABLE(filters);
    }

    void InProcessTransport::getTransportFilters(
        std::vector<FilterPtr> &        filters)
    {
        RCF_ASSERT(0);
        RCF_UNUSED_VARIABLE(filters);
    }

    void InProcessTransport::setTimer(
        boost::uint32_t timeoutMs,
        ClientTransportCallback * pClientStub)
    {
        RCF_ASSERT(0);
        RCF_UNUSED_VARIABLE(timeoutMs);
        RCF_UNUSED_VARIABLE(pClientStub);
    }

    void InProcessTransport::postRead()
    {
        RCF_ASSERT(0);
    }

    ByteBuffer InProcessTransport::getReadByteBuffer()
    {
        RCF_ASSERT(0);
        return ByteBuffer();
    }
    void InProcessTransport::postWrite(std::vector<ByteBuffer> &byteBuffers)
    {
        RCF_ASSERT(0);
        RCF_UNUSED_VARIABLE(byteBuffers);
    }
    void InProcessTransport::postClose()
    {
        RCF_ASSERT(0);
    }
    ServerTransport & InProcessTransport::getServerTransport()
    {
        return *this;
    }
    const RemoteAddress & InProcessTransport::getRemoteAddress()
    {
        return mRemoteAddress;
    }
    //void InProcessTransport::setTransportFilters(const std::vector<FilterPtr> &filters)
    //{
    //  //RCF_ASSERT(0);
    //}
    //void InProcessTransport::getTransportFilters(std::vector<FilterPtr> &filters)
    //{
    //  //RCF_ASSERT(0);
    //}

    // I_ServerTransport
    ServerTransportPtr InProcessTransport::clone()
    {
        RCF_ASSERT(0);
        return ServerTransportPtr();
    }

    // I_ServerTransportEx
    ClientTransportAutoPtr 
        InProcessTransport::createClientTransport(
            const Endpoint &endpoint)
    {
        const InProcessEndpoint & ep = 
            dynamic_cast<const InProcessEndpoint &>(endpoint);

        ClientTransportAutoPtr clientTransportAutoPtr = ep.createClientTransport();

        return clientTransportAutoPtr;
    }

    SessionPtr 
        InProcessTransport::createServerSession(
            ClientTransportAutoPtr & clientTransportAutoPtr,
            StubEntryPtr stubEntryPtr,
            bool keepClientConnection)
    {
        RCF_UNUSED_VARIABLE(keepClientConnection);

        InProcessTransport * pTransport = dynamic_cast<InProcessTransport *>(
            clientTransportAutoPtr.get());

        if (pTransport->mOppositeSessionPtr && stubEntryPtr)
        {
            pTransport->mOppositeSessionPtr->setDefaultStubEntryPtr(stubEntryPtr);
        }

        return pTransport->mSessionPtr;
    }

    ClientTransportAutoPtr 
        InProcessTransport::createClientTransport(
            SessionPtr sessionPtr)
    {
        InProcessTransport * pTransport = dynamic_cast<InProcessTransport *>(
            & getCurrentRcfSession().getSessionState());

        RCF_ASSERT(pTransport);
        RCF_ASSERT(pTransport->mpCallbackServer);

        InProcessTransport * pNewTransport = 
            new InProcessTransport(* pTransport->mpCallbackServer);

        pTransport->mOppositeSessionPtr = pNewTransport->mSessionPtr;

        ClientTransportAutoPtr clientTransportAutoPtr(pNewTransport);
        return clientTransportAutoPtr;
    }

    bool 
        InProcessTransport::reflect(
            const SessionPtr &sessionPtr1,
            const SessionPtr &sessionPtr2)
    {
        RCF_ASSERT(0);
        RCF_UNUSED_VARIABLE(sessionPtr1);
        RCF_UNUSED_VARIABLE(sessionPtr2);
        return false;
    }

    bool 
        InProcessTransport::isConnected(
            const SessionPtr &sessionPtr)
    {
        RCF_UNUSED_VARIABLE(sessionPtr);
        return true;
    }

} // namespace RCF

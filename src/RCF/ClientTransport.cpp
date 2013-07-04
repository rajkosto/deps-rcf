
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

#include <RCF/ClientTransport.hpp>

#include <RCF/Asio.hpp>
#include <RCF/ClientStub.hpp>
#include <RCF/Exception.hpp>
#include <RCF/ServerTransport.hpp>

namespace RCF {

    I_ClientTransport::I_ClientTransport() :
        mMaxMessageLength(getDefaultMaxMessageLength()),
        mLastRequestSize(0),
        mLastResponseSize(0),
        mRunningTotalBytesSent(0),
        mRunningTotalBytesReceived(0),
        mAsync(false)
    {}

    I_ClientTransport::I_ClientTransport(const I_ClientTransport & rhs) :
        mMaxMessageLength(rhs.mMaxMessageLength),
        mLastRequestSize(),
        mLastResponseSize(0),
        mRunningTotalBytesSent(0),
        mRunningTotalBytesReceived(0),
        mAsync(false)
    {
    }

    bool I_ClientTransport::isInProcess()
    {
        return false;
    }

    void I_ClientTransport::doInProcessCall(ClientStub & clientStub)
    {
        RCF_UNUSED_VARIABLE(clientStub);
        RCF_ASSERT(0);
    }

    bool I_ClientTransport::isConnected()
    {
        return true;
    }

    void I_ClientTransport::setMaxMessageLength(std::size_t maxMessageLength)
    {
        setMaxIncomingMessageLength(maxMessageLength);
    }

    std::size_t I_ClientTransport::getMaxMessageLength() const
    {
        return getMaxIncomingMessageLength();
    }

    void I_ClientTransport::setMaxIncomingMessageLength(
        std::size_t maxMessageLength)
    {
        mMaxMessageLength = maxMessageLength;
    }

    std::size_t I_ClientTransport::getMaxIncomingMessageLength() const
    {
        return mMaxMessageLength;
    }

    RcfSessionWeakPtr I_ClientTransport::getRcfSession()
    {
        return mRcfSessionWeakPtr;
    }

    void I_ClientTransport::setRcfSession(RcfSessionWeakPtr rcfSessionWeakPtr)
    {
        mRcfSessionWeakPtr = rcfSessionWeakPtr;
    }

    std::size_t I_ClientTransport::getLastRequestSize()
    {
        return mLastRequestSize;
    }

    std::size_t I_ClientTransport::getLastResponseSize()
    {
        return mLastResponseSize;
    }

    boost::uint64_t I_ClientTransport::getRunningTotalBytesSent()
    {
        return mRunningTotalBytesSent;
    }

    boost::uint64_t I_ClientTransport::getRunningTotalBytesReceived()
    {
        return mRunningTotalBytesReceived;
    }

    void I_ClientTransport::resetRunningTotals()
    {
        mRunningTotalBytesSent = 0;
        mRunningTotalBytesReceived = 0;
    }

    void I_ClientTransportCallback::setAsyncDispatcher(RcfServer & server)
    {
        RCF_ASSERT(!mpAsyncDispatcher);
        mpAsyncDispatcher = &server;
    }

    RcfServer * I_ClientTransportCallback::getAsyncDispatcher()
    {
        return mpAsyncDispatcher;
    }

    void I_ClientTransport::setAsync(bool async)
    {
        mAsync = async;
    }

    void I_ClientTransport::associateWithIoService(AsioIoService & ioService)
    {
        RCF_UNUSED_VARIABLE(ioService);
        RCF_ASSERT(0 && "Asynchronous operations not implemented for this transport.");
    }

    bool I_ClientTransport::isAssociatedWithIoService()
    {
        return false;
    }

    void I_ClientTransport::cancel()
    {
        RCF_ASSERT(0 && "cancel() not implemented for this transport");
    }

} // namespace RCF


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

#include <RCF/ServerTransport.hpp>

#include <RCF/Service.hpp>

namespace RCF {

    I_ServerTransport::I_ServerTransport() :
        mRpcProtocol(Rp_Rcf),
        mCustomFraming(false),
        mReadWriteMutex(WriterPriority),
        mMaxMessageLength(getDefaultMaxMessageLength()),
        mConnectionLimit(0),
        mInitialNumberOfConnections(1)
    {}

    I_ServerTransport & I_ServerTransport::setMaxMessageLength(std::size_t maxMessageLength)
    {
        return setMaxIncomingMessageLength(maxMessageLength);
    }

    std::size_t I_ServerTransport::getMaxMessageLength() const
    {
        return getMaxIncomingMessageLength();
    }

    I_ServerTransport & I_ServerTransport::setMaxIncomingMessageLength(std::size_t maxMessageLength)
    {
        WriteLock writeLock(mReadWriteMutex);
        mMaxMessageLength = maxMessageLength;
        return *this;
    }

    std::size_t I_ServerTransport::getMaxIncomingMessageLength() const
    {
        ReadLock readLock(mReadWriteMutex);
        return mMaxMessageLength;
    }

    std::size_t I_ServerTransport::getConnectionLimit() const
    {
        ReadLock readLock(mReadWriteMutex);
        return mConnectionLimit;
    }

    I_ServerTransport & I_ServerTransport::setConnectionLimit(
        std::size_t connectionLimit)
    {
        WriteLock writeLock(mReadWriteMutex);
        mConnectionLimit = connectionLimit;

        return *this;
    }

    std::size_t I_ServerTransport::getInitialNumberOfConnections() const
    {
        ReadLock readLock(mReadWriteMutex);
        return mInitialNumberOfConnections;
    }

    I_ServerTransport & I_ServerTransport::setInitialNumberOfConnections(
        std::size_t initialNumberOfConnections)
    {
        WriteLock writeLock(mReadWriteMutex);
        mInitialNumberOfConnections = initialNumberOfConnections;

        return *this;
    }

    void I_ServerTransport::setRpcProtocol(RpcProtocol rpcProtocol)
    {
        mRpcProtocol = rpcProtocol;

        // For JSON-RPC over HTTP/S, enable HTTP framing.
        if (rpcProtocol == Rp_JsonRpc)
        {
            TransportType tt = getTransportType();
            if (tt == Tt_Http || tt == Tt_Https)
            {
                mCustomFraming = true;
            }
        }
    }

    RpcProtocol I_ServerTransport::getRpcProtocol() const
    {
        return mRpcProtocol;
    }

    I_ServerTransport & I_ServerTransport::setThreadPool(
        ThreadPoolPtr threadPoolPtr)
    {
        I_Service & svc = dynamic_cast<I_Service &>(*this);
        svc.setThreadPool(threadPoolPtr);
        return *this;
    }

    I_ServerTransport & I_ServerTransport::setSupportedProtocols(const std::vector<TransportProtocol> & protocols)
    {
        mSupportedProtocols = protocols;
        return *this;
    }

    const std::vector<TransportProtocol> & I_ServerTransport::getSupportedProtocols() const
    {
        return mSupportedProtocols;
    }

    std::size_t gDefaultMaxMessageLength = 1024*1024; // 1 Mb

    std::size_t getDefaultMaxMessageLength()
    {
        return gDefaultMaxMessageLength;
    }

    void setDefaultMaxMessageLength(std::size_t maxMessageLength)
    {
        gDefaultMaxMessageLength = maxMessageLength;
    }

    I_SessionState::I_SessionState() :
        mEnableReconnect(true),
        mBytesReceivedCounter(0),
        mBytesSentCounter(0)
    {
    }

    void I_SessionState::setEnableReconnect(bool enableReconnect)
    {
        mEnableReconnect = enableReconnect;
    }

    bool I_SessionState::getEnableReconnect()
    {
        return mEnableReconnect;
    }

    boost::uint64_t I_SessionState::getTotalBytesReceived() const
    {
        return mBytesReceivedCounter;
    }

    boost::uint64_t I_SessionState::getTotalBytesSent() const
    {
        return mBytesSentCounter;
    }

} // namespace RCF

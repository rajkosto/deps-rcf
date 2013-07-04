
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

#ifndef INCLUDE_RCF_SERVERTRANSPORT_HPP
#define INCLUDE_RCF_SERVERTRANSPORT_HPP

#include <memory>
#include <string>
#include <vector>

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include <RCF/ByteBuffer.hpp>
#include <RCF/Enums.hpp>
#include <RCF/Export.hpp>
#include <RCF/Filter.hpp>
#include <RCF/ThreadLibrary.hpp>

namespace RCF {

    class Filter;
    class I_Endpoint;
    class I_ClientTransport;
    class I_SessionState;
    class StubEntry;

    typedef boost::shared_ptr<Filter>               FilterPtr;
    typedef std::auto_ptr<I_ClientTransport>        ClientTransportAutoPtr;
    typedef boost::shared_ptr<StubEntry>            StubEntryPtr;

    class I_ServerTransport;
    typedef boost::shared_ptr<I_ServerTransport>    ServerTransportPtr;
    typedef std::auto_ptr<I_ServerTransport>        ServerTransportAutoPtr;

    class I_RemoteAddress
    {
    public:
        virtual ~I_RemoteAddress()
        {}

        virtual std::string string() const
        {
            return "";
        }
    };

    class NoRemoteAddress : public I_RemoteAddress
    {};
   
    class I_SessionState : public boost::enable_shared_from_this<I_SessionState>
    {
    public:

        I_SessionState();
        virtual ~I_SessionState() {}

        virtual void        postRead() = 0;
        virtual ByteBuffer  getReadByteBuffer() = 0;
        virtual void        postWrite(std::vector<ByteBuffer> &byteBuffers) = 0;
        virtual void        postClose() = 0;
       
        virtual I_ServerTransport & 
                            getServerTransport() = 0;

        virtual const I_RemoteAddress & 
                            getRemoteAddress() = 0;

        virtual void        setTransportFilters(const std::vector<FilterPtr> &filters) = 0;
        virtual void        getTransportFilters(std::vector<FilterPtr> &filters) = 0;
        void                setEnableReconnect(bool enableReconnect);
        bool                getEnableReconnect();

        boost::uint64_t     getTotalBytesReceived() const;
        boost::uint64_t     getTotalBytesSent() const;

    protected:
        bool                    mEnableReconnect;
        boost::uint64_t         mBytesReceivedCounter;
        boost::uint64_t         mBytesSentCounter;

    };

    typedef boost::shared_ptr<I_SessionState> SessionStatePtr;

    class RcfSession;
    typedef boost::shared_ptr<RcfSession> RcfSessionPtr;
    
    typedef RcfSession I_Session;
    typedef RcfSessionPtr SessionPtr;
    class ThreadPool;
    typedef boost::shared_ptr<ThreadPool> ThreadPoolPtr;

    enum RpcProtocol
    {
        Rp_Rcf = 0,
        Rp_JsonRpc = 1,
    };

    // Base class of all server transport services.
    class RCF_EXPORT I_ServerTransport
    {
    public:
        I_ServerTransport();

        virtual ~I_ServerTransport() {}

        virtual TransportType getTransportType() = 0;

        virtual ServerTransportPtr 
                        clone() = 0;

        // Deprecated - use setMaxIncomingMessageLength()/getMaxIncomingMessageLength() instead.
        I_ServerTransport & setMaxMessageLength(std::size_t maxMessageLength);
        std::size_t         getMaxMessageLength() const;

        I_ServerTransport & setMaxIncomingMessageLength(std::size_t maxMessageLength);
        std::size_t         getMaxIncomingMessageLength() const;

        std::size_t         getConnectionLimit() const;
        I_ServerTransport & setConnectionLimit(std::size_t connectionLimit);

        std::size_t         getInitialNumberOfConnections() const;
        I_ServerTransport & setInitialNumberOfConnections(std::size_t initialNumberOfConnections);
        

        void                setRpcProtocol(RpcProtocol rpcProtocol);
        RpcProtocol         getRpcProtocol() const;

        I_ServerTransport & setThreadPool(ThreadPoolPtr threadPoolPtr);

        I_ServerTransport & setSupportedProtocols(const std::vector<TransportProtocol> & protocols);
        const std::vector<TransportProtocol> & getSupportedProtocols() const;
        
    protected:

        RpcProtocol                 mRpcProtocol;
        bool                        mCustomFraming;

    private:

        mutable ReadWriteMutex      mReadWriteMutex;
        std::size_t                 mMaxMessageLength;
        std::size_t                 mConnectionLimit;       
        std::size_t                 mInitialNumberOfConnections;

        std::vector<TransportProtocol> mSupportedProtocols;
    };

    class I_ServerTransportEx
    {
    public:

        virtual ~I_ServerTransportEx() {}

        virtual ClientTransportAutoPtr 
            createClientTransport(
                const I_Endpoint &endpoint) = 0;
       
        virtual SessionPtr 
            createServerSession(
                ClientTransportAutoPtr & clientTransportAutoPtr,
                StubEntryPtr stubEntryPtr,
                bool keepClientConnection) = 0;

        virtual ClientTransportAutoPtr 
            createClientTransport(
                SessionPtr sessionPtr) = 0;
       
        virtual bool 
            reflect(
                const SessionPtr &sessionPtr1,
                const SessionPtr &sessionPtr2) = 0;
       
        virtual bool 
            isConnected(
                const SessionPtr &sessionPtr) = 0;
    };   

    RCF_EXPORT std::size_t  getDefaultMaxMessageLength();

    RCF_EXPORT void         setDefaultMaxMessageLength(
                                std::size_t maxMessageLength);

} // namespace RCF

#endif // ! INCLUDE_RCF_SERVERTRANSPORT_HPP

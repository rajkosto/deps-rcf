
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

#ifndef INCLUDE_RCF_CLIENTTRANSPORT_HPP
#define INCLUDE_RCF_CLIENTTRANSPORT_HPP

#include <memory>
#include <string>
#include <vector>

#include <boost/cstdint.hpp>
#include <boost/weak_ptr.hpp>

#include <RCF/AsioFwd.hpp>
#include <RCF/Enums.hpp>
#include <RCF/Filter.hpp>
#include <RCF/ByteBuffer.hpp>
#include <RCF/Export.hpp>

namespace RCF {

    class I_Endpoint;
    typedef boost::shared_ptr<I_Endpoint> EndpointPtr;

    class RcfServer;

    class OverlappedAmi;
    typedef boost::shared_ptr<OverlappedAmi> OverlappedAmiPtr;

    class ClientStub;
    class RcfSession;
    typedef boost::weak_ptr<RcfSession> RcfSessionWeakPtr;

    class RCF_EXPORT I_ClientTransportCallback
    {
    public:
        I_ClientTransportCallback() : mpAsyncDispatcher() {}
        virtual ~I_ClientTransportCallback() {}
        virtual void onConnectCompleted(bool alreadyConnected = false) = 0;
        virtual void onSendCompleted() = 0;
        virtual void onReceiveCompleted() = 0;
        virtual void onTimerExpired() = 0;
        virtual void onError(const std::exception &e) = 0;

        void setAsyncDispatcher(RcfServer & server);
        RcfServer * getAsyncDispatcher();

    private:
        RcfServer * mpAsyncDispatcher;
    };

    class ClientStub;

    class RCF_EXPORT I_ClientTransport
    {
    public:
        I_ClientTransport();
        I_ClientTransport(const I_ClientTransport & rhs);

        virtual ~I_ClientTransport()
        {}

        virtual TransportType getTransportType() = 0;

        virtual 
        std::auto_ptr<I_ClientTransport> clone() const = 0;

        virtual 
        EndpointPtr getEndpointPtr() const = 0;
       
        virtual 
        int send(
            I_ClientTransportCallback &     clientStub, 
            const std::vector<ByteBuffer> & data, 
            unsigned int                    timeoutMs) = 0;

        virtual 
        int receive(
            I_ClientTransportCallback &     clientStub, 
            ByteBuffer &                    byteBuffer, 
            unsigned int                    timeoutMs) = 0;

        virtual 
        bool isConnected() = 0;

        virtual 
        void connect(
            I_ClientTransportCallback &     clientStub, 
            unsigned int                    timeoutMs) = 0;

        virtual 
        void disconnect(
            unsigned int                    timeoutMs = 0) = 0;

        virtual 
        void setTransportFilters(
            const std::vector<FilterPtr> &  filters) = 0;
       
        virtual 
        void getTransportFilters(
            std::vector<FilterPtr> &        filters) = 0;

        // Deprecated - use setMaxIncomingMessageLength()/getMaxIncomingMessageLength() instead.
        void setMaxMessageLength(std::size_t maxMessageLength);
        std::size_t getMaxMessageLength() const;

        void setMaxIncomingMessageLength(std::size_t maxMessageLength);
        std::size_t getMaxIncomingMessageLength() const;

        RcfSessionWeakPtr getRcfSession();
        void setRcfSession(RcfSessionWeakPtr rcfSessionWeakPtr);

        std::size_t getLastRequestSize();
        std::size_t getLastResponseSize();

        boost::uint64_t getRunningTotalBytesSent();
        boost::uint64_t getRunningTotalBytesReceived();
        void resetRunningTotals();

        void setAsync(bool async);

        virtual void cancel();

        virtual void setTimer(
            boost::uint32_t timeoutMs,
            I_ClientTransportCallback * pClientStub = NULL) = 0;

        virtual void associateWithIoService(AsioIoService & ioService);
        virtual bool isAssociatedWithIoService();

        virtual bool isInProcess();
        virtual void doInProcessCall(ClientStub & clientStub);

        virtual bool supportsTransportFilters()
        {
            return true;
        }

    private:
        std::size_t mMaxMessageLength;
        RcfSessionWeakPtr mRcfSessionWeakPtr;

    protected:
        std::size_t mLastRequestSize;
        std::size_t mLastResponseSize;

        boost::uint64_t mRunningTotalBytesSent;
        boost::uint64_t mRunningTotalBytesReceived;

        bool mAsync;

        friend class ClientStub;
    };

    typedef boost::shared_ptr<I_ClientTransport> ClientTransportPtr;

    typedef std::auto_ptr<I_ClientTransport> ClientTransportAutoPtr;

    typedef boost::shared_ptr< ClientTransportAutoPtr > ClientTransportAutoPtrPtr;

} // namespace RCF

#endif // ! INCLUDE_RCF_CLIENTTRANSPORT_HPP

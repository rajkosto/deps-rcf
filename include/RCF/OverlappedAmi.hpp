
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

#ifndef INCLUDE_RCF_OVERLAPPEDAMI_HPP
#define INCLUDE_RCF_OVERLAPPEDAMI_HPP

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <RCF/ThreadLibrary.hpp>

namespace RCF {

    class RCF_EXPORT AmiNotification
    {
    public:

        typedef boost::function0<void> Cb;

        // Need mutexPtr so that the mutex doesn't die before the lock.
        void set(Cb cb, LockPtr lockPtr, MutexPtr mutexPtr);
        void run();
        void clear();

    private:
        Cb                  mCb;
        MutexPtr   mMutexPtr;
        LockPtr    mLockPtr;
    };


    class OverlappedAmi;
    typedef boost::shared_ptr<OverlappedAmi> OverlappedAmiPtr;

    class ConnectionOrientedClientTransport;

    class OverlappedAmi : 
        public boost::enable_shared_from_this<OverlappedAmi>
    {
    public:

        enum AsyncOpType
        {
            None,
            Wait,
            Connect,
            Write,
            Read
        };

        OverlappedAmi(ConnectionOrientedClientTransport *pTcpClientTransport) : 
            mpTransport(pTcpClientTransport),
            mIndex(0),
            mOpType(None)
        {
        }

        ~OverlappedAmi()
        {
        }

        void onCompletion(
            std::size_t index,
            const AsioErrorCode & ec, 
            std::size_t bytesTransferred);

        void onTimerExpired(
            std::size_t index,
            const AsioErrorCode & ec);

        RecursiveMutex                      mMutex;
        ConnectionOrientedClientTransport * mpTransport;
        std::size_t                         mIndex;
        AsyncOpType                         mOpType;
    };


} // namespace RCF

#endif // ! INCLUDE_RCF_OVERLAPPEDAMI_HPP

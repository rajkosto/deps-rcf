
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

#ifndef INCLUDE_RCF_ASIO_HPP
#define INCLUDE_RCF_ASIO_HPP

#include <RCF/Config.hpp>

// Turn off auto-linking for Boost Date Time lib. Asio headers include some boost/date_time/ headers.
#define BOOST_DATE_TIME_NO_LIB
#define BOOST_REGEX_NO_LIB

#include <RCF/AsioFwd.hpp>

// Some issues with asio headers.
//#if defined(__MACH__) && defined(__APPLE__)
//#include <limits.h>
//#ifndef IOV_MAX
//#define IOV_MAX 1024
//#endif
//#endif

#if defined(BOOST_WINDOWS) || defined(__CYGWIN__)
# if defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#  error WinSock.h has already been included. Define WIN32_LEAN_AND_MEAN in your build, to avoid implicit inclusion of WinSock.h.
# endif // defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4913) // user defined binary operator ',' exists but no overload could convert all operands, default built-in binary operator ',' used
#endif

#ifdef RCF_USE_BOOST_ASIO
#include <boost/asio.hpp>
#else
#include <RCF/external/asio/asio.hpp>
#endif

// Do we have local sockets?
#ifdef RCF_USE_BOOST_ASIO
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
#define RCF_HAS_LOCAL_SOCKETS
#endif
#else
#ifdef ASIO_HAS_LOCAL_SOCKETS
#define RCF_HAS_LOCAL_SOCKETS
#endif
#endif

#include <RCF/AsioDeadlineTimer.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace RCF {


    typedef ASIO_NS::ip::tcp::socket                    AsioSocket;
    typedef boost::shared_ptr<AsioSocket>               AsioSocketPtr;

    typedef ASIO_NS::const_buffer                       AsioConstBuffer;

    // This adapter around a std::vector prevents asio from making a deep copy
    // of the buffer list, when sending multiple buffers. The deep copy would
    // involve making memory allocations.
    class AsioBuffers
    {
    public:

        typedef std::vector<AsioConstBuffer>            BufferVec;
        typedef boost::shared_ptr<BufferVec>            BufferVecPtr;

        typedef AsioConstBuffer                         value_type;
        typedef BufferVec::const_iterator               const_iterator;

        AsioBuffers()
        {
            mVecPtr.reset( new std::vector<AsioConstBuffer>() );
        }

        const_iterator begin() const
        {
            return mVecPtr->begin();
        }

        const_iterator end() const
        {
            return mVecPtr->end();
        }

        BufferVecPtr mVecPtr;
    };

} // namespace RCF


#endif // ! INCLUDE_RCF_ASIO_HPP

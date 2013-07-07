
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

#ifndef INCLUDE_RCF_TOOLS_HPP
#define INCLUDE_RCF_TOOLS_HPP

// Various utilities

#include <deque>
#include <iosfwd>
#include <iterator>
#include <stdexcept>
#include <typeinfo>
#include <vector>

#include <boost/config.hpp>
#include <boost/shared_ptr.hpp>

#include <RCF/Export.hpp>
#include <RCF/util/UnusedVariable.hpp>

// Logging mechanism
#include <RCF/util/Log.hpp>

namespace RCF {
    static const int LogNameRcf = 1;
    static const int LogLevel_1 = 1; // Error and exceptions.
    static const int LogLevel_2 = 2; // Larger scale setup/teardown.
    static const int LogLevel_3 = 3; // Messages sent and received (RCF level), RCF client and session lifetime.
    static const int LogLevel_4 = 4; // Messages sent and received (network level), network client and session lifetime.

} // namespace RCF

#define RCF_LOG_1() UTIL_LOG(RCF::LogNameRcf, RCF::LogLevel_1)
#define RCF_LOG_2() UTIL_LOG(RCF::LogNameRcf, RCF::LogLevel_2)
#define RCF_LOG_3() UTIL_LOG(RCF::LogNameRcf, RCF::LogLevel_3)
#define RCF_LOG_4() UTIL_LOG(RCF::LogNameRcf, RCF::LogLevel_4)

// Assertion mechanism
#include <RCF/util/Assert.hpp>
#define RCF_ASSERT(x) UTIL_ASSERT(x, RCF::AssertionFailureException(), RCF::LogNameRcf, RCF::LogLevel_1)

#define RCF_ASSERT_EQ(a,b)      RCF_ASSERT(a == b)(a)(b)
#define RCF_ASSERT_NEQ(a,b)     RCF_ASSERT(a != b)(a)(b)

#define RCF_ASSERT_LT(a,b)      RCF_ASSERT(a < b)(a)(b)
#define RCF_ASSERT_LTEQ(a,b)    RCF_ASSERT(a <= b)(a)(b)

#define RCF_ASSERT_GT(a,b)      RCF_ASSERT(a > b)(a)(b)
#define RCF_ASSERT_GTEQ(a,b)    RCF_ASSERT(a >= b)(a)(b)

// Throw mechanism
#include <RCF/util/Throw.hpp>
namespace RCF {
    class Exception;
    RCF_EXPORT ::util::DummyVariableArgMacroObject rcfThrow(const char * szFile, int line, const char * szFunc, const Exception & e);
}
#define RCF_THROW(e)            RCF::rcfThrow(__FILE__, __LINE__, __FUNCTION__, e)

// Verification mechanism
#define RCF_VERIFY(cond, e)     if (cond); else RCF_THROW(e)


// Scope guard mechanism
#include <boost/multi_index/detail/scope_guard.hpp>

namespace RCF 
{
    class Exception;
}

// assorted tracing conveniences
namespace std {

    // Trace std::vector
    template<typename T>
    std::ostream &operator<<(std::ostream &os, const std::vector<T> &v)
    {
        os << "(";
        std::copy(v.begin(), v.end(), std::ostream_iterator<T>(os, ", "));
        os << ")";
        return os;
    }

    // Trace std::deque
    template<typename T>
    std::ostream &operator<<(std::ostream &os, const std::deque<T> &d)
    {
        os << "(";
        std::copy(d.begin(), d.end(), std::ostream_iterator<T>(os, ", "));
        os << ")";
        return os;
    }

    // Trace type_info
    RCF_EXPORT std::ostream &operator<<(std::ostream &os, const std::type_info &ti);

    // Trace exception
    RCF_EXPORT std::ostream &operator<<(std::ostream &os, const std::exception &e);

    // Trace exception
    RCF_EXPORT std::ostream &operator<<(std::ostream &os, const RCF::Exception &e);

} // namespace std

namespace RCF {

    // null deleter, for use with for shared_ptr
    class NullDeleter
    {
    public:
        template<typename T>
        void operator()(T)
        {}
    };

    class SharedPtrIsNull
    {
    public:
        template<typename T>
        bool operator()(boost::shared_ptr<T> spt) const
        {
            return spt.get() == NULL;
        }
    };

} // namespace RCF

namespace RCF {

    

} // namespace RCF

// destructor try/catch blocks
#define RCF_DTOR_BEGIN                              \
    try {

#define RCF_DTOR_END                                \
    }                                               \
    catch (const std::exception &e)                 \
    {                                               \
        if (!util::detail::uncaught_exception())    \
        {                                           \
            throw;                                  \
        }                                           \
        else                                        \
        {                                           \
            RCF_LOG_1()(e);                         \
        }                                           \
    }

//#if defined(_MSC_VER) && _MSC_VER < 1310
//#define RCF_PFTO_HACK long
//#else
//#define RCF_PFTO_HACK
//#endif
#define RCF_PFTO_HACK

// Auto linking on VC++
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "crypt32.lib")
#endif

namespace RCF {

    struct Void {};

    template<typename Container, typename Element>
    void eraseRemove(Container & container, const Element & element)
    {
        container.erase(
            std::remove(
                container.begin(),
                container.end(),
                element),
            container.end());
    }

    RCF_EXPORT boost::uint64_t fileSize(const std::string & path);

} // namespace RCF

namespace boost {
    
    template<typename T>
    inline bool operator==(
        const boost::weak_ptr<T> & lhs, 
        const boost::weak_ptr<T> & rhs)
    {
        return ! (lhs < rhs) && ! (rhs < lhs);
    }

    template<typename T>
    inline bool operator!=(
        const boost::weak_ptr<T> & lhs, 
        const boost::weak_ptr<T> & rhs)
    {
        return ! (lhs == rhs);
    }

} // namespace boost

#endif // ! INCLUDE_RCF_TOOLS_HPP


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

#include <RCF/Tools.hpp>

#include <RCF/Exception.hpp>
#include <RCF/InitDeinit.hpp>
#include <RCF/ThreadLibrary.hpp>

namespace RCF {

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4995) // 'sprintf': name was marked as #pragma deprecated
#pragma warning(disable: 4996) // 'sprintf': This function or variable may be unsafe.
#endif

    ::util::DummyVariableArgMacroObject rcfThrow(const char * szFile, int line, const char * szFunc, const Exception & e)
    {
        std::string context = szFile;
        context += ":";
        char szBuffer[32] = {0};
        sprintf(szBuffer, "%d", line);
        context += szBuffer;
        const_cast<Exception&>(e).setContext(context);

        if (RCF::LogManager::instance().isEnabled(LogNameRcf, LogLevel_1))
        {
            RCF::LogEntry entry(LogNameRcf, LogLevel_1, szFile, line, szFunc);

            entry
                << "RCF exception thrown. Error message: "
                << e.getErrorString();
        }

        e.throwSelf();

        return ::util::DummyVariableArgMacroObject();
    }

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

}

namespace std {

    // Logging of type_info.
    std::ostream &operator<<(std::ostream &os, const std::type_info &ti)
    {
        return os << ti.name();
    }

    // Logging of exception.
    std::ostream &operator<<(std::ostream &os, const std::exception &e)
    {
        os << RCF::toString(e);
        return os;
    }

    // Logging of RCF::Exception.
    std::ostream &operator<<(std::ostream &os, const RCF::Exception &e)
    {
        os << RCF::toString(e);
        return os;
    }

} // namespace std

namespace RCF {

    std::string toString(const std::exception &e)
    {
        std::ostringstream os;

        const RCF::Exception *pE = dynamic_cast<const RCF::Exception *>(&e);
        if (pE)
        {
            int err = pE->getErrorId();
            std::string errMsg = pE->getErrorString();
            os << "[RCF: " << err << ": " << errMsg << "]";
        }
        else
        {
            os << "[What: " << e.what() << "]" ;
        }

        return os.str();
    }

    // Generate a timeout value for the given ending time.
    // Returns zero if endTime <= current time <= endTime+10% of timer resolution,
    // otherwise returns a nonzero duration in ms.
    // Timer resolution as above (49 days).
    boost::uint32_t generateTimeoutMs(unsigned int endTimeMs)
    {
        // 90% of the timer interval
        boost::uint32_t currentTimeMs = getCurrentTimeMs();
        boost::uint32_t timeoutMs = endTimeMs - currentTimeMs;
        return (timeoutMs <= MaxTimeoutMs) ? timeoutMs : 0;
    }

    boost::uint64_t fileSize(const std::string & path)
    {
        // TODO: this may not work for files larger than 32 bits, on some 32 bit
        // STL implementations. msvc for instance.

        std::ifstream fin ( path.c_str() );
        RCF_VERIFY(fin, Exception(_RcfError_FileOpen(path)));
        std::size_t begin = static_cast<std::size_t>(fin.tellg());
        fin.seekg (0, std::ios::end);
        std::size_t end = static_cast<std::size_t>(fin.tellg());
        fin.close();
        return end - begin;
    }

} // namespace RCF


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

#ifndef INCLUDE_UTIL_LOG_HPP
#define INCLUDE_UTIL_LOG_HPP

#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <boost/config.hpp>
#include <boost/current_function.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include <RCF/Export.hpp>
#include <RCF/ThreadLibrary.hpp>

#include <RCF/util/Tchar.hpp>
#include <RCF/util/VariableArgMacro.hpp>

#ifdef BOOST_WINDOWS
#include <Windows.h>
#endif

namespace RCF { 
   
    class ByteBuffer;

    class LogBuffers
    {
    public:
        RCF::MemOstream mTlsUserBuffer;
        RCF::MemOstream mTlsLoggerBuffer;
        RCF::MemOstream mTlsVarArgBuffer1;
        RCF::MemOstream mTlsVarArgBuffer2;
    };

    //******************************************************************************
    // LogManager

    class LogEntry;
    class Logger;

    typedef boost::shared_ptr<Logger> LoggerPtr;

    class RCF_EXPORT LogManager
    {
    private:
        LogManager();
        ~LogManager();

    public:
        static void init();
        static void deinit();
        static LogManager & instance();

        void deactivateAllLoggers();
        void deactivateAllLoggers(int name);

        bool isEnabled(int name, int level);

        typedef std::map< int, std::vector< LoggerPtr > > Loggers;
        ReadWriteMutex              mLoggersMutex;
        Loggers                     mLoggers;

        void writeToLoggers(const LogEntry & logEntry);
        void activateLogger(LoggerPtr loggerPtr);
        void deactivateLogger(LoggerPtr loggerPtr);
        bool isLoggerActive(LoggerPtr loggerPtr);

        const std::string   DefaultLogFormat;
        
        Mutex               DefaultLoggerPtrMutex;
        LoggerPtr           DefaultLoggerPtr;
    };

    //******************************************************************************
    // LogTarget

    class LogTarget;
    typedef boost::shared_ptr<LogTarget> LogTargetPtr;

    /// Base class for log targets. Log targets are passed to the enableLogging()
    /// function, to configure RCF logging.
    class RCF_EXPORT LogTarget
    {
    public:
        virtual ~LogTarget() {}
        virtual LogTarget * clone() const = 0;
        virtual void write(const RCF::ByteBuffer & output) = 0;
    };

    /// Configures log output to be directed to standard output.
    class RCF_EXPORT LogToStdout : public LogTarget
    {
    public:
        LogToStdout(bool flushAfterEachWrite = true);
        LogTarget * clone() const;
        void write(const RCF::ByteBuffer & output);

        static RCF::Mutex sIoMutex;

    private:
        bool mFlush;
    };

#ifdef BOOST_WINDOWS

    /// Configures log output to be directed to Windows debug output.
    class RCF_EXPORT LogToDebugWindow : public LogTarget
    {
    public:
        LogTarget * clone() const;
        void write(const RCF::ByteBuffer & output);
    };

    /// Configures log output to be directed to the Windows event log.
    class RCF_EXPORT LogToEventLog : public LogTarget
    {
    public:
        LogToEventLog(const std::string & appName, int eventLogLevel);
        ~LogToEventLog();

        LogTarget * clone() const;
        void write(const RCF::ByteBuffer & output);

    private:
        std::string mAppName;
        int mEventLogLevel;
        HANDLE mhEventLog;
    };

#endif

    /// Configures log output to be directed to a log file.
    class RCF_EXPORT LogToFile : public LogTarget
    {
    public:
        LogToFile(const std::string & filePath, bool flushAfterEachWrite = false);
        LogToFile(const LogToFile & rhs);
        LogTarget * clone() const;
        void write(const RCF::ByteBuffer & output);

    private:
        
        RCF::Mutex mMutex;

        std::string mFilePath;
        bool mOpened;
        std::ofstream mFout;
        bool mFlush;
    };

    typedef boost::function1<void, const RCF::ByteBuffer &> LogFunctor;

    class RCF_EXPORT LogToFunc : public LogTarget
    {
    public:
        LogToFunc(LogFunctor logFunctor);
        LogTarget * clone() const;
        void write(const RCF::ByteBuffer & output);

    private:
        LogFunctor mLogFunctor;
    };

    //******************************************************************************
    // LogEntry

    class RCF_EXPORT LogEntry
    {
    public:

        LogEntry(int name, int level);
        LogEntry(int name, int level, const char * szFile, int line, const char * szFunc);
        ~LogEntry();

        // Pass everything through to mOstream.
        template<typename T>
        const LogEntry& operator<<(const T& t) const
        {
            const_cast<RCF::MemOstream&>(*mpOstream) << t;
            return *this;
        }

#ifndef BOOST_NO_STD_WSTRING
        const LogEntry& operator<<(const std::wstring& t) const
        {
            const_cast<RCF::MemOstream&>(*mpOstream) << RCF::wstringToString(t);
            return *this;
        }
#endif

        // Special streaming for std::endl etc.
        typedef std::ostream& (*Pfn)(std::ostream&);
        const LogEntry& operator<<(Pfn pfn) const
        {
            const_cast<RCF::MemOstream&>(*mpOstream) << pfn;
            return *this;
        }

        RCF::MemOstream & getOstream()
        {
            return *mpOstream;
        }

    private:

        friend class LogManager;
        friend class Logger;

        int                 mName;
        int                 mLevel;
        const char *        mFile;
        int                 mLine;
        const char *        mFunc;

        RCF::ThreadId       mThreadId;
        time_t              mTime;
        boost::uint32_t     mTimeMs;

        RCF::MemOstream *  mpOstream;
    };

    //******************************************************************************
    // Logger

    typedef boost::function2<void, const LogEntry &, RCF::ByteBuffer&> LogFormatFunctor;

    class RCF_EXPORT Logger : public boost::enable_shared_from_this<Logger>
    {
    public:
        Logger(int name, int level, const LogTarget& logTarget, const std::string & logFormat = "");
        Logger(int name, int level, const LogTarget& logTarget, LogFormatFunctor logFormatFunctor);

        Logger(int name, int level, LogTargetPtr logTargetPtr, const std::string & logFormat = "");
        Logger(int name, int level, LogTargetPtr logTargetPtr, LogFormatFunctor logFormatFunctor);

        void setName(int name);
        void setLevel(int level);
        void setTarget(const LogTarget & logTarget);
        void setFormat(const std::string & logFormat);

        int getName() const;
        int getLevel() const;
        const LogTarget& getTarget() const;
        std::string getFormat() const;

        void write(const LogEntry & logEntry);

        void activate();
        void deactivate();
        bool isActive();

    private:

        int mName;
        int mLevel;
        LogTargetPtr mTargetPtr;
        std::string mFormat;
        LogFormatFunctor mFormatFunctor;
    };

    typedef boost::shared_ptr<Logger> LoggerPtr;

    template<typename T>
    class LogNameValue
    {
    public:
        LogNameValue(const char * name, const T & value) : 
            mName(name), 
            mValue(value)
        {
        }

        const char * mName;
        const T& mValue;

        friend std::ostream& operator<<(std::ostream & os, const LogNameValue& lnv)
        {
            os << "(" << lnv.mName << " = " << lnv.mValue << ")";
            return os;
        }
    };

    template<typename T>
    LogNameValue<T> makeNameValue(const char * name, const T & value)
    {
        return LogNameValue<T>(name, value);
    }

    #define NAMEVALUE(x) RCF::makeNameValue(#x, x)

    class LogVarsFunctor : public util::VariableArgMacroFunctor
    {
    public:

        LogVarsFunctor() : mLogEntry(0, 0, NULL, 0, NULL)
        {
        }

        LogVarsFunctor(int name, int level, const char * file, int line, const char * szFunc) :
            mLogEntry(name, level, file, line, szFunc)
        {
        }

        ~LogVarsFunctor()
        {
            if (mArgs->tellp() > 0)
            {
                mLogEntry << " [Args: ";
                mLogEntry.getOstream().write(mArgs->str(), mArgs->tellp());
                mLogEntry << "]";
            }
        }

        template<typename T>
        const LogVarsFunctor & operator<<(const T & t) const
        {
            const_cast<LogEntry &>(mLogEntry) << t;
            return *this;
        }

        typedef std::ostream& (*Pfn)(std::ostream&);
        const LogVarsFunctor & operator<<(Pfn pfn) const
        {
            const_cast<LogEntry &>(mLogEntry) << pfn;
            return *this;
        }

    private:
        LogEntry mLogEntry;
    };

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4355) // warning C4355: 'this' : used in base member initializer list
#endif

    DECLARE_VARIABLE_ARG_MACRO( UTIL_LOG, LogVarsFunctor );

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(__GNUC__) && (__GNUC__ < 3 || (__GNUC__ == 3 && __GNUC_MINOR__ < 4))
#define UTIL_LOG_GCC_33_HACK (const RCF::VariableArgMacro<RCF::LogVarsFunctor> &)
#else
#define UTIL_LOG_GCC_33_HACK
#endif

    #define UTIL_LOG(name, level)                                               \
        if (RCF::LogManager::instance().isEnabled(name, level))                \
            UTIL_LOG_GCC_33_HACK RCF::VariableArgMacro<RCF::LogVarsFunctor>(  \
                name, level, __FILE__, __LINE__, BOOST_CURRENT_FUNCTION)        \
                .cast( (RCF::VariableArgMacro<RCF::LogVarsFunctor> *) NULL )  \
                .UTIL_LOG_A

    #define UTIL_LOG_A(x)                         UTIL_LOG_OP(x, B)
    #define UTIL_LOG_B(x)                         UTIL_LOG_OP(x, A)
    #define UTIL_LOG_OP(x, next)                  UTIL_LOG_A.notify_((x), #x).UTIL_LOG_ ## next


#ifdef BOOST_WINDOWS
    typedef LogToDebugWindow DefaultLogTarget;
#else
    typedef LogToStdout DefaultLogTarget;
#endif

    /// Configures logging for the RCF runtime.
    /// logTarget is where the log output will appear.
    /// loglevel is the detail level of the log, ranging from 0 (no logging), 1 (errors-only logging) to 4 (maximum detail logging).
    /// logFormat is an optional parameter describing the format of the log output.
    RCF_EXPORT void enableLogging(
        const LogTarget &       logTarget = DefaultLogTarget(), 
        int                     logLevel = 2, 
        const std::string &     logFormat = "");

    /// Disables logging for the RCF runtime.
    RCF_EXPORT void disableLogging();

} // namespace RCF

#endif // ! INCLUDE_UTIL_LOG_HPP

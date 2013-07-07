
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

#ifndef INCLUDE_RCF_MEMSTREAM_HPP
#define INCLUDE_RCF_MEMSTREAM_HPP

#include <istream>
#include <streambuf>

// std::size_t for vc6
#include <boost/cstdint.hpp> 

#include <boost/noncopyable.hpp>

#include <RCF/Config.hpp>
#include <RCF/ByteBuffer.hpp>

namespace RCF {

    // mem_istreambuf

    class MemIstreamBuf :
        public std::streambuf, 
        boost::noncopyable   
    {   
      public:   
        MemIstreamBuf(char * buffer = NULL, std::size_t bufferLen = 0);
        ~MemIstreamBuf();
        void reset(char * buffer, std::size_t bufferLen);
           
      private:   
        std::streambuf::int_type underflow();   

        pos_type seekoff(
            off_type off, 
            std::ios_base::seekdir dir,
            std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out);
           
        char * mBuffer;
        std::size_t mBufferLen; 
    };   

    // mem_istream - a replacement for std::istrstream.

    class MemIstream : 
        public std::basic_istream<char>
    {   
    public:   
        MemIstream(const char * buffer = NULL, std::size_t bufferLen = 0);
        ~MemIstream();
        void reset(const char * buffer, std::size_t bufferLen);

    private:   

        MemIstreamBuf * mpBuf;
    };   

    // mem_ostreambuf

    class MemOstreamBuf :
        public std::streambuf, 
        boost::noncopyable   
    {   
    public:   
        MemOstreamBuf();
        ~MemOstreamBuf();

    private:   
        std::streambuf::int_type overflow(std::streambuf::int_type ch);

        pos_type seekoff(
            off_type off, 
            std::ios_base::seekdir dir,
            std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out);

        friend class MemOstream;
        ReallocBuffer mWriteBuffer;
    };   

    // mem_ostream - a replacement for std::ostrstream.

    class RCF_EXPORT MemOstream : 
        public std::basic_ostream<char>
    {   
    public:   
        MemOstream();
        ~MemOstream();

        char * str();

        std::size_t capacity();
        
    private:   

        MemOstreamBuf * mpBuf;
    };   

    typedef boost::shared_ptr<MemOstream> MemOstreamPtr;


} // namespace RCF

#endif

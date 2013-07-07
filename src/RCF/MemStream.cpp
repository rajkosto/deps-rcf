
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

#include <RCF/MemStream.hpp>

#include <RCF/Tools.hpp>

namespace RCF {

    // mem_istreambuf implementation

    MemIstreamBuf::MemIstreamBuf(   
        char * buffer, 
        std::size_t bufferLen) : 
            mBuffer(buffer),
            mBufferLen(bufferLen)
    {   
        setg(mBuffer, mBuffer, mBuffer + mBufferLen);
    }   

    MemIstreamBuf::~MemIstreamBuf()
    {
    }

    void MemIstreamBuf::reset(char * buffer, std::size_t bufferLen)
    {
        mBuffer = buffer;
        mBufferLen = bufferLen;
        setg(mBuffer, mBuffer, mBuffer + mBufferLen);
    }

    std::streambuf::int_type MemIstreamBuf::underflow()   
    {   
        if (gptr() < egptr())
        {
            return traits_type::to_int_type(*gptr());
        }

        return traits_type::eof();   
    }

    MemIstreamBuf::pos_type MemIstreamBuf::seekoff(
        MemIstreamBuf::off_type offset, 
        std::ios_base::seekdir dir,
        std::ios_base::openmode mode)
    {
        RCF_UNUSED_VARIABLE(mode);

        char * pBegin = mBuffer;
        char * pEnd = mBuffer + mBufferLen;
        
        char * pBase = NULL;
        switch(dir)
        {
            case std::ios::cur: pBase = gptr(); break;
            case std::ios::beg: pBase = pBegin; break;
            case std::ios::end: pBase = pEnd; break;
            default: assert(0); break; 
        }

        char * pNewPos = pBase + offset;
        if (pBegin <= pNewPos && pNewPos <= pEnd)
        {
            setg(pBegin, pNewPos, pEnd);
            return pNewPos - pBegin;
        }
        else
        {
            return pos_type(-1);
        }
    }

    // mem_istreambuf implementation

    MemOstreamBuf::MemOstreamBuf()
    {   
        mWriteBuffer.resize(10);

        setp( 
            &mWriteBuffer[0], 
            &mWriteBuffer[0] + mWriteBuffer.size());
    }

    MemOstreamBuf::~MemOstreamBuf()
    {
    }

    std::streambuf::int_type MemOstreamBuf::overflow(std::streambuf::int_type ch)
    {
        if (ch == traits_type::eof())
        {
            return traits_type::eof();
        }

        mWriteBuffer.resize( 2*mWriteBuffer.size() );
        
        std::size_t nextPos = pptr() - pbase();

        setp( 
            &mWriteBuffer[0],
            &mWriteBuffer[0] + mWriteBuffer.size());

        pbump( static_cast<int>(nextPos) );

        *pptr() = static_cast<char>(ch);
        pbump(1);
        return ch;
    }

    MemOstreamBuf::pos_type MemOstreamBuf::seekoff(
        MemOstreamBuf::off_type offset, 
        std::ios_base::seekdir dir,
        std::ios_base::openmode mode)
    {
        RCF_UNUSED_VARIABLE(mode);

        char * pBegin = pbase();
        char * pEnd = epptr();
        
        char * pBase = NULL;
        switch(dir)
        {
            case std::ios::cur: pBase = pptr(); break;
            case std::ios::beg: pBase = pBegin; break;
            case std::ios::end: pBase = pEnd; break;
            default: assert(0); break; 
        }

        char * pNewPos = pBase + offset;
        if (pBegin <= pNewPos && pNewPos <= pEnd)
        {
            setp(pBegin, pEnd);
            pbump( static_cast<int>(pNewPos - pBegin) );
            return pNewPos - pBegin;
        }
        else
        {
            return pos_type(-1);
        }
    }

    // mem_istream
    MemIstream::MemIstream(const char * buffer, std::size_t bufferLen) :
        std::basic_istream<char>(new MemIstreamBuf(const_cast<char *>(buffer), bufferLen))
    {   
        mpBuf = static_cast<MemIstreamBuf *>(rdbuf());
    }

    MemIstream::~MemIstream()
    {
        delete mpBuf;
    }

    void MemIstream::reset(const char * buffer, std::size_t bufferLen)
    {
        clear();
        mpBuf->reset(const_cast<char *>(buffer), bufferLen);
    }

    // mem_ostream
    MemOstream::MemOstream() :
        std::basic_ostream<char>(new MemOstreamBuf())
    {   
        mpBuf = static_cast<MemOstreamBuf *>(rdbuf());
    }

    MemOstream::~MemOstream()
    {
        delete mpBuf;
    }

    char * MemOstream::str()
    {
        return & mpBuf->mWriteBuffer[0];
    }

    std::size_t MemOstream::capacity()
    {
        return mpBuf->mWriteBuffer.capacity();
    }

} // namespace RCF

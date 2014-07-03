
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

#include <RCF/HttpFrameFilter.hpp>

#include <RCF/Exception.hpp>

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace RCF  {

    void splitString(
        const std::string & stringToSplit, 
        const std::string & splitAt, 
        std::vector<std::string> & lines)
    {
        std::size_t pos = 0;
        std::size_t posNext = 0;
        while (pos < stringToSplit.size() && pos != std::string::npos)
        {
            posNext = stringToSplit.find(splitAt, pos);
            if (posNext != std::string::npos)
            {
                lines.push_back( stringToSplit.substr(pos, posNext - pos) );
                pos = posNext + splitAt.size();
            }
        }
        eraseRemove(lines, std::string());
    }

    HttpFrameFilter::HttpFrameFilter() :
        mServerPort(0)
    {
        resetState();
    }

    HttpFrameFilter::HttpFrameFilter(const std::string serverAddr, int serverPort) :
        mServerAddr(serverAddr),
        mServerPort(serverPort)
    {
        resetState();
    }

    void HttpFrameFilter::resetState()
    {
        mWriteBuffers.clear();
        mWritePos = 0;
        //mBytesRequested = 0;
        mReadVectorPtr.reset( new std::vector<char>() );
        mBytesReceived = 0;
        mReadPos = 0;
        mHeaderLen = 0;
        mContentLen = 0;
        mOrigBytesRequested = 0;
    }

    void HttpFrameFilter::read(
        const ByteBuffer &byteBuffer,
        std::size_t bytesRequested)
    {
        if (bytesRequested == 0)
        {
            mpPostFilter->read(ByteBuffer(), bytesRequested);
        }
        else if (!mHeaderLen || mReadPos == mReadVectorPtr->size())
        {
            // Start reading a new HTTP message.
            mReadVectorPtr->resize(1024);
            mHeaderLen = 0;
            mContentLen = 0;
            mBytesReceived = 0;
            mReadPos = 0;
            mOrigReadBuffer = byteBuffer;
            mOrigBytesRequested = bytesRequested;

            mpPostFilter->read(
                ByteBuffer(mReadVectorPtr), 
                mReadVectorPtr->size());
        }
        else
        {
            // Copy from loaded HTTP message.
            std::size_t bytesRemainingInMessage = mHeaderLen + mContentLen - mReadPos;
            std::size_t bytesToReturn = RCF_MIN(bytesRemainingInMessage, bytesRequested);
            memcpy(byteBuffer.getPtr(), & (*mReadVectorPtr)[mReadPos], bytesToReturn);
            mReadPos += bytesToReturn;
            mpPreFilter->onReadCompleted(ByteBuffer(byteBuffer, 0, bytesToReturn));
        }
    }

    void HttpFrameFilter::onReadCompleted(const ByteBuffer & byteBuffer)
    {
        if (byteBuffer.isEmpty())
        {
            mpPreFilter->onReadCompleted(byteBuffer);
            return;
        }
        else if (mHeaderLen)
        {
            mBytesReceived += byteBuffer.getLength();
            std::size_t bytesRemainingInMessage = mReadVectorPtr->size() - mBytesReceived;
            if (bytesRemainingInMessage)
            {
                mpPostFilter->read( 
                    ByteBuffer(ByteBuffer(mReadVectorPtr), mBytesReceived, bytesRemainingInMessage),
                    bytesRemainingInMessage);
            }
            else
            {
                //mpPreFilter->onReadCompleted(ByteBuffer());
                mReadPos = mHeaderLen;
                read(mOrigReadBuffer, mOrigBytesRequested);
            }
        }
        else
        {
            RCF_ASSERT(mHeaderLen == 0);

            mBytesReceived += byteBuffer.getLength();

            // See if we can pick out the HTTP request header.
            // Scan bytes for CRLF CRLF to mark end of HTTP header.
            
            const char * szBuffer = & (*mReadVectorPtr)[0];
            std::size_t szBufferLen = mBytesReceived;
            const char * pChar = strstr(szBuffer, "\r\n\r\n");
            if (pChar)
            {
                mHeaderLen = pChar - szBuffer + 4;

                mRequestLine.clear();
                mResponseLine.clear();
                mHeaders.clear();

                // Split HTTP request into lines.
                std::string httpRequest(szBuffer, szBufferLen);
                std::string httpRequestHeader(szBuffer, mHeaderLen);
                std::vector<std::string> requestLines;
                splitString(httpRequestHeader, "\r\n", requestLines);

                // Parse request/response line.
                std::string firstLine = requestLines.front();
                if (boost::iequals(firstLine.substr(0, 4), "POST"))
                {
                    mRequestLine = requestLines.front();
                }
                else if (boost::iequals(firstLine.substr(0, 3), "GET"))
                {
                    mRequestLine = requestLines.front();
                }
                else if (boost::iequals(firstLine.substr(0, 5), "HTTP/"))
                {
                    mResponseLine = requestLines.front();
                }

                // Parse headers.
                for (std::size_t i=1; i<requestLines.size(); ++i)
                {
                    const std::string & line = requestLines[i];

                    std::size_t pos = line.find(':');
                    if (pos != std::string::npos)
                    {
                        std::string headerName = line.substr(0, pos);
                        std::string headerValue = line.substr(pos+1);
                        boost::trim_left(headerValue);
                        mHeaders[headerName] = headerValue;

                        // Is it the content-length header?
                        if (boost::iequals(headerName, "Content-Length"))
                        {
                            mContentLen = atoi(headerValue.c_str());
                            if (mContentLen)
                            {
                                mReadVectorPtr->resize(mHeaderLen + mContentLen);
                            }
                        }
                    }
                }
            }

            if (mHeaderLen == 0)
            {
                // Still don't have the header, go back for more.

                if (mBytesReceived > 10*1024)
                {
                    RCF_THROW(Exception(_RcfError_InvalidHttpMessage()));
                }

                if (mBytesReceived == mReadVectorPtr->size())
                {
                    mReadVectorPtr->resize(mReadVectorPtr->size() + 1024);
                }

                // Issue read.
                mpPostFilter->read(
                    ByteBuffer(ByteBuffer(mReadVectorPtr), mBytesReceived), 
                    mReadVectorPtr->size() - mBytesReceived);
            }
            else if (mHeaderLen > 0 && mContentLen == 0)
            {
                // Couldn't find the content length header.
                std::string httpMessage(szBuffer, szBufferLen);
                if (mResponseLine.size() > 0)
                {
                    RCF_THROW(Exception(_RcfError_HttpResponseContentLength(mResponseLine, httpMessage)));
                }
                else
                {
                    RCF_THROW(Exception(_RcfError_HttpRequestContentLength(httpMessage)));
                }
            }
            else if (mHeaderLen > 0 && mContentLen > 0)
            {
                std::size_t bytesRemainingInMessage = mReadVectorPtr->size() - mBytesReceived;
                if (bytesRemainingInMessage)
                {
                    mpPostFilter->read( 
                        ByteBuffer(ByteBuffer(mReadVectorPtr), mBytesReceived, bytesRemainingInMessage),
                        bytesRemainingInMessage);
                }
                else
                {
                    // Check HTTP error status.
                    if (mResponseLine.size() > 0)
                    {
                        std::size_t pos = mResponseLine.find("200");
                        if (pos == std::string::npos)
                        {
                            std::string httpMessage(szBuffer, szBufferLen);
                            RCF_THROW(Exception(_RcfError_HttpResponseContentLength(mResponseLine, httpMessage)));
                        }
                    }

                    //mpPreFilter->onReadCompleted(ByteBuffer());
                    mReadPos = mHeaderLen;
                    read(mOrigReadBuffer, mOrigBytesRequested);
                }
            }
        }
    }

    std::size_t HttpFrameFilter::getFrameSize()
    {
        return mContentLen;
    }

    void HttpFrameFilter::write(const std::vector<ByteBuffer> & byteBuffers)
    {
        mWriteBuffers = byteBuffers;
        mWritePos = 0;

        unsigned int messageLength = static_cast<unsigned int>(
            lengthByteBuffers(byteBuffers));

        MemOstream os;

        if (mServerAddr.size() > 0)
        {
            // Client-side request.

            // This needs to work whether or not we are going through a proxy.

            os <<  
                "POST http://" << mServerAddr << ":" << mServerPort << "/ HTTP/1.1\r\n"
                "Host: " << mServerAddr << "\r\n"
                "Accept: */*\r\n"
                "Connection: Keep-Alive\r\n"
                "Content-Length: " << messageLength << "\r\n"
                "\r\n";
        }
        else
        {
            // Server-side response.

            os <<  
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: " << messageLength << "\r\n"
                "\r\n";
        }

        std::string s = os.string();
        RCF::ByteBuffer httpResponseHeader(s);
        mWriteBuffers.insert(mWriteBuffers.begin(), httpResponseHeader);
        mpPostFilter->write(mWriteBuffers);
    }

    void HttpFrameFilter::onWriteCompleted(std::size_t bytesTransferred)
    {
        mWritePos += bytesTransferred;
        RCF_ASSERT(0 <= mWritePos && mWritePos <= lengthByteBuffers(mWriteBuffers));

        if (mWritePos < lengthByteBuffers(mWriteBuffers))
        {
            std::vector<ByteBuffer> slicedBuffers;
            sliceByteBuffers(slicedBuffers, mWriteBuffers, mWritePos);
            mpPostFilter->write(slicedBuffers);
        }
        else
        {
            std::size_t bytesWritten = mWritePos - mWriteBuffers.front().getLength();
            mpPreFilter->onWriteCompleted(bytesWritten);
        }
    }

    int HttpFrameFilter::getFilterId() const
    {
        return RcfFilter_Unknown;
    }   

} // namespace RCF

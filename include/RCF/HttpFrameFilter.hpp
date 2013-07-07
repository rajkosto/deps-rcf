
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

#ifndef INCLUDE_RCF_HTTPFRAMEFILTER_HPP
#define INCLUDE_RCF_HTTPFRAMEFILTER_HPP

#include <map>

#include <boost/shared_ptr.hpp>

#include <RCF/Filter.hpp>
#include <RCF/ByteBuffer.hpp>

namespace RCF {

    class HttpFrameFilter : public Filter
    {
    public:

        HttpFrameFilter();
        HttpFrameFilter(const std::string serverAddr, int serverPort);

        void resetState();

        void read(
            const ByteBuffer &byteBuffer,
            std::size_t bytesRequested);

        void write(const std::vector<ByteBuffer> &byteBuffers);

        void onReadCompleted(const ByteBuffer &byteBuffer);

        void onWriteCompleted(std::size_t bytesTransferred);

        int getFilterId() const;

        virtual std::size_t getFrameSize();

    private:

        std::string mServerAddr;
        int mServerPort;

        std::vector<ByteBuffer> mWriteBuffers;
        std::size_t mWritePos;

        ByteBuffer mOrigReadBuffer;
        std::size_t mOrigBytesRequested;

        boost::shared_ptr< std::vector<char> > mReadVectorPtr;
        std::size_t mBytesReceived;
        std::size_t mReadPos;

        std::size_t mHeaderLen;
        std::size_t mContentLen;

        std::string mRequestLine;
        std::string mResponseLine;
        std::map<std::string, std::string> mHeaders;
    };

} // namespace RCF

#endif // ! INCLUDE_RCF_HTTPFRAMEFILTER_HPP

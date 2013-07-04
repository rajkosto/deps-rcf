
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

#ifndef INCLUDE_RCF_OPENSSLENCRYPTIONFILTER_HPP
#define INCLUDE_RCF_OPENSSLENCRYPTIONFILTER_HPP

#include <memory>
#include <string>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include <RCF/Filter.hpp>
#include <RCF/Export.hpp>

typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct x509_st X509;

namespace RCF {

    // Enumeration describing the role in a SSL conversation that an endpoint is playing.
    enum SslRole
    {
        SslServer,
        SslClient
    };

    class OpenSslEncryptionFilter;
    class OpenSslEncryptionFilterImpl;

    typedef boost::function1<bool, OpenSslEncryptionFilter&> OpenSslCertificateValidationCb;

    class RCF_EXPORT PemCertificate : public I_Certificate
    {
    public:
        PemCertificate(const std::string & pathToCert, const std::string & password = "");

    private:
        friend class OpenSslEncryptionFilter;
        friend class OpenSslEncryptionFilterFactory;

        std::string mPathToCert;
        std::string mPassword;
    };

    class X509Certificate : public I_Certificate
    {
    public:
        X509Certificate(X509 * pX509) : mpX509(pX509)
        {
        }

        X509 * getX509()
        {
            return mpX509;
        }

    private:
        X509 * mpX509;
    };

    typedef boost::shared_ptr<X509Certificate> X509CertificatePtr;

    class ClientStub;

    class RCF_EXPORT OpenSslEncryptionFilter : public Filter, boost::noncopyable
    {
    public:
        // TODO: should be private
        static const FilterDescription *    spFilterDescription;
        static const FilterDescription &    sGetFilterDescription();
        const FilterDescription &           getFilterDescription() const;

    public:

        OpenSslEncryptionFilter(
            ClientStub *            pClientStub,
            SslRole                 sslRole = SslClient,
            unsigned int            bioBufferSize = 2048);

        OpenSslEncryptionFilter(
            const std::string &     certificateFile,
            const std::string &     certificateFilePassword,
            const std::string &     caCertificate,
            const std::string &     ciphers,
            OpenSslCertificateValidationCb verifyFunctor,
            SslRole                 sslRole = SslClient,
            unsigned int            bioBufferSize = 2048);

        void        resetState();
        void        read(const ByteBuffer &byteBuffer, std::size_t bytesRequested);
        void        write(const std::vector<ByteBuffer> &byteBuffers);
        void        onReadCompleted(const ByteBuffer &byteBuffer);
        void        onWriteCompleted(std::size_t bytesTransferred);

        SSL *       getSSL();
        SSL_CTX *   getCTX();

        CertificatePtr getPeerCertificate();

    private:
        friend class OpenSslEncryptionFilterImpl;
        boost::shared_ptr<OpenSslEncryptionFilterImpl> mImplPtr;
    };

    class RCF_EXPORT OpenSslEncryptionFilterFactory : public FilterFactory
    {
    public:
        OpenSslEncryptionFilterFactory();

        FilterPtr                   createFilter(RcfServer & server);
        const FilterDescription &   getFilterDescription();

    private:
        SslRole         mRole;
    };


} // namespace RCF

#endif // ! INCLUDE_RCF_OPENSSLENCRYPTIONFILTER_HPP

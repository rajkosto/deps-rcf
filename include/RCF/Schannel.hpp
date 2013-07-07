
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

#ifndef INCLUDE_RCF_SCHANNEL_HPP
#define INCLUDE_RCF_SCHANNEL_HPP

#include <RCF/Filter.hpp>
#include <RCF/SspiFilter.hpp>
#include <RCF/util/Tchar.hpp>

#include <schnlsp.h>

// missing stuff in mingw headers
#ifdef __MINGW32__
#ifndef SP_PROT_NONE
#define SP_PROT_NONE                    0
#endif
#endif // __MINGW32__

namespace RCF {

    static const ULONG DefaultSchannelContextRequirements = 
        ASC_REQ_SEQUENCE_DETECT |
        ASC_REQ_REPLAY_DETECT   |
        ASC_REQ_CONFIDENTIALITY |
        ASC_REQ_EXTENDED_ERROR  |
        ASC_REQ_ALLOCATE_MEMORY |
        ASC_REQ_STREAM;

    class SchannelServerFilter : public SspiServerFilter
    {
    public:
        SchannelServerFilter(
            RcfServer & server,
            DWORD enabledProtocols,
            ULONG contextRequirements);

        int getFilterId() const;
    };

    class SchannelFilterFactory : public FilterFactory
    {
    public:

        SchannelFilterFactory(
            DWORD enabledProtocols = SP_PROT_TLS1_SERVER,
            ULONG contextRequirements = DefaultSchannelContextRequirements);

        FilterPtr                           createFilter(RcfServer & server);
        int                                 getFilterId();

    private:
        
        ULONG mContextRequirements;
        DWORD mEnabledProtocols;
    };

    class SchannelClientFilter : public SspiClientFilter
    {
    public:
        SchannelClientFilter(
            ClientStub * pClientStub,
            DWORD enabledProtocols = SP_PROT_NONE,
            ULONG contextRequirements = DefaultSchannelContextRequirements);

        int getFilterId() const;
    };

    typedef SchannelClientFilter        SchannelFilter;

    // Certificate utility classes.

    class Win32Certificate;
    typedef boost::shared_ptr<Win32Certificate> Win32CertificatePtr;
    
    /// Represents an in-memory certificate, usually from a remote peer. Only applicable to Schannel.
    class RCF_EXPORT Win32Certificate : public Certificate
    {
    public:
        Win32Certificate();
        Win32Certificate(PCCERT_CONTEXT pContext);
        ~Win32Certificate();

        // *** SWIG BEGIN ***

        virtual CertificateImplementationType _getType()
        {
            return Cit_Win32;
        }

        /// Gets the name of the certificate.
        tstring getCertificateName();

        /// Gets the name of the issuer of the certificate.
        tstring getIssuerName();

        /// Exports the certificate in PFX format, to the given file path.
        void exportToPfx(const std::string & pfxFilePath);

        /// Attempts to locate a root certificate for this certificate, in the given certificate store.
        /// Returns the root certificate if found, and otherwise null.
        Win32CertificatePtr findRootCertificate(
            Win32CertificateLocation certStoreLocation, 
            Win32CertificateStore certStore);

        // *** SWIG END ***

        PCCERT_CONTEXT getWin32Context();

        

        void setHasBeenDeleted()
        {
            mHasBeenDeleted = true;
        }

        tstring getSubjectName();
        tstring getOrganizationName();
        tstring getCertAttribute(const char * whichAttr);

        RCF::ByteBuffer exportToPfx();

    protected:

        PCCERT_CONTEXT mpCert;
        bool mHasBeenDeleted;
    };

    
    /// Use this class to load a certificate from .pfx format. Only applicable to Schannel.
    class RCF_EXPORT PfxCertificate : public Win32Certificate
    {
    public:

        // *** SWIG BEGIN ***

        /// Loads a certificate from a .pfx file, using the given file path, password and certificate name.
        PfxCertificate(
            const std::string & pathToCert, 
            const tstring & password,
            const tstring & certName);

        /// Adds the certificate to the given Windows certificate store.
        void addToStore(
            Win32CertificateLocation certStoreLocation, 
            Win32CertificateStore certStore);

        // *** SWIG END ***

        PfxCertificate(
            ByteBuffer certPfxBlob, 
            const tstring & password,
            const tstring & certName);

        ~PfxCertificate();

    private:

        void init(
            ByteBuffer pfxBlob,
            const tstring & password,
            const tstring & certName);

        void initFromFile(
            const std::string & pathToCert, 
            const RCF::tstring & password,
            const RCF::tstring & certName);

        HCERTSTORE mPfxStore;
    };

    /// Represents a certificate in a Windows certificate store.
    class RCF_EXPORT StoreCertificate  : public Win32Certificate
    {
    public:

        // *** SWIG BEGIN ***

        /// Loads a certificate from a certificate store.
        StoreCertificate(
            Win32CertificateLocation certStoreLocation, 
            Win32CertificateStore certStore,
            const tstring & certName);

        /// Removes the certificate from the store it was loaded from.
        void removeFromStore();

        // *** SWIG END ***

        ~StoreCertificate();

    private:
        HCERTSTORE mStore;
    };

    /// Iterates over the certificates in a Windows certificate store.
    class RCF_EXPORT StoreCertificateIterator
    {
    public:

        // *** SWIG BEGIN ***

        /// Constructs a store iterator for the the given certificate location and store.
        StoreCertificateIterator(
            Win32CertificateLocation certStoreLocation, 
            Win32CertificateStore certStore);

        /// Moves to the next certificate in the store. Returns false if there are no more certificates, and true otherwise.
        bool moveNext();

        /// Resets the iterator back to the beginning of the store.
        void reset();

        /// Returns the current certificate.
        Win32CertificatePtr current();

        // *** SWIG END ***

        ~StoreCertificateIterator();

    private:

        HCERTSTORE              mhCertStore;
        PCCERT_CONTEXT          mpCertIterator;
        Win32CertificatePtr     mCurrentCertPtr;
    };

} // namespace RCF

#endif // ! INCLUDE_RCF_SCHANNEL_HPP

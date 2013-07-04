
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

    class RCF_EXPORT SchannelServerFilter : public SspiServerFilter
    {
    public:
        SchannelServerFilter(
            RcfServer & server,
            DWORD enabledProtocols,
            ULONG contextRequirements);

        const FilterDescription &           getFilterDescription() const;
    };

    class RCF_EXPORT SchannelFilterFactory : public FilterFactory
    {
    public:

        SchannelFilterFactory(
            DWORD enabledProtocols = SP_PROT_TLS1_SERVER,
            ULONG contextRequirements = DefaultSchannelContextRequirements);

        FilterPtr                           createFilter(RcfServer & server);
        const FilterDescription &           getFilterDescription();

    private:
        
        ULONG mContextRequirements;
        DWORD mEnabledProtocols;
    };

    class RCF_EXPORT SchannelClientFilter : public SspiClientFilter
    {
    public:
        SchannelClientFilter(
            ClientStub * pClientStub,
            DWORD enabledProtocols = SP_PROT_NONE,
            ULONG contextRequirements = DefaultSchannelContextRequirements);

        const FilterDescription &           getFilterDescription() const;
    };

    typedef SchannelClientFilter        SchannelFilter;

    // Certificate utility classes.

    class Win32Certificate;
    typedef boost::shared_ptr<Win32Certificate> Win32CertificatePtr;
    
    class RCF_EXPORT Win32Certificate : public I_Certificate
    {
    public:
        Win32Certificate();
        Win32Certificate(PCCERT_CONTEXT pContext);
        ~Win32Certificate();

        tstring getCertAttribute(const char * whichAttr);
        tstring getSubjectName();
        tstring getOrganizationName();

        PCCERT_CONTEXT getWin32Context();

        // Validate against the Root store. If ok, returns handle to the root certificate, otherwise null.
        Win32CertificatePtr isCertificateValid();

        void setHasBeenDeleted()
        {
            mHasBeenDeleted = true;
        }

    protected:

        PCCERT_CONTEXT mpCert;
        bool mHasBeenDeleted;
    };

    

    class RCF_EXPORT PfxCertificate : public Win32Certificate
    {
    public:

        PfxCertificate(
            const std::string & pathToCert, 
            const tstring & password,
            const tstring & certName);

        PfxCertificate(
            ByteBuffer certPfxBlob, 
            const tstring & password,
            const tstring & certName);

        ~PfxCertificate();

        void addToStore(
            DWORD dwFlags, 
            const std::string & storeName);

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

    class RCF_EXPORT StoreCertificate  : public Win32Certificate
    {
    public:

        StoreCertificate(
            DWORD dwStoreFlags,
            const std::string & storeName,
            const tstring & certName);

        ~StoreCertificate();

        void removeFromStore();
        RCF::ByteBuffer exportToPfx();

    private:
        HCERTSTORE mStore;
    };

} // namespace RCF

#endif // ! INCLUDE_RCF_SCHANNEL_HPP

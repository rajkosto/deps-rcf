
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

#include <RCF/Schannel.hpp>

#include <RCF/Exception.hpp>
#include <RCF/RcfServer.hpp>
#include <RCF/SspiFilter.hpp>
#include <RCF/ThreadLocalData.hpp>
#include <RCF/Tools.hpp>

#include <wincrypt.h>
#include <schnlsp.h>

#ifdef __MINGW32__
#ifndef CERT_STORE_ADD_USE_EXISTING
#define CERT_STORE_ADD_USE_EXISTING                         2
#endif
#endif // __MINGW32__

namespace RCF {

    PSecurityFunctionTable getSft();

    void SspiFilter::encryptWriteBufferSchannel()
    {
        // encrypt the pre buffer to the write buffer

        RCF_ASSERT_EQ(mContextState , AuthOkAck);

        SecPkgContext_Sizes sizes;
        getSft()->QueryContextAttributes(
            &mContext,
            SECPKG_ATTR_SIZES,
            &sizes);

        SecPkgContext_StreamSizes streamSizes;
        getSft()->QueryContextAttributes(
            &mContext,
            SECPKG_ATTR_STREAM_SIZES,
            &streamSizes);

        DWORD cbHeader          = streamSizes.cbHeader;
        DWORD cbMsg             = static_cast<DWORD>(mWriteByteBufferOrig.getLength());
        DWORD cbTrailer         = streamSizes.cbTrailer;
        DWORD cbPacket          = cbHeader + cbMsg + cbTrailer;

        resizeWriteBuffer(cbPacket);
        
        memcpy(
            mWriteBuffer+cbHeader,
            mWriteByteBufferOrig.getPtr(),
            mWriteByteBufferOrig.getLength());

        BYTE *pEncryptedMsg     =((BYTE *) mWriteBuffer);

        SecBuffer rgsb[4]       = {0};
        rgsb[0].cbBuffer        = cbHeader;
        rgsb[0].BufferType      = SECBUFFER_STREAM_HEADER;
        rgsb[0].pvBuffer        = pEncryptedMsg;

        rgsb[1].cbBuffer        = cbMsg;
        rgsb[1].BufferType      = SECBUFFER_DATA;
        rgsb[1].pvBuffer        = pEncryptedMsg + cbHeader;

        rgsb[2].cbBuffer        = cbTrailer;
        rgsb[2].BufferType      = SECBUFFER_STREAM_TRAILER;
        rgsb[2].pvBuffer        = pEncryptedMsg + cbHeader + cbMsg;

        rgsb[3].cbBuffer        = 0;
        rgsb[3].BufferType      = SECBUFFER_EMPTY;
        rgsb[3].pvBuffer        = NULL;

        SecBufferDesc sbd       = {0};
        sbd.ulVersion           = SECBUFFER_VERSION;
        sbd.cBuffers            = sizeof(rgsb)/sizeof(*rgsb);
        sbd.pBuffers            = rgsb;

        SECURITY_STATUS status = getSft()->EncryptMessage(
            &mContext,
            0,
            &sbd,
            0);

        RCF_VERIFY(
            status == SEC_E_OK,
            FilterException(
                _RcfError_SspiEncrypt("EncryptMessage()"),
                status,
                RcfSubsystem_Os))(status);

        RCF_ASSERT_EQ(rgsb[0].cbBuffer , cbHeader);
        RCF_ASSERT_EQ(rgsb[1].cbBuffer , cbMsg);
        RCF_ASSERT_LTEQ(rgsb[2].cbBuffer , cbTrailer);

        cbTrailer               = rgsb[2].cbBuffer;
        cbPacket                = cbHeader + cbMsg + cbTrailer;
        resizeWriteBuffer(cbPacket);
    }

    bool SspiFilter::decryptReadBufferSchannel()
    {
        // decrypt read buffer in place

        RCF_ASSERT_EQ(mContextState , AuthOkAck);

        BYTE *pMsg              = ((BYTE *) mReadBuffer);
        DWORD cbMsg             = static_cast<DWORD>(mReadBufferPos);
        SecBuffer rgsb[4]       = {0};
        
        rgsb[0].cbBuffer        = cbMsg;
        rgsb[0].BufferType      = SECBUFFER_DATA;
        rgsb[0].pvBuffer        = pMsg;

        rgsb[1].cbBuffer        = 0;
        rgsb[1].BufferType      = SECBUFFER_EMPTY;
        rgsb[1].pvBuffer        = NULL;

        rgsb[2].cbBuffer        = 0;
        rgsb[2].BufferType      = SECBUFFER_EMPTY;
        rgsb[2].pvBuffer        = NULL;

        rgsb[3].cbBuffer        = 0;
        rgsb[3].BufferType      = SECBUFFER_EMPTY;
        rgsb[3].pvBuffer        = NULL;

        SecBufferDesc sbd       = {0};
        sbd.ulVersion           = SECBUFFER_VERSION;
        sbd.cBuffers            = sizeof(rgsb)/sizeof(*rgsb);
        sbd.pBuffers            = rgsb;
        ULONG qop               = 0;

        SECURITY_STATUS status  = getSft()->DecryptMessage(
            &mContext,
            &sbd,
            0,
            &qop);

        if (status == SEC_E_INCOMPLETE_MESSAGE)
        {
            // Not enough data.
            std::size_t readBufferPos = mReadBufferPos;
            resizeReadBuffer(mReadBufferPos + mReadAheadChunkSize);
            mReadBufferPos = readBufferPos;
            readBuffer();
            return false;
        }
        else
        {
            for (int i=1; i<4; ++i)
            {
                if (rgsb[i].BufferType == SECBUFFER_EXTRA)
                {
                    // Found extra data - set a marker where it begins.
                    char * pRemainingData = (char *) rgsb[i].pvBuffer;
                    mRemainingDataPos = pRemainingData - mReadBuffer;
                    RCF_ASSERT(0 < mRemainingDataPos && mRemainingDataPos < mReadBufferPos);
                    break;
                }
            }            
        }

        trimReadBuffer();

        RCF_VERIFY(
            status == SEC_E_OK,
            FilterException(
                _RcfError_SspiDecrypt("DecryptMessage()"),
                status,
                RcfSubsystem_Os))(status);

        RCF_ASSERT_EQ(rgsb[0].BufferType , SECBUFFER_STREAM_HEADER);
        RCF_ASSERT_EQ(rgsb[1].BufferType , SECBUFFER_DATA);
        RCF_ASSERT_EQ(rgsb[2].BufferType , SECBUFFER_STREAM_TRAILER);

        DWORD cbHeader          = rgsb[0].cbBuffer;
        DWORD cbData            = rgsb[1].cbBuffer;
        DWORD cbTrailer         = rgsb[2].cbBuffer;

        RCF_UNUSED_VARIABLE(cbTrailer);

        mReadBufferPos          = cbHeader;
        mReadBufferLen          = cbHeader + cbData;

        return true;
    }

    bool SspiServerFilter::doHandshakeSchannel()
    {
        // use the block in the read buffer to proceed through the handshake procedure

        // lazy acquiring of implicit credentials
        if (mImplicitCredentials && !mHaveCredentials)
        {
            acquireCredentials();
        }

        DWORD cbPacket          = mPkgInfo.cbMaxToken;

        SecBuffer ob            = {0};
        ob.BufferType           = SECBUFFER_TOKEN;
        ob.cbBuffer             = 0;
        ob.pvBuffer             = NULL;

        SecBufferDesc obd       = {0};
        obd.cBuffers            = 1;
        obd.ulVersion           = SECBUFFER_VERSION;
        obd.pBuffers            = &ob;
        
        SecBuffer ib[2]         = {0};
        ib[0].BufferType        = SECBUFFER_TOKEN;
        ib[0].cbBuffer          = static_cast<DWORD>(mReadBufferPos);
        ib[0].pvBuffer          = mReadBuffer;
        ib[1].BufferType        = SECBUFFER_EMPTY;
        ib[1].cbBuffer          = 0;
        ib[1].pvBuffer          = NULL;

        SecBufferDesc ibd       = {0};
        ibd.cBuffers            = 2;
        ibd.ulVersion           = SECBUFFER_VERSION;
        ibd.pBuffers            = ib;

        DWORD contextRequirements = mContextRequirements;
        if (mCertValidationCallback || mAutoCertValidation.size() > 0)
        {
            // Need this to get the client to send the server a certificate.
            contextRequirements |= ASC_REQ_MUTUAL_AUTH;
        }

        DWORD   CtxtAttr        = 0;
        TimeStamp Expiration    = {0};
        SECURITY_STATUS status  = getSft()->AcceptSecurityContext(
            &mCredentials,
            mHaveContext ? &mContext : NULL,
            &ibd,
            contextRequirements,
            SECURITY_NATIVE_DREP,
            &mContext,
            &obd,
            &CtxtAttr,
            &Expiration);

        switch (status)
        {
        case SEC_E_OK:
        case SEC_I_CONTINUE_NEEDED:
        case SEC_I_COMPLETE_NEEDED:
        case SEC_I_COMPLETE_AND_CONTINUE:
            mHaveContext = true;
            break;
        default:
            break;
        }

        cbPacket = ob.cbBuffer;

        RCF_ASSERT(
            status != SEC_I_COMPLETE_AND_CONTINUE &&
            status != SEC_I_COMPLETE_NEEDED)
            (status);

        if (status == SEC_E_INCOMPLETE_MESSAGE)
        {
            // Not enough data.
            std::size_t readBufferPos = mReadBufferPos;
            resizeReadBuffer(mReadBufferPos + mReadAheadChunkSize);
            mReadBufferPos = readBufferPos;
            readBuffer();
            return false;
        }
        else if (ib[1].BufferType == SECBUFFER_EXTRA)
        {
            // We consider any extra data at this stage to be a protocol error.
            Exception e(_RcfError_SspiHandshakeExtraData());
            RCF_THROW(e);
        }

        trimReadBuffer();
        
        if (status == SEC_I_CONTINUE_NEEDED)
        {
            // Authorization ok so far, copy outbound data to write buffer.
            resizeWriteBuffer(ob.cbBuffer);
            memcpy(mWriteBuffer, ob.pvBuffer, ob.cbBuffer);
            getSft()->FreeContextBuffer(ob.pvBuffer);
        }
        else if (status == SEC_E_OK)
        {
            // Authorization ok, send last handshake block to the client.
            mContextState = AuthOk;
            RCF_ASSERT_GT(cbPacket , 0);
            resizeWriteBuffer(cbPacket);
            memcpy(mWriteBuffer, ob.pvBuffer, ob.cbBuffer);
            getSft()->FreeContextBuffer(ob.pvBuffer);

            // Extract the peer certificate.
            PCCERT_CONTEXT pRemoteCertContext = NULL;

            status = getSft()->QueryContextAttributes(
                &mContext,
                SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                (PVOID)&pRemoteCertContext);

            if (pRemoteCertContext)
            {
                mRemoteCert.reset( new Win32Certificate(pRemoteCertContext) );
            }

            // If we have a custom validation callback, call it.
            if (mCertValidationCallback)
            {
                mCertValidationCallback(*this);
            }
        }
        else
        {
            // Authorization failed. Do nothing here, the connection will automatically close.
        }

        return true;
    }

    bool SspiClientFilter::doHandshakeSchannel()
    {
        // use the block in the read buffer to proceed through the handshake procedure

        // lazy acquiring of implicit credentials
        if (mImplicitCredentials && !mHaveCredentials)
        {
            acquireCredentials();
        }
     
        SecBuffer ob            = {0};
        ob.BufferType           = SECBUFFER_TOKEN;
        ob.cbBuffer             = 0;
        ob.pvBuffer             = NULL;

        SecBufferDesc obd       = {0};
        obd.cBuffers            = 1;
        obd.ulVersion           = SECBUFFER_VERSION;
        obd.pBuffers            = &ob;

        SecBuffer ib[2]         = {0};
        
        ib[0].BufferType        = SECBUFFER_TOKEN;
        ib[0].cbBuffer          = static_cast<DWORD>(mReadBufferPos);
        ib[0].pvBuffer          = mReadBuffer;

        ib[1].BufferType        = SECBUFFER_EMPTY;
        ib[1].cbBuffer          = 0;
        ib[1].pvBuffer          = NULL;

        SecBufferDesc ibd       = {0};
        ibd.cBuffers            = 2;
        ibd.ulVersion           = SECBUFFER_VERSION;
        ibd.pBuffers            = ib;

        tstring strTarget       = mAutoCertValidation;
        const TCHAR *target     = strTarget.empty() ? RCF_T("") : strTarget.c_str();
        DWORD CtxtAttr          = 0;
        TimeStamp Expiration    = {0};

        DWORD contextRequirements = mContextRequirements;
        if (mLocalCert && mLocalCert->getWin32Context())
        {
            contextRequirements |= ISC_REQ_USE_SUPPLIED_CREDS;
        }

        SECURITY_STATUS status  = getSft()->InitializeSecurityContext(
            &mCredentials,
            mHaveContext ? &mContext : NULL,
            (TCHAR *) target,
            mContextRequirements,
            0,
            SECURITY_NATIVE_DREP,
            mHaveContext ? &ibd : NULL,
            0,
            &mContext,
            &obd,
            &CtxtAttr,
            &Expiration);

        switch (status)
        {
        case SEC_E_OK:
        case SEC_I_CONTINUE_NEEDED:
        case SEC_I_COMPLETE_NEEDED:
        case SEC_I_COMPLETE_AND_CONTINUE:
        case SEC_I_INCOMPLETE_CREDENTIALS:
            mHaveContext = true;
            break;
        default:
            break;
        }

        RCF_ASSERT(
            status != SEC_I_COMPLETE_NEEDED &&
            status != SEC_I_COMPLETE_AND_CONTINUE)
            (status);

        if (status == SEC_E_INCOMPLETE_MESSAGE)
        {
            // Not enough data.
            std::size_t readBufferPos = mReadBufferPos;            
            resizeReadBuffer(mReadBufferPos + mReadAheadChunkSize);
            mReadBufferPos = readBufferPos;
            readBuffer();
            return false;
        }
        else if (ib[1].BufferType == SECBUFFER_EXTRA)
        {
            // We consider any extra data at this stage to be a protocol error.
            Exception e(_RcfError_SspiHandshakeExtraData());
            RCF_THROW(e);
        }

        trimReadBuffer();
            
        if (status == SEC_I_CONTINUE_NEEDED)
        {
            // Handshake OK so far.

            RCF_ASSERT(ob.cbBuffer);
            mContextState = AuthContinue;
            resizeWriteBuffer(ob.cbBuffer);
            memcpy(mWriteBuffer, ob.pvBuffer, ob.cbBuffer);
            getSft()->FreeContextBuffer(ob.pvBuffer);
            return true;
        }
        else if (status == SEC_E_OK)
        {
            // Handshake OK.

            // Extract the peer certificate.
            PCCERT_CONTEXT pRemoteCertContext = NULL;

            status = getSft()->QueryContextAttributes(
                &mContext,
                SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                (PVOID)&pRemoteCertContext);

            mRemoteCert.reset( new Win32Certificate(pRemoteCertContext) );

            // If we have a custom validation callback, call it.
            if (mCertValidationCallback)
            {
                mCertValidationCallback(*this);
            }

            // And now back to business.
            mContextState = AuthOkAck;
            resumeUserIo();
            return false;
        }
        else
        {
            Exception e(_RcfError_SspiAuthFailClient(), status);
            RCF_THROW(e);
            return false;
        }
    }

    void SspiFilter::setupCredentialsSchannel()
    {
        SCHANNEL_CRED schannelCred          = {0};       
        schannelCred.dwVersion              = SCHANNEL_CRED_VERSION;
        PCCERT_CONTEXT pCertContext         = NULL;
        if(mLocalCert)
        {
            pCertContext                    = mLocalCert->getWin32Context();
            schannelCred.cCreds             = 1;
            schannelCred.paCred             = &pCertContext;
        }

        schannelCred.grbitEnabledProtocols  = mEnabledProtocols;

        if (mServer)
        {
            if (mCertValidationCallback)
            {
                // Server side manual validation.
                schannelCred.dwFlags            = SCH_CRED_MANUAL_CRED_VALIDATION;
            }
            else
            {
                // Server side auto validation.
                schannelCred.dwFlags            = 0;
            }
        }
        else
        {
            if (mCertValidationCallback)
            {
                // Client side manual validation.
                schannelCred.dwFlags            = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION;
            }
            else
            {
                // Client side auto validation.
                schannelCred.dwFlags            = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_AUTO_CRED_VALIDATION;
            }
        }

        SECURITY_STATUS status = getSft()->AcquireCredentialsHandle(
            NULL,
            UNISP_NAME,
            mServer ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND ,
            NULL,
            &schannelCred,
            NULL, 
            NULL,
            &mCredentials,
            NULL);

        if (status != SEC_E_OK)
        {
            FilterException e(
                _RcfError_Sspi("AcquireCredentialsHandle()"), 
                status, 
                RcfSubsystem_Os);

            RCF_THROW(e)(mPkgInfo.Name)(status);
        }

        mHaveCredentials = true;
    }

    SchannelServerFilter::SchannelServerFilter(
        RcfServer & server,
        DWORD enabledProtocols,
        ULONG contextRequirements) :
            SspiServerFilter(UNISP_NAME, RCF_T(""), BoolSchannel)
    {
        CertificatePtr certificatePtr = server.getSslCertificate();
        Win32CertificatePtr certificateBasePtr = boost::dynamic_pointer_cast<Win32Certificate>(certificatePtr);
        if (certificateBasePtr)
        {
            mLocalCert = certificateBasePtr;
        }

        mCertValidationCallback = server.getSchannelCertificateValidationCb();
        mAutoCertValidation = server.getSchannelDefaultCertificateValidation();

        mContextRequirements = contextRequirements;
        mEnabledProtocols = enabledProtocols;
    }

    SchannelFilterFactory::SchannelFilterFactory(
        DWORD enabledProtocols,
        ULONG contextRequirements) :
            mContextRequirements(contextRequirements),
            mEnabledProtocols(enabledProtocols)/*,
            mEnableClientCertificateValidation(false)*/
    {
    }

    FilterPtr SchannelFilterFactory::createFilter(RcfServer & server)
    {
        boost::shared_ptr<SchannelServerFilter> filterPtr(
            new SchannelServerFilter(
                server,
                mEnabledProtocols,
                mContextRequirements));

        return filterPtr;
    }

    SchannelClientFilter::SchannelClientFilter(
        ClientStub *            pClientStub,
        DWORD                   enabledProtocols,
        ULONG                   contextRequirements) :
            SspiClientFilter(
                pClientStub,
                Encryption, 
                contextRequirements, 
                UNISP_NAME, 
                RCF_T(""),
                BoolSchannel)
    {
        mEnabledProtocols = enabledProtocols;

        CertificatePtr certificatePtr = pClientStub->getSslCertificate();
        Win32CertificatePtr certificateBasePtr = boost::dynamic_pointer_cast<Win32Certificate>(certificatePtr);
        if (certificateBasePtr)
        {
            mLocalCert = certificateBasePtr;
        }

        mCertValidationCallback = pClientStub->getSchannelCertificateValidationCb();
        mAutoCertValidation = pClientStub->getSchannelDefaultCertificateValidation();
    }

    Win32CertificatePtr SspiFilter::getPeerCertificate()
    {
        return mRemoteCert;
    }    

#if defined(__MINGW32__) && (__GNUC__ <= 3)

    // PfxCertificate and StoreCertificate have not been implemented for mingw gcc 3.4 and earlier.
    // A number of Cert* and PFX* functions seem to be missing from the mingw headers, and there
    // are issues with std::wstring.

#else

    // Certificate utility classes.

    PfxCertificate::PfxCertificate(
        const std::string & pathToCert, 
        const RCF::tstring & password,
        const RCF::tstring & certName) : 
            mPfxStore(NULL)
    {
        initFromFile(pathToCert, password, certName);
    }

    PfxCertificate::PfxCertificate(
        ByteBuffer pfxBlob, 
        const tstring & password,
        const tstring & certName) :
            mPfxStore(NULL)
    {
        init(pfxBlob, password, certName);
    }

    void PfxCertificate::initFromFile(
        const std::string & pathToCert, 
        const RCF::tstring & password,
        const RCF::tstring & certName)
    {
        std::size_t fileSize = static_cast<std::size_t>(RCF::fileSize(pathToCert));
        ByteBuffer pfxBlob(fileSize);

        std::ifstream fin( pathToCert.c_str() , std::ios::binary);
        fin.read(pfxBlob.getPtr(), pfxBlob.getLength());
        std::size_t bytesRead = static_cast<std::size_t>(fin.gcount());
        fin.close();
        RCF_ASSERT_EQ(bytesRead , fileSize);

        init(pfxBlob, password, certName);
    }

    void PfxCertificate::init(
        ByteBuffer pfxBlob, 
        const tstring & password,
        const tstring & certName)
    {
        CRYPT_DATA_BLOB blob = {0};
        blob.cbData   = static_cast<DWORD>(pfxBlob.getLength());
        blob.pbData   = (BYTE*) pfxBlob.getPtr();

        BOOL recognizedPFX = PFXIsPFXBlob(&blob);
        DWORD dwErr = GetLastError();


        RCF_VERIFY(
            recognizedPFX, 
            Exception(_RcfError_ApiError("PFXIsPFXBlob()"), dwErr));

        std::wstring wPassword = RCF::toWstring(password);

        // For Windows 98, the flag CRYPT_MACHINE_KEYSET is not valid.
        mPfxStore = PFXImportCertStore(
            &blob, 
            wPassword.c_str(),
            CRYPT_MACHINE_KEYSET | CRYPT_EXPORTABLE);

        dwErr = GetLastError();

        RCF_VERIFY(
            mPfxStore, 
            Exception(_RcfError_ApiError("PFXImportCertStore()"), dwErr));

        PCCERT_CONTEXT pCertStore = NULL;

        // Tempting to use CERT_FIND_ANY for the case where there is just a single
        // certificate in the PFX file. However, doing so appears to not load the
        // private key information. So we use CERT_FIND_SUBJECT_STR instead, and
        // require the certificate name to be passed in.
        
        DWORD dwFindType = CERT_FIND_SUBJECT_STR;
        std::wstring wCertName = RCF::toWstring(certName);
        const void * pvFindPara = wCertName.c_str();

        pCertStore = CertFindCertificateInStore(
            mPfxStore, 
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 
            0,
            dwFindType,
            pvFindPara,
            pCertStore);

        dwErr = GetLastError();

        RCF_VERIFY(
            pCertStore, 
            Exception(_RcfError_ApiError("CertFindCertificateInStore()"), dwErr));

        BOOL bFreeHandle;
        HCRYPTPROV hProv;
        DWORD dwKeySpec;

        BOOL bResult = CryptAcquireCertificatePrivateKey(
            pCertStore, 
            0, 
            NULL, 
            &hProv, 
            &dwKeySpec, 
            &bFreeHandle);

        dwErr = GetLastError();

        RCF_VERIFY(
            bResult, 
            Exception(_RcfError_ApiError("CryptAcquireCertificatePrivateKey()"), dwErr));

        mpCert = pCertStore; 
    }

    void PfxCertificate::addToStore(
        DWORD dwFlags, 
        const std::string & storeName)
    {
        std::wstring wStoreName(storeName.begin(), storeName.end());

        HCERTSTORE hCertStore = CertOpenStore(
            (LPCSTR) CERT_STORE_PROV_SYSTEM,
            X509_ASN_ENCODING,
            0,
            dwFlags,
            wStoreName.c_str());

        DWORD dwErr = GetLastError();

        RCF_VERIFY(
            hCertStore, 
            RCF::Exception(
                _RcfError_CryptoApiError("CertOpenStore()"), 
                dwErr, 
                RCF::RcfSubsystem_Os));

        BOOL ret = CertAddCertificateContextToStore(
            hCertStore,
            mpCert,
            CERT_STORE_ADD_USE_EXISTING,
            NULL);

        dwErr = GetLastError();

        RCF_VERIFY(
            ret, 
            RCF::Exception(
                _RcfError_CryptoApiError("CertAddCertificateContextToStore()"), 
                dwErr, 
                RCF::RcfSubsystem_Os));

        CertCloseStore(hCertStore, 0);
    }

    Win32Certificate::Win32Certificate() : 
        mpCert(NULL), 
        mHasBeenDeleted(false)
    {
    }

    Win32Certificate::Win32Certificate(PCCERT_CONTEXT pContext) :
        mpCert(pContext), 
        mHasBeenDeleted(false)
    {
    }

    Win32Certificate::~Win32Certificate()
    {
        if (mpCert && !mHasBeenDeleted)
        {
            CertFreeCertificateContext(mpCert);
            mpCert = NULL;
        }
    }

    tstring Win32Certificate::getSubjectName()
    {
        return getCertAttribute(szOID_COMMON_NAME);
    }

    tstring Win32Certificate::getOrganizationName()
    {
        return getCertAttribute(szOID_ORGANIZATION_NAME);
    }

    tstring Win32Certificate::getCertAttribute(const char * whichAttr)
    {
        PCCERT_CONTEXT pContext = getWin32Context();

        // How much space do we need.
        DWORD cbNameInfo = 0;

        DWORD dwRet = CryptDecodeObject(
            X509_ASN_ENCODING,
            X509_NAME,
            pContext->pCertInfo->Subject.pbData,
            pContext->pCertInfo->Subject.cbData,
            CRYPT_DECODE_NOCOPY_FLAG,
            NULL,
            &cbNameInfo);

        DWORD dwErr = GetLastError();

        RCF_VERIFY(
            dwRet, 
            RCF::Exception(
                _RcfError_CryptoApiError("CryptDecodeObject()"), 
                dwErr, 
                RCF::RcfSubsystem_Os));

        std::vector<char> vec(cbNameInfo);

        // Retrieve name info.
        dwRet = CryptDecodeObject(X509_ASN_ENCODING,
            X509_NAME,
            pContext->pCertInfo->Subject.pbData,
            pContext->pCertInfo->Subject.cbData,
            CRYPT_DECODE_NOCOPY_FLAG,
            &vec[0],
            &cbNameInfo);

        dwErr = GetLastError();

        RCF_VERIFY(
            dwRet, 
            RCF::Exception(
                _RcfError_CryptoApiError("CryptDecodeObject()"), 
                dwErr, 
                RCF::RcfSubsystem_Os));

        PCERT_NAME_INFO pCertNameInfo = (PCERT_NAME_INFO) &vec[0];

        PCERT_RDN_ATTR pCertAttr = CertFindRDNAttr(whichAttr, pCertNameInfo);
        if (pCertAttr)
        {
            DWORD cbLen = CertRDNValueToStr(pCertAttr->dwValueType, &pCertAttr->Value, NULL, 0);
            std::vector<TCHAR> vec(cbLen);

            CertRDNValueToStr(
                pCertAttr->dwValueType, 
                &pCertAttr->Value, 
                &vec[0], 
                static_cast<DWORD>(vec.size()));

            tstring attr(&vec[0]);
            return attr;
        }
        
        return tstring();
    }

    Win32CertificatePtr Win32Certificate::isCertificateValid()
    {
        Win32CertificatePtr issuerCertPtr;

        std::wstring storeName = L"Root";

        HCERTSTORE hCertStore = CertOpenStore(
            (LPCSTR) CERT_STORE_PROV_SYSTEM,
            X509_ASN_ENCODING,
            0,
            CERT_SYSTEM_STORE_LOCAL_MACHINE,
            storeName.c_str());

        DWORD dwErr = GetLastError();

        RCF_VERIFY(
            hCertStore, 
            Exception(_RcfError_ApiError("CertOpenStore()"), dwErr));

        PCCERT_CONTEXT pSubjectContext = getWin32Context();

        {
            DWORD           dwFlags;
            PCCERT_CONTEXT  pIssuerContext;

            pSubjectContext = CertDuplicateCertificateContext(pSubjectContext);
            if (pSubjectContext)
            {
                do 
                {
                    dwFlags = 
                            CERT_STORE_REVOCATION_FLAG 
                        |   CERT_STORE_SIGNATURE_FLAG 
                        |   CERT_STORE_TIME_VALIDITY_FLAG;

                    pIssuerContext = CertGetIssuerCertificateFromStore(
                        hCertStore,
                        pSubjectContext, 
                        0, 
                        &dwFlags);

                    if (pIssuerContext) 
                    {
                        CertFreeCertificateContext(pSubjectContext);
                        pSubjectContext = pIssuerContext;
                        if (dwFlags & CERT_STORE_NO_CRL_FLAG)
                        {
                            // No CRL list available. Proceed anyway.
                            dwFlags &= ~(CERT_STORE_NO_CRL_FLAG | CERT_STORE_REVOCATION_FLAG);
                        }
                        if (dwFlags) 
                        {
                            if ( dwFlags & CERT_STORE_TIME_VALIDITY_FLAG)
                            {
                                // Certificate is expired.
                                // ...
                            }
                            break;
                        }
                    } 
                    else if (GetLastError() == CRYPT_E_SELF_SIGNED) 
                    {
                        // Got the root certificate.
                        issuerCertPtr.reset( new Win32Certificate(pSubjectContext) );
                    }
                } 
                while (pIssuerContext);
            }
        }

        CertCloseStore(hCertStore, 0);

        return issuerCertPtr;
    }

    PfxCertificate::~PfxCertificate()
    {
        CertCloseStore(mPfxStore, 0);
    }

    PCCERT_CONTEXT Win32Certificate::getWin32Context()
    {
        return mpCert;
    }

    StoreCertificate::StoreCertificate(
        DWORD dwStoreFlags, 
        const std::string & storeName,
        const tstring & certName) :
            mStore(0)
    {
        std::wstring wStoreName(storeName.begin(), storeName.end());

        mStore = CertOpenStore(
            (LPCSTR) CERT_STORE_PROV_SYSTEM,
            X509_ASN_ENCODING,
            0,
            dwStoreFlags,
            &wStoreName[0]);

        DWORD dwErr = GetLastError();

        RCF_VERIFY(
            mStore, 
            RCF::Exception(
                _RcfError_CryptoApiError("CertOpenStore()"), 
                dwErr, 
                RCF::RcfSubsystem_Os));

        std::wstring wCertName(certName.begin(), certName.end());

        DWORD dwFindType = CERT_FIND_SUBJECT_STR;

        PCCERT_CONTEXT pStoreCert = CertFindCertificateInStore(
            mStore, 
            X509_ASN_ENCODING, 
            0,
            dwFindType,
            wCertName.c_str(),
            NULL);

        dwErr = GetLastError();

        RCF_VERIFY(
            pStoreCert, 
            RCF::Exception(
            _RcfError_CryptoApiError("CertFindCertificateInStore()"), 
            dwErr, 
            RCF::RcfSubsystem_Os));

        mpCert = pStoreCert;
    }

    void StoreCertificate::removeFromStore()
    {
        if (mpCert)
        {
            BOOL ret = CertDeleteCertificateFromStore(mpCert);
            DWORD dwErr = GetLastError();

            RCF_VERIFY(
                ret, 
                RCF::Exception(
                    _RcfError_CryptoApiError("CertDeleteCertificateFromStore()"), 
                    dwErr, 
                    RCF::RcfSubsystem_Os));

            setHasBeenDeleted();
            mpCert = NULL;
        }
    }

    RCF::ByteBuffer StoreCertificate::exportToPfx()
    {
        PCCERT_CONTEXT pContext = getWin32Context();

        // Create in-memory store
        HCERTSTORE  hMemoryStore;

        hMemoryStore = CertOpenStore(
            CERT_STORE_PROV_MEMORY,    // Memory store
            0,                         // Encoding type, not used with a memory store
            NULL,                      // Use the default provider
            0,                         // No flags
            NULL);                     // Not needed

        DWORD dwErr = GetLastError();

        RCF_VERIFY(
            hMemoryStore, 
            Exception(_RcfError_ApiError("CertOpenStore()"), dwErr));

        // Add the certificate.
        BOOL ok = CertAddCertificateContextToStore(
            hMemoryStore,                // Store handle
            pContext,                   // Pointer to a certificate
            CERT_STORE_ADD_USE_EXISTING,
            NULL);

        dwErr = GetLastError();

        RCF_VERIFY(
            ok, 
            Exception(_RcfError_ApiError("CertAddCertificateContextToStore()"), dwErr));

        // Export in-memory store.
        CRYPT_DATA_BLOB pfxBlob = {};
        BOOL exportOk = PFXExportCertStore(hMemoryStore, &pfxBlob, L"", 0/*EXPORT_PRIVATE_KEYS*/);

        dwErr = GetLastError();

        RCF_VERIFY(
            exportOk, 
            Exception(_RcfError_ApiError("PFXExportCertStore()"), dwErr));

        RCF::ByteBuffer pfxBuffer(pfxBlob.cbData);
        pfxBlob.pbData = (BYTE *) pfxBuffer.getPtr();

        exportOk = PFXExportCertStore(hMemoryStore, &pfxBlob, L"", 0/*EXPORT_PRIVATE_KEYS*/);
        
        dwErr = GetLastError();

        RCF_VERIFY(
            exportOk, 
            Exception(_RcfError_ApiError("PFXExportCertStore()"), dwErr));

        CertCloseStore(hMemoryStore, 0);

        return pfxBuffer;
    }

    StoreCertificate::~StoreCertificate()
    {
        CertCloseStore(mStore, 0);
    }

#endif

} // namespace RCF

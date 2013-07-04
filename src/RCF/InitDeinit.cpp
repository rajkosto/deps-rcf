
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

#include <RCF/InitDeinit.hpp>

#include <RCF/Config.hpp>
#include <RCF/ThreadLibrary.hpp>
#include <RCF/AmiThreadPool.hpp>

#include <RCF/Filter.hpp>

// std::size_t
#include <cstdlib>

// std::size_t for vc6
#include <boost/config.hpp>

#include <csignal>

namespace RCF {

    extern AmiThreadPool * gpAmiThreadPool;

    static Mutex gInitRefCountMutex;
    static std::size_t gInitRefCount = 0;

    void initAmiHandlerCache();
    void initFileIoThreadPool();
    void initAmi();
    void initNamedPipeEndpointSerialization();
    void initObjectPool();
    void initOpenSslEncryptionFilterDescription();
    void initPerformanceData();
    void SspiInitialize();
    void initSspiFilterDescriptions();
    void initThreadLocalData();
    void initTpHandlerCache();
    void initWinsock();
    void initOpenSsl();
    void initZlibCompressionFilterDescriptions();
    void initRegistrySingleton();
    void initTcpEndpointSerialization();
    void initUdpEndpointSerialization();
    void initUnixLocalEndpointSerialization();
    void initWin32NamedPipeEndpointSerialization();
    void initLogManager();
    void initPfnGetUserName();

    void deinitAmiHandlerCache();
    void deinitFileIoThreadPool();
    void deinitAmi();
    void deinitNamedPipeEndpointSerialization();
    void deinitObjectPool();
    void deinitOpenSslEncryptionFilterDescription();
    void deinitPerformanceData();
    void SspiUninitialize();
    void deinitSspiFilterDescriptions();
    void deinitThreadLocalData();
    void deinitTpHandlerCache();
    void deinitWinsock();
    void deinitOpenSsl();
    void deinitZlibCompressionFilterDescriptions();
    void deinitRegistrySingleton();
    void deinitTcpEndpointSerialization();
    void deinitUdpEndpointSerialization();
    void deinitUnixLocalEndpointSerialization();
    void deinitWin32NamedPipeEndpointSerialization();
    void deinitLogManager();
    void deinitPfnGetUserName();

    bool init()
    {
        Lock lock(getRootMutex());
        if (gInitRefCount == 0)
        {
            // General initialization.

            RCF::getCurrentTimeMs();
            initAmiHandlerCache();
            IdentityFilter::spFilterDescription = new FilterDescription("identity filter", RcfFilter_Identity, true);
            XorFilter::spFilterDescription = new FilterDescription("Xor filter", RcfFilter_Xor, true);
            initFileIoThreadPool();
            initAmi();
            
            #if defined(BOOST_WINDOWS)
            initNamedPipeEndpointSerialization();
            #endif

            initObjectPool();
            initPerformanceData();
            

            //#if defined(sun) || defined(__sun) || defined(__sun__)
            //if (!pThreadLocalDataPtr) pThreadLocalDataPtr = new ThreadLocalDataPtr;
            //#endif

            initThreadLocalData();
            initTpHandlerCache();

            #ifdef RCF_USE_OPENSSL
            initOpenSsl();
            initOpenSslEncryptionFilterDescription();
            #endif

            #ifdef RCF_USE_ZLIB
            initZlibCompressionFilterDescriptions();
            #endif

            initLogManager();

            #ifdef RCF_USE_SF_SERIALIZATION
            initRegistrySingleton();
            initTcpEndpointSerialization();
            initUdpEndpointSerialization();
            
            #if defined(BOOST_WINDOWS)
            initWin32NamedPipeEndpointSerialization();            
            #endif

            #ifdef RCF_HAS_LOCAL_SOCKETS
            initUnixLocalEndpointSerialization();
            #endif

            #endif // RCF_USE_SF_SERIALIZATION


            #ifdef BOOST_WINDOWS
            initWinsock();
            initPfnGetUserName();
            SspiInitialize();
            #endif

#ifndef BOOST_WINDOWS
            // Disable broken pipe signals.
            std::signal(SIGPIPE, SIG_IGN);
#endif
            
            // Start the AMI thread pool.
            gpAmiThreadPool = new AmiThreadPool(); 
            gpAmiThreadPool->start();
        }

        ++gInitRefCount;
        return gInitRefCount == 1;
    }

    bool deinit()
    {
        Lock lock(getRootMutex());
        --gInitRefCount;
        if (gInitRefCount == 0)
        {
            // Stop the AMI thread pool.
            gpAmiThreadPool->stop(); 
            delete gpAmiThreadPool; 
            gpAmiThreadPool = NULL;

            // General deinitialization.
            deinitAmiHandlerCache();
            delete IdentityFilter::spFilterDescription; IdentityFilter::spFilterDescription = NULL;
            delete XorFilter::spFilterDescription; XorFilter::spFilterDescription = NULL;
            deinitFileIoThreadPool();
            deinitAmi();
            deinitObjectPool();

            #ifdef RCF_USE_OPENSSL
            deinitOpenSslEncryptionFilterDescription();
            #endif

            deinitPerformanceData();
            deinitThreadLocalData();
            deinitTpHandlerCache();

            #ifdef RCF_USE_ZLIB
            deinitZlibCompressionFilterDescriptions();
            #endif

            #ifdef BOOST_WINDOWS
            SspiUninitialize();
            deinitPfnGetUserName();
            deinitWinsock();
            #endif

#ifdef RCF_USE_SF_SERIALIZATION
            deinitRegistrySingleton();
#endif

            deinitLogManager();         
        }
        return gInitRefCount == 0;
    }

    RcfInitDeinit::RcfInitDeinit() : mIsRootInstance(false)
    {
        mIsRootInstance = init();
    }

    RcfInitDeinit::~RcfInitDeinit()
    {
        deinit();
    }

    bool RcfInitDeinit::isRootInstance()
    {
        return mIsRootInstance;
    }

#ifdef RCF_AUTO_INIT_DEINIT
    RcfInitDeinit gRcfAutoInit;
#endif

} // namespace RCF

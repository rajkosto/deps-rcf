
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

#include <boost/config.hpp>
#include <boost/version.hpp>

#include <RCF/Config.hpp>

// Problems with BSer. Include valarray early so it doesn't get trampled by min/max macro definitions.
#if defined(_MSC_VER) && _MSC_VER == 1310 && defined(RCF_USE_BOOST_SERIALIZATION)
#include <valarray>
#endif

// Problems with BSer. Suppress some static warnings.
#if defined(_MSC_VER) && defined(RCF_USE_BOOST_SERIALIZATION) && BOOST_VERSION >= 104100
#pragma warning( push )
#pragma warning( disable : 4308 )  // warning C4308: negative integral constant converted to unsigned type
#endif

#ifndef RCF_CPP_WHICH_SECTION
#define RCF_CPP_WHICH_SECTION 0
#endif

#if RCF_CPP_WHICH_SECTION == 0 || RCF_CPP_WHICH_SECTION == 1

#include "AmiThreadPool.cpp"
#include "BsdClientTransport.cpp"
#include "ByteBuffer.cpp"
#include "ByteOrdering.cpp"
#include "CheckRtti.cpp"
#include "ClientStub.cpp"
#include "ClientTransport.cpp"
#include "ConnectionOrientedClientTransport.cpp"
#include "CurrentSerializationProtocol.cpp"
#include "CurrentSession.cpp"
#include "CustomAllocator.cpp"
#include "Endpoint.cpp"
#include "Exception.cpp"
#include "FileIoThreadPool.cpp"
#include "Filter.cpp"
#include "FilterService.cpp"

#endif // RCF_CPP_WHICH_SECTION == 1

#if RCF_CPP_WHICH_SECTION == 0 || RCF_CPP_WHICH_SECTION == 2

#include "HttpEndpoint.cpp"
#include "HttpClientTransport.cpp"
#include "HttpServerTransport.cpp"
#include "HttpsEndpoint.cpp"
#include "HttpsClientTransport.cpp"
#include "HttpsServerTransport.cpp"
#include "HttpFrameFilter.cpp"
#include "HttpConnectFilter.cpp"

#include "InitDeinit.cpp"
#include "InProcessEndpoint.cpp"
#include "InProcessTransport.cpp"
#include "IpAddress.cpp"
#include "IpClientTransport.cpp"
#include "IpServerTransport.cpp"
#include "Marshal.cpp"
#include "MemStream.cpp"
#include "MethodInvocation.cpp"
#include "MulticastClientTransport.cpp"
#include "ObjectFactoryService.cpp"
#include "ObjectPool.cpp"
#include "PerformanceData.cpp"
#include "PeriodicTimer.cpp"
#include "PingBackService.cpp"
#include "PublishingService.cpp"
#include "RcfClient.cpp"
#include "RcfServer.cpp"
#include "RcfSession.cpp"
#include "ReallocBuffer.cpp"
#include "SerializationProtocol.cpp"
#include "ServerInterfaces.cpp"
#include "ServerStub.cpp"
#include "ServerTask.cpp"
#include "ServerTransport.cpp"
#include "Service.cpp"
#include "SessionObjectFactoryService.cpp"
#include "SessionTimeoutService.cpp"
#include "StubEntry.cpp"
#include "StubFactory.cpp"
#include "SubscriptionService.cpp"
#include "TcpClientTransport.cpp"
#include "TcpEndpoint.cpp"

#endif // RCF_CPP_WHICH_SECTION == 2

#if RCF_CPP_WHICH_SECTION == 0 || RCF_CPP_WHICH_SECTION == 3

#include "ThreadLibrary.cpp"
#include "ThreadLocalData.cpp"
#include "ThreadPool.cpp"
#include "TimedBsdSockets.cpp"
#include "Timer.cpp"
#include "Token.cpp"
#include "Tools.cpp"
#include "UdpClientTransport.cpp"
#include "UdpEndpoint.cpp"
#include "UdpServerTransport.cpp"
#include "UsingBsdSockets.cpp"
#include "Version.cpp"

#include "util/Log.cpp"
#include "util/Platform.cpp"

#include "AsioHandlerCache.cpp"
#include "AsioServerTransport.cpp"
#include "TcpAsioServerTransport.cpp"

#include <RCF/Asio.hpp>

#ifdef RCF_HAS_LOCAL_SOCKETS
#include "UnixLocalServerTransport.cpp"
#include "UnixLocalClientTransport.cpp"
#include "UnixLocalEndpoint.cpp"
#endif

#if defined(BOOST_WINDOWS) || defined(RCF_HAS_LOCAL_SOCKETS)
#include "NamedPipeEndpoint.cpp"
#endif

#if defined(BOOST_WINDOWS)

#include "Schannel.cpp"
#include "SspiFilter.cpp"
#include "Win32NamedPipeClientTransport.cpp"
#include "Win32NamedPipeEndpoint.cpp"
#include "Win32NamedPipeServerTransport.cpp"

#include "Win32Username.cpp"

#endif

#ifdef RCF_USE_OPENSSL
#include "OpenSslEncryptionFilter.cpp"
#include "UsingOpenSsl.cpp"
#endif

#ifdef RCF_USE_ZLIB
#include "ZlibCompressionFilter.cpp"
#endif

#ifdef RCF_USE_SF_SERIALIZATION
#include "../SF/SF.cpp"
#else
#include "../SF/Encoding.cpp"
#endif

#ifdef RCF_USE_BOOST_FILESYSTEM
#include "FileTransferService.cpp"
#include "FileStream.cpp"
#endif

#ifdef RCF_USE_JSON
#include "JsonRpc.cpp"
#endif

#if defined(BOOST_WINDOWS)
#include "ChildProcess.cpp"
#endif

// Don't support UTF-8 conversion on mingw or borland. mingw doesn't have std::wstring,
// and borland chokes on the UTF-8 codecvt from Boost.

#ifndef BOOST_NO_STD_WSTRING

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4244)
#endif

#include <RCF/utf8/convert.cpp>
#include <RCF/utf8/utf8_codecvt_facet.cpp>
#include <RCF/thread/impl/thread_src.cpp>

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#endif

#endif // RCF_CPP_WHICH_SECTION == 3

#if defined(_MSC_VER) && defined(RCF_USE_BOOST_SERIALIZATION) && BOOST_VERSION >= 104100
#pragma warning( pop )
#endif

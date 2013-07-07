
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

//
// detail/impl/win_thread.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2011 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef RCF_DETAIL_IMPL_WIN_THREAD_IPP
#define RCF_DETAIL_IMPL_WIN_THREAD_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#if defined(BOOST_WINDOWS) && !defined(UNDER_CE)

#include <process.h>
#include "RCF/thread/win_thread.hpp"
#include "RCF/thread/push_options.hpp"

namespace RCF {
namespace detail {

win_thread::~win_thread()
{
  ::CloseHandle(thread_);

  // The exit_event_ handle is deliberately allowed to leak here since it
  // is an error for the owner of an internal thread not to join() it.
}

void win_thread::join()
{
  HANDLE handles[2] = { exit_event_, thread_ };
  ::WaitForMultipleObjects(2, handles, FALSE, INFINITE);
  ::CloseHandle(exit_event_);
}

void win_thread::start_thread(func_base* arg, unsigned int stack_size)
{
  ::HANDLE entry_event = 0;
  arg->entry_event_ = entry_event = ::CreateEvent(0, true, false, 0);
  if (!entry_event)
  {
    DWORD last_error = ::GetLastError();
    delete arg;
    RCF_VERIFY(last_error == 0, Exception(_RcfError_ThreadingError("CreateEvent()"), last_error));
  }

  arg->exit_event_ = exit_event_ = ::CreateEvent(0, true, false, 0);
  if (!exit_event_)
  {
    DWORD last_error = ::GetLastError();
    delete arg;
    RCF_VERIFY(last_error == 0, Exception(_RcfError_ThreadingError("CreateEvent()"), last_error));
  }

  unsigned int thread_id = 0;
  thread_ = reinterpret_cast<HANDLE>(::_beginthreadex(0,
        stack_size, win_thread_function, arg, 0, &thread_id));
  if (!thread_)
  {
    DWORD last_error = ::GetLastError();
    delete arg;
    if (entry_event)
      ::CloseHandle(entry_event);
    if (exit_event_)
      ::CloseHandle(exit_event_);
    RCF_VERIFY(last_error == 0, Exception(_RcfError_ThreadingError("CloseHandle()"), last_error));
  }

  if (entry_event)
  {
    ::WaitForSingleObject(entry_event, INFINITE);
    ::CloseHandle(entry_event);
  }
}

unsigned int __stdcall win_thread_function(void* arg)
{
  std::auto_ptr<win_thread::func_base> func(
      static_cast<win_thread::func_base*>(arg));

  ::SetEvent(func->entry_event_);

  func->run();

  // Signal that the thread has finished its work, but rather than returning go
  // to sleep to put the thread into a well known state. If the thread is being
  // joined during global object destruction then it may be killed using
  // TerminateThread (to avoid a deadlock in DllMain). Otherwise, the SleepEx
  // call will be interrupted using QueueUserAPC and the thread will shut down
  // cleanly.
  HANDLE exit_event = func->exit_event_;
  func.reset();
  ::SetEvent(exit_event);

  return 0;
}

} // namespace detail
} // namespace RCF

#include "RCF/thread/pop_options.hpp"

#endif // defined(BOOST_WINDOWS) && !defined(UNDER_CE)

#endif // RCF_DETAIL_IMPL_WIN_THREAD_IPP

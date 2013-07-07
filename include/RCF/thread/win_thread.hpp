
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
// detail/win_thread.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2011 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef RCF_DETAIL_WIN_THREAD_HPP
#define RCF_DETAIL_WIN_THREAD_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#if defined(BOOST_WINDOWS) && !defined(UNDER_CE)

#include <RCF/thread/push_options.hpp>

namespace RCF {
namespace detail {

RCF_EXPORT unsigned int __stdcall win_thread_function(void* arg);

class RCF_EXPORT  win_thread
  : private noncopyable/*,
    public win_thread_base<win_thread>*/
{
public:
  // Constructor.
  template <typename Function>
  win_thread(Function f, unsigned int stack_size = 0)
    : thread_(0),
      exit_event_(0)
  {
    start_thread(new func<Function>(f), stack_size);
  }

  // Destructor.
  ~win_thread();

  // Wait for the thread to exit.
  void join();

private:
  friend RCF_EXPORT unsigned int __stdcall win_thread_function(void* arg);

  class func_base
  {
  public:
    virtual ~func_base() {}
    virtual void run() = 0;
    ::HANDLE entry_event_;
    ::HANDLE exit_event_;
  };

  struct auto_func_base_ptr
  {
    func_base* ptr;
    ~auto_func_base_ptr() { delete ptr; }
  };

  template <typename Function>
  class func
    : public func_base
  {
  public:
    func(Function f)
      : f_(f)
    {
    }

    virtual void run()
    {
      f_();
    }

  private:
    Function f_;
  };

  void start_thread(func_base* arg, unsigned int stack_size);

  ::HANDLE thread_;
  ::HANDLE exit_event_;
};

} // namespace detail
} // namespace RCF

#include <RCF/thread/pop_options.hpp>

#endif // defined(BOOST_WINDOWS) && !defined(UNDER_CE)

#endif // RCF_DETAIL_WIN_THREAD_HPP

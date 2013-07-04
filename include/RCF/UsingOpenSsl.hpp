
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

#ifndef INCLUDE_RCF_USINGOPENSSL_HPP
#define INCLUDE_RCF_USINGOPENSSL_HPP

#include <string>

#include <RCF/Export.hpp>

namespace RCF {

    // Calling ERR_print_errors_fp() crashes the whole app for some reason,
    // so call my_ERR_print_errors_fp() instead,
    // it does the exact same thing.

    int                         my_print_fp(
                                    const char *str, 
                                    size_t len, 
                                    void *fp);

    void                        my_ERR_print_errors_fp(FILE *fp);
    std::string                 getOpenSslErrors();
    
    void                        initOpenSsl();

} // namespace RCF

#endif // ! INCLUDE_RCF_USINGOPENSSL_HPP

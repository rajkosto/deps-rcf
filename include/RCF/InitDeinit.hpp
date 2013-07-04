
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

#ifndef INCLUDE_RCF_INITDEINIT_HPP
#define INCLUDE_RCF_INITDEINIT_HPP

#include <RCF/Export.hpp>

namespace RCF {

    // Ref-counted initialization of RCF framework.
    RCF_EXPORT bool init();

    // Ref-counted deinitialization of RCF framework.
    RCF_EXPORT bool deinit();

    // Initialization sentry. Ctor calls RCF::init(), dtor calls RCF::deinit().
    class RCF_EXPORT RcfInitDeinit
    {
    public:
        RcfInitDeinit();
        ~RcfInitDeinit();

        // Returns true if this instance initialized RCF.
        bool isRootInstance();

    private:
        bool mIsRootInstance;
    };

} // namespace RCF

#endif // ! INCLUDE_RCF_INITDEINIT_HPP

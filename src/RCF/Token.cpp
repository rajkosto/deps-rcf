
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

#include <RCF/Token.hpp>

#ifdef RCF_USE_SF_SERIALIZATION
#include <SF/Archive.hpp>
#endif

#include <iostream>

namespace RCF {

    //*****************************************
    // Token

    Token::Token() :
        mId()
    {}

    bool operator<(const Token &lhs, const Token &rhs)
    {
        return (lhs.getId() < rhs.getId());
    }

    bool operator==(const Token &lhs, const Token &rhs)
    {
        return lhs.getId() == rhs.getId();
    }

    bool operator!=(const Token &lhs, const Token &rhs)
    {
        return ! (lhs == rhs);
    }

    int Token::getId() const
    {
        return mId;
    }
   
    std::ostream &operator<<(std::ostream &os, const Token &token)
    {
        os << "( id = " << token.getId() << " )";
        return os;
    }

    Token::Token(int id) :
        mId(id)
    {}

#ifdef RCF_USE_SF_SERIALIZATION

    void Token::serialize(SF::Archive &ar)
    {
        ar & mId;
    }

#endif

    // TokenFactory

    TokenFactory::TokenFactory(int tokenCount) :
        mMutex(WriterPriority)
    {
        for (int i=0; i<tokenCount; i++)
        {
            mTokenSpace.push_back( Token(i+1) );
        }

        mAvailableTokens.assign( mTokenSpace.rbegin(), mTokenSpace.rend() );
    }

    bool TokenFactory::requestToken(Token &token)
    {
        WriteLock writeLock(mMutex);
        RCF_UNUSED_VARIABLE(writeLock);
        if (mAvailableTokens.empty())
        {
            RCF_LOG_1()(mAvailableTokens.size())(mTokenSpace.size()) 
                << "TokenFactory - no more tokens available.";

            return false;
        }
        else
        {
            Token myToken = mAvailableTokens.back();
            mAvailableTokens.pop_back();
            token = myToken;
            return true;
        }
    }

    void TokenFactory::returnToken(const Token &token)
    {
        // TODO: perhaps should verify that the token is part of the token 
        // space as well...
        if (token != Token())
        {
            WriteLock writeLock(mMutex);
            RCF_UNUSED_VARIABLE(writeLock);
            mAvailableTokens.push_back(token);
        }
    }

    const std::vector<Token> &TokenFactory::getTokenSpace()
    {
        return mTokenSpace;
    }

    std::size_t TokenFactory::getAvailableTokenCount()
    {
        ReadLock readLock( mMutex );
        RCF_UNUSED_VARIABLE(readLock);
        return mAvailableTokens.size();
    }

    bool TokenFactory::isAvailable(const Token & token)
    {
        ReadLock readLock( mMutex );
        return 
            std::find(mAvailableTokens.begin(), mAvailableTokens.end(), token) 
            != mAvailableTokens.end();
    }
   
} // namespace RCF

//------------------------------------------------------------------------------
/*
    This file is part of divvyd: https://github.com/xdv/divvyd
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2011 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef RIPPLE_CRYPTO_CAUTOBN_CTX_H_INCLUDED
#define RIPPLE_CRYPTO_CAUTOBN_CTX_H_INCLUDED

#include <openssl/bn.h>

#include <stdexcept>
#include <string>

namespace divvy {

class CAutoBN_CTX
{
protected:
    BN_CTX* pctx;

public:
    CAutoBN_CTX ()
    {
        pctx = BN_CTX_new ();

        if (pctx == nullptr)
            throw std::runtime_error ("CAutoBN_CTX : BN_CTX_new() returned nullptr");
    }

    ~CAutoBN_CTX ()
    {
        if (pctx != nullptr)
            BN_CTX_free (pctx);
    }

    CAutoBN_CTX (CAutoBN_CTX const&) = delete;
    CAutoBN_CTX& operator= (CAutoBN_CTX const&) = delete;

    operator BN_CTX* ()
    {
        return pctx;
    }
    BN_CTX& operator* ()
    {
        return *pctx;
    }
    BN_CTX** operator& ()
    {
        return &pctx;
    }
    bool operator! ()
    {
        return (pctx == nullptr);
    }
};

}

#endif

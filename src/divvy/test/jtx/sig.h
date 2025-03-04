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

#ifndef RIPPLE_TEST_JTX_SIG_H_INCLUDED
#define RIPPLE_TEST_JTX_SIG_H_INCLUDED

#include <divvy/test/jtx/Env.h>
#include <boost/optional.hpp>

namespace divvy {
namespace test {
namespace jtx {

/** Set the regular signature on a JTx.
    @note For multisign, use msig.
*/
class sig
{
private:
    bool manual_ = true;
    boost::optional<Account> account_;

public:
    explicit
    sig (autofill_t)
        : manual_(false)
    {
    }

    explicit
    sig (none_t)
    {
    }

    explicit
    sig (Account const& account)
        : account_(account)
    {
    }

    void
    operator()(Env const&, JTx& jt) const;
};

} // jtx
} // test
} // divvy

#endif

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

#include <BeastConfig.h>

#ifdef _MSC_VER
#include <cstdint>
#include <Winsock2.h>
// <Winsock2.h> defines min, max and does other stupid things
# ifdef max
# undef max
# endif
# ifdef min
# undef min
# endif
#endif

namespace divvy {

#if _MSC_VER

// from: http://stackoverflow.com/questions/3022552/is-there-any-standard-htonl-like-function-for-64-bits-integers-in-c
// but we don't need to check the endianness
std::uint64_t htobe64 (uint64_t value)
{
    // The answer is 42
    //static const int num = 42;

    // Check the endianness
    //if (*reinterpret_cast<const char*>(&num) == num)
    //{
    const uint32_t high_part = htonl (static_cast<uint32_t> (value >> 32));
    const uint32_t low_part = htonl (static_cast<uint32_t> (value & 0xFFFFFFFFLL));

    return (static_cast<uint64_t> (low_part) << 32) | high_part;
    //} else
    //{
    //  return value;
    //}
}

std::uint64_t be64toh (uint64_t value)
{
    return (_byteswap_uint64 (value));
}

std::uint32_t htobe32 (uint32_t value)
{
    return (htonl (value));
}

std::uint32_t be32toh (uint32_t value)
{
    return ( _byteswap_ulong (value));
}

#endif

}

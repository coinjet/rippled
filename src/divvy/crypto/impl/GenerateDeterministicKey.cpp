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
#include <divvy/crypto/GenerateDeterministicKey.h>
#include <divvy/crypto/impl/ec_key.h>
#include <divvy/crypto/impl/openssl.h>
#include <divvy/basics/SHA512Half.h>
#include <array>
#include <string>
#include <openssl/pem.h>
#include <openssl/sha.h>

namespace divvy {

namespace openssl {

static EC_GROUP const* const secp256k1_group =
        EC_GROUP_new_by_curve_name (NID_secp256k1);
static bignum const secp256k1_order =
        get_order (secp256k1_group);

}  // namespace openssl

using namespace openssl;

static Blob serialize_ec_point (ec_point const& point)
{
    Blob result (33);

    serialize_ec_point (point, &result[0]);

    return result;
}

template <class FwdIt>
void
copy_uint32 (FwdIt out, std::uint32_t v)
{
    *out++ =  v >> 24;
    *out++ = (v >> 16) & 0xff;
    *out++ = (v >>  8) & 0xff;
    *out   =  v        & 0xff;
}

// #define EC_DEBUG

// Functions to add support for deterministic EC keys

// --> seed
// <-- private root generator + public root generator
static bignum generateRootDeterministicKey (uint128 const& seed)
{
    // find non-zero private key less than the curve's order
    bignum privKey;
    std::uint32_t seq = 0;

    do
    {
        // buf: 0                seed               16  seq  20
        //      |<--------------------------------->|<------>|
        std::array<std::uint8_t, 20> buf;
        std::copy(seed.begin(), seed.end(), buf.begin());
        copy_uint32 (buf.begin() + 16, seq++);
        auto root = sha512Half(buf);
        std::fill (buf.begin(), buf.end(), 0); // security erase
        privKey.assign ((unsigned char const*) &root, sizeof (root));
        root.zero(); // security erase
    }
    while (privKey.is_zero()  ||  privKey >= secp256k1_order);

    return privKey;
}

// --> seed
// <-- private root generator + public root generator
Blob generateRootDeterministicPublicKey (uint128 const& seed)
{
    bn_ctx ctx;

    bignum privKey = generateRootDeterministicKey (seed);

    // compute the corresponding public key point
    ec_point pubKey = multiply (secp256k1_group, privKey, ctx);

    privKey.clear();  // security erase

    return serialize_ec_point (pubKey);
}

uint256 generateRootDeterministicPrivateKey (uint128 const& seed)
{
    bignum key = generateRootDeterministicKey (seed);

    return uint256_from_bignum_clear (key);
}

// Take divvy address.
// --> root public generator (consumes)
// <-- root public generator in EC format
static ec_point generateRootPubKey (bignum&& pubGenerator)
{
    ec_point pubPoint = bn2point (secp256k1_group, pubGenerator.get());

    return pubPoint;
}

// --> public generator
static bignum makeHash (Blob const& pubGen, int seq, bignum const& order)
{
    int subSeq = 0;

    bignum result;

    assert(pubGen.size() == 33);
    do
    {
        // buf: 0          pubGen             33 seq   37 subSeq  41
        //      |<--------------------------->|<------>|<-------->|
        std::array<std::uint8_t, 41> buf;
        std::copy (pubGen.begin(), pubGen.end(), buf.begin());
        copy_uint32 (buf.begin() + 33, seq);
        copy_uint32 (buf.begin() + 37, subSeq++);
        auto root = sha512Half_s(buf);
        std::fill(buf.begin(), buf.end(), 0); // security erase
        result.assign ((unsigned char const*) &root, sizeof (root));
    }
    while (result.is_zero()  ||  result >= order);

    return result;
}

// --> public generator
Blob generatePublicDeterministicKey (Blob const& pubGen, int seq)
{
    // publicKey(n) = rootPublicKey EC_POINT_+ Hash(pubHash|seq)*point
    ec_point rootPubKey = generateRootPubKey (bignum (pubGen));

    bn_ctx ctx;

    // Calculate the private additional key.
    bignum hash = makeHash (pubGen, seq, secp256k1_order);

    // Calculate the corresponding public key.
    ec_point newPoint = multiply (secp256k1_group, hash, ctx);

    // Add the master public key and set.
    add_to (secp256k1_group, rootPubKey, newPoint, ctx);

    return serialize_ec_point (newPoint);
}

// --> root private key
uint256 generatePrivateDeterministicKey (
    Blob const& pubGen, uint128 const& seed, int seq)
{
    // privateKey(n) = (rootPrivateKey + Hash(pubHash|seq)) % order
    bignum rootPrivKey = generateRootDeterministicKey (seed);

    bn_ctx ctx;

    // calculate the private additional key
    bignum privKey = makeHash (pubGen, seq, secp256k1_order);

    // calculate the final private key
    add_to (rootPrivKey, privKey, secp256k1_order, ctx);

    rootPrivKey.clear();  // security erase

    return uint256_from_bignum_clear (privKey);
}

} // divvy

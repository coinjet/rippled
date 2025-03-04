//------------------------------------------------------------------------------
/*
    This file is part of divvyd: https://github.com/xdv/divvyd
    Copyright 2014 Ripple Labs Inc.

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
#include <divvy/basics/TestSuite.h>
#include <divvy/overlay/impl/Manifest.h>
#include <divvy/core/DatabaseCon.h>
#include <divvy/app/main/DBInit.h>
#include <divvy/protocol/Sign.h>
#include <divvy/protocol/STExchange.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

namespace divvy {
namespace tests {

class manifest_test : public divvy::TestSuite
{
private:
    static void cleanupDatabaseDir (boost::filesystem::path const& dbPath)
    {
        using namespace boost::filesystem;
        if (!exists (dbPath) || !is_directory (dbPath) || !is_empty (dbPath))
            return;
        remove (dbPath);
    }

    static void setupDatabaseDir (boost::filesystem::path const& dbPath)
    {
        using namespace boost::filesystem;
        if (!exists (dbPath))
        {
            create_directory (dbPath);
            return;
        }

        if (!is_directory (dbPath))
        {
            // someone created a file where we want to put our directory
            throw std::runtime_error ("Cannot create directory: " +
                                      dbPath.string ());
        }
    }
    static boost::filesystem::path getDatabasePath ()
    {
        return boost::filesystem::current_path () / "manifest_test_databases";
    }
public:
    manifest_test ()
    {
        try
        {
            setupDatabaseDir (getDatabasePath ());
        }
        catch (...)
        {
        }
    }
    ~manifest_test ()
    {
        try
        {
            cleanupDatabaseDir (getDatabasePath ());
        }
        catch (...)
        {
        }
    }

    Manifest
    make_Manifest
        (AnySecretKey const& sk, AnyPublicKey const& spk, int seq,
         bool broken = false)
    {
        auto const pk = sk.publicKey();

        STObject st(sfGeneric);
        set(st, sfSequence, seq);
        set(st, sfPublicKey, pk);
        set(st, sfSigningPubKey, spk);

        sign(st, HashPrefix::manifest, sk);
        expect(verify(st, HashPrefix::manifest, pk));

        if (broken)
        {
            set(st, sfSequence, seq + 1);
        }

        Serializer s;
        st.add(s);

        std::string const m (static_cast<char const*> (s.data()), s.size());
        if (auto r = divvy::make_Manifest (std::move (m)))
        {
            return std::move (*r);
        }
        throw std::runtime_error("Could not create a manifest");
    }

    Manifest
    clone (Manifest const& m)
    {
        return Manifest (m.serialized, m.masterKey, m.signingKey, m.sequence);
    }

    void testLoadStore (ManifestCache const& m)
    {
        testcase ("load/store");

        std::string const dbName("ManifestCacheTestDB");
        { 
            // create a database, save the manifest to the db and reload and
            // check that the manifest caches are the same
            DatabaseCon::Setup setup;
            setup.dataDir = getDatabasePath ();
            DatabaseCon dbCon(setup, dbName, WalletDBInit, WalletDBCount);

            m.save (dbCon);

            ManifestCache loaded;
            beast::Journal journal;
            loaded.load (dbCon, journal);

            auto getPopulatedManifests =
                    [](ManifestCache const& cache) -> std::vector<Manifest const*>
                    {
                        std::vector<Manifest const*> result;
                        result.reserve (32);
                        cache.for_each_manifest (
                            [&result](Manifest const& m)
            {result.push_back (&m);});
                        return result;
                    };
            auto sort =
                    [](std::vector<Manifest const*> mv) -> std::vector<Manifest const*>
                    {
                        std::sort (mv.begin (),
                                   mv.end (),
                                   [](Manifest const* lhs, Manifest const* rhs)
            {return lhs->serialized < rhs->serialized;});
                        return mv;
                    };
            std::vector<Manifest const*> const inManifests (
                sort (getPopulatedManifests (m)));
            std::vector<Manifest const*> const loadedManifests (
                sort (getPopulatedManifests (loaded)));
            if (inManifests.size () == loadedManifests.size ())
            {
                expect (std::equal
                        (inManifests.begin (), inManifests.end (),
                         loadedManifests.begin (),
                         [](Manifest const* lhs, Manifest const* rhs)
                         {return *lhs == *rhs;}));
            }
            else
            {
                fail ();
            }
        }
        boost::filesystem::remove (getDatabasePath () /
                                   boost::filesystem::path (dbName));
    }

    void
    run() override
    {
        ManifestCache cache;
        {
            testcase ("apply");
            auto const accepted = ManifestDisposition::accepted;
            auto const untrusted = ManifestDisposition::untrusted;
            auto const stale = ManifestDisposition::stale;
            auto const invalid = ManifestDisposition::invalid;

            beast::Journal journal;

            auto const sk_a = AnySecretKey::make_ed25519 ();
            auto const sk_b = AnySecretKey::make_ed25519 ();
            auto const pk_a = sk_a.publicKey ();
            auto const pk_b = sk_b.publicKey ();
            auto const kp_a = AnySecretKey::make_secp256k1_pair ();
            auto const kp_b = AnySecretKey::make_secp256k1_pair ();

            auto const s_a0 = make_Manifest (sk_a, kp_a.second, 0);
            auto const s_a1 = make_Manifest (sk_a, kp_a.second, 1);
            auto const s_b0 = make_Manifest (sk_b, kp_b.second, 0);
            auto const s_b1 = make_Manifest (sk_b, kp_b.second, 1);
            auto const s_b2 =
                make_Manifest (sk_b, kp_b.second, 2, true);  // broken
            auto const fake = s_b1.serialized + '\0';

            expect (cache.applyManifest (clone (s_a0), journal) == untrusted,
                    "have to install a trusted key first");

            cache.addTrustedKey (pk_a, "a");
            cache.addTrustedKey (pk_b, "b");

            expect (cache.applyManifest (clone (s_a0), journal) == accepted);
            expect (cache.applyManifest (clone (s_a0), journal) == stale);

            expect (cache.applyManifest (clone (s_a1), journal) == accepted);
            expect (cache.applyManifest (clone (s_a1), journal) == stale);
            expect (cache.applyManifest (clone (s_a0), journal) == stale);

            expect (cache.applyManifest (clone (s_b0), journal) == accepted);
            expect (cache.applyManifest (clone (s_b0), journal) == stale);

            expect (!divvy::make_Manifest(fake));
            expect (cache.applyManifest (clone (s_b2), journal) == invalid);
        }
        testLoadStore (cache);
    }
};

BEAST_DEFINE_TESTSUITE(manifest,overlay,divvy);

} // tests
} // divvy

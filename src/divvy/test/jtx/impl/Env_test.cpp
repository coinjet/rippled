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
#include <divvy/basics/Log.h>
#include <divvy/test/jtx.h>
#include <divvy/json/to_string.h>
#include <divvy/protocol/TxFlags.h>
#include <beast/hash/uhash.h>
#include <beast/unit_test/suite.h>
#include <boost/lexical_cast.hpp>
#include <utility>

namespace divvy {
namespace test {

class Env_test : public beast::unit_test::suite
{
public:
    template <class T>
    static
    std::string
    to_string (T const& t)
    {
        return boost::lexical_cast<
            std::string>(t);
    }

    // Declarations in Account.h
    void
    testAccount()
    {
        using namespace jtx;
        {
            Account a;
            Account b(a);
            a = b;
            a = std::move(b);
            Account c(std::move(a));
        }
        Account("alice");
        Account("alice", KeyType::secp256k1);
        Account("alice", KeyType::ed25519);
        auto const gw = Account("gw");
        [](AccountID){}(gw);
        auto const USD = gw["USD"];
        void(Account("alice") < gw);
        std::set<Account>().emplace(gw);
        std::unordered_set<Account,
            beast::uhash<>>().emplace("alice");
    }

    // Declarations in amount.h
    void
    testAmount()
    {
        using namespace jtx;

        PrettyAmount(0);
        PrettyAmount(1);
        PrettyAmount(0u);
        PrettyAmount(1u);
        PrettyAmount(-1);
        static_assert(! std::is_constructible<
            PrettyAmount, char>::value, "");
        static_assert(! std::is_constructible<
            PrettyAmount, unsigned char>::value, "");
        static_assert(! std::is_constructible<
            PrettyAmount, short>::value, "");
        static_assert(! std::is_constructible<
            PrettyAmount, unsigned short>::value, "");

        try
        {
            XDV(0.0000001);
            fail("missing exception");
        }
        catch(std::domain_error const&)
        {
            pass();
        }
        XDV(-0.000001);
        try
        {
            XDV(-0.0000009);
            fail("missing exception");
        }
        catch(std::domain_error const&)
        {
            pass();
        }

        expect(to_string(XDV(5)) == "5 XDV");
        expect(to_string(XDV(.80)) == "0.8 XDV");
        expect(to_string(XDV(.005)) == "5000 drops");
        expect(to_string(XDV(0.1)) == "0.1 XDV");
        expect(to_string(XDV(10000)) == "10000 XDV");
        expect(to_string(drops(10)) == "10 drops");
        expect(to_string(drops(123400000)) == "123.4 XDV");
        expect(to_string(XDV(-5)) == "-5 XDV");
        expect(to_string(XDV(-.99)) == "-0.99 XDV");
        expect(to_string(XDV(-.005)) == "-5000 drops");
        expect(to_string(XDV(-0.1)) == "-0.1 XDV");
        expect(to_string(drops(-10)) == "-10 drops");
        expect(to_string(drops(-123400000)) == "-123.4 XDV");

        expect(XDV(1) == drops(1000000));
        expect(XDV(1) == STAmount(1000000));
        expect(STAmount(1000000) == XDV(1));

        auto const gw = Account("gw");
        auto const USD = gw["USD"];
        expect(to_string(USD(0)) == "0/USD(gw)");
        expect(to_string(USD(10)) == "10/USD(gw)");
        expect(to_string(USD(-10)) == "-10/USD(gw)");
        expect(USD(0) == STAmount(USD, 0));
        expect(USD(1) == STAmount(USD, 1));
        expect(USD(-1) == STAmount(USD, -1));

        auto const get = [](AnyAmount a){ return a; };
        expect(! get(USD(10)).is_any);
        expect(get(any(USD(10))).is_any);
    }

    // Test Env
    void
    testEnv()
    {
        using namespace jtx;
        auto const n = XDV(10000);
        auto const gw = Account("gw");
        auto const USD = gw["USD"];
        auto const alice = Account("alice");

        // fund
        {
            Env env(*this);

            // variadics
            env.fund(n, "alice");
            env.fund(n, "bob", "carol");
            env.fund(n, "dave", nodivvy("eric"));
            env.fund(n, "fred", nodivvy("gary", "hank"));
            env.fund(n, nodivvy("irene"));
            env.fund(n, nodivvy("jim"), "karen");
            env.fund(n, nodivvy("lisa", "mary"));

            // flags
            env.fund(n, nodivvy("xavier"));
            env.require(nflags("xavier", asfDefaultDivvy));
            env.fund(n, "yana");
            env.require(flags("yana", asfDefaultDivvy));
        }

        // trust
        {
            Env env(*this);
            env.fund(n, "alice", "bob", gw);
            env(trust("alice", USD(100)), require(lines("alice", 1)));
        }

        // balance
        {
            Env env(*this);
            expect(env.balance(alice) == 0);
            expect(env.balance(alice, USD) != 0);
            expect(env.balance(alice, USD) == USD(0));
            env.fund(n, alice, gw);
            expect(env.balance(alice) == n);
            expect(env.balance(gw) == n);
            env.trust(USD(1000), alice);
            env(pay(gw, alice, USD(10)));
            expect(to_string(env.balance("alice", USD)) == "10/USD(gw)");
            expect(to_string(env.balance(gw, alice["USD"])) == "-10/USD(alice)");
        }

        // seq
        {
            Env env(*this);
            env.fund(n, nodivvy("alice", gw));
            expect(env.seq("alice") == 1);
            expect(env.seq(gw) == 1);
        }

        // autofill
        {
            Env env(*this);
            env.fund(n, "alice");
            env.require(balance("alice", n));
            env(noop("alice"), fee(1),                  ter(telINSUF_FEE_P));
            env(noop("alice"), seq(none),               ter(temMALFORMED));
            env(noop("alice"), seq(none), fee(10),      ter(temMALFORMED));
            env(noop("alice"), fee(none),               ter(temMALFORMED));
            env(noop("alice"), sig(none),               ter(temMALFORMED));
            env(noop("alice"), fee(autofill));
            env(noop("alice"), fee(autofill), seq(autofill));
            env(noop("alice"), fee(autofill), seq(autofill), sig(autofill));
        }
    }

    // Env::require
    void
    testRequire()
    {
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gw");
        auto const USD = gw["USD"];
        env.require(balance("alice", none));
        env.require(balance("alice", XDV(none)));
        env.fund(XDV(10000), "alice", gw);
        env.require(balance("alice", USD(none)));
        env.trust(USD(100), "alice");
        env.require(balance("alice", XDV(10000))); // fee refunded
        env.require(balance("alice", USD(0)));
        env(pay(gw, "alice", USD(10)), require(balance("alice", USD(10))));

        env.require(nflags("alice", asfRequireDest));
        env(fset("alice", asfRequireDest), require(flags("alice", asfRequireDest)));
        env(fclear("alice", asfRequireDest), require(nflags("alice", asfRequireDest)));
    }

    // Signing with secp256k1 and ed25519 keys
    void
    testKeyType()
    {
        using namespace jtx;

        Env env(*this);
        Account const alice("alice", KeyType::ed25519);
        Account const bob("bob", KeyType::secp256k1);
        Account const carol("carol");
        env.fund(XDV(10000), alice, bob);

        // Master key only
        env(noop(alice));
        env(noop(bob));
        env(noop(alice), sig("alice"),                          ter(tefBAD_AUTH_MASTER));
        env(noop(alice), sig(Account("alice",
            KeyType::secp256k1)),                               ter(tefBAD_AUTH_MASTER));
        env(noop(bob), sig(Account("bob",
            KeyType::ed25519)),                                 ter(tefBAD_AUTH_MASTER));
        env(noop(alice), sig(carol),                            ter(tefBAD_AUTH_MASTER));

        // Master and Regular key
        env(regkey(alice, bob));
        env(noop(alice));
        env(noop(alice), sig(bob));
        env(noop(alice), sig(alice));

        // Regular key only
        env(fset(alice, asfDisableMaster), sig(alice));
        env(noop(alice));
        env(noop(alice), sig(bob));
        env(noop(alice), sig(alice),                            ter(tefMASTER_DISABLED));
        env(fclear(alice, asfDisableMaster), sig(alice),        ter(tefMASTER_DISABLED));
        env(fclear(alice, asfDisableMaster), sig(bob));
        env(noop(alice), sig(alice));
    }

    // Payment basics
    void
    testPayments()
    {
        using namespace jtx;
        Env env(*this);
        auto const gw = Account("gateway");
        auto const USD = gw["USD"];

        env(pay(env.master, "alice", XDV(1000)), fee(none),     ter(temMALFORMED));
        env(pay(env.master, "alice", XDV(1000)), fee(1),        ter(telINSUF_FEE_P));
        env(pay(env.master, "alice", XDV(1000)), seq(none),     ter(temMALFORMED));
        env(pay(env.master, "alice", XDV(1000)), seq(2),        ter(terPRE_SEQ));
        env(pay(env.master, "alice", XDV(1000)), sig(none),     ter(temMALFORMED));
        env(pay(env.master, "alice", XDV(1000)), sig("bob"),    ter(tefBAD_AUTH_MASTER));

        env(pay(env.master, "dilbert", XDV(1000)), sig(env.master));

        env.fund(XDV(10000), "alice", "bob", "carol", gw);
        env.require(balance("alice", XDV(10000)));
        env.require(balance("bob", XDV(10000)));
        env.require(balance("carol", XDV(10000)));
        env.require(balance(gw, XDV(10000)));

        env.trust(USD(100), "alice", "bob", "carol");
        env.require(owners("alice", 1), lines("alice", 1));
        env(rate(gw, 1.05));

        env(pay(gw, "carol", USD(50)));
        env.require(balance("carol", USD(50)));
        env.require(balance(gw, Account("carol")["USD"](-50)));

        env(offer("carol", XDV(50), USD(50)), require(owners("carol", 2)));
        env(pay("alice", "bob", any(USD(10))),                  ter(tecPATH_DRY));
        env(pay("alice", "bob", any(USD(10))),
            paths(XDV), sendmax(XDV(10)),                       ter(tecPATH_PARTIAL));
        env(pay("alice", "bob", any(USD(10))), paths(XDV),
            sendmax(XDV(20)));
        env.require(balance("bob", USD(10)));
        env.require(balance("carol", USD(39.5)));

        env.memoize("eric");
        env(regkey("alice", "eric"));
        env(noop("alice"));
        env(noop("alice"), sig("alice"));
        env(noop("alice"), sig("eric"));
        env(noop("alice"), sig("bob"),                          ter(tefBAD_AUTH));
        env(fset("alice", asfDisableMaster),                    ter(tecNEED_MASTER_KEY));
        env(fset("alice", asfDisableMaster), sig("eric"),       ter(tecNEED_MASTER_KEY));
        env.require(nflags("alice", asfDisableMaster)); 
        env(fset("alice", asfDisableMaster), sig("alice"));
        env.require(flags("alice", asfDisableMaster));
        env(regkey("alice", disabled),                          ter(tecMASTER_DISABLED));
        env(noop("alice"));
        env(noop("alice"), sig("alice"),                        ter(tefMASTER_DISABLED));
        env(noop("alice"), sig("eric"));
        env(noop("alice"), sig("bob"),                          ter(tefBAD_AUTH));
        env(fclear("alice", asfDisableMaster), sig("bob"),      ter(tefBAD_AUTH));
        env(fclear("alice", asfDisableMaster), sig("alice"),    ter(tefMASTER_DISABLED));
        env(fclear("alice", asfDisableMaster));
        env.require(nflags("alice", asfDisableMaster));
        env(regkey("alice", disabled));
        env(noop("alice"), sig("eric"),                         ter(tefBAD_AUTH_MASTER));
        env(noop("alice"));
    }

    // Multi-sign basics
    void
    testMultiSign()
    {
        using namespace jtx;

        Env env(*this);
        env.fund(XDV(10000), "alice");
        env(signers("alice", 1,
            { { "alice", 1 }, { "bob", 2 } }),                  ter(temBAD_SIGNER));
        env(signers("alice", 1,
            { { "bob", 1 }, { "carol", 2 } }));
        env(noop("alice"));

        env(noop("alice"), msig("bob"));
        env(noop("alice"), msig("carol"));
        env(noop("alice"), msig("bob", "carol"));
        env(noop("alice"), msig("bob", "carol", "dilbert"),     ter(tefBAD_SIGNATURE));
    }

    // Two level Multi-sign
    void
    testMultiSign2()
    {
        using namespace jtx;

        Env env(*this);
        env.fund(XDV(10000), "alice", "bob", "carol");
        env.fund(XDV(10000), "david", "eric", "frank", "greg");
        env(signers("alice", 2, { { "bob", 1 },   { "carol", 1 } }));
        env(signers("bob",   1, { { "david", 1 }, { "eric", 1 } }));
        env(signers("carol", 1, { { "frank", 1 }, { "greg", 1 } }));

        env(noop("alice"), msig2(
        { { "bob", "david" } }),                                ter(tefBAD_QUORUM));
        
        env(noop("alice"), msig2(
        { { "bob", "david" }, { "bob", "eric" } }),             ter(tefBAD_QUORUM));

        env(noop("alice"), msig2(
        { { "carol", "frank" } }),                              ter(tefBAD_QUORUM));
        
        env(noop("alice"), msig2(
        { { "carol", "frank" }, { "carol", "greg" } }),         ter(tefBAD_QUORUM));

        env(noop("alice"), msig2(
        { { "bob", "david" }, { "carol", "frank" } }));

        env(noop("alice"), msig2(
        { { "bob", "david" }, { "bob", "eric" },
          { "carol", "frank" }, { "carol", "greg" } }));
    }

    void
    testTicket()
    {
        using namespace jtx;
        // create syntax
        ticket::create("alice", "bob");
        ticket::create("alice", 60);
        ticket::create("alice", "bob", 60);
        ticket::create("alice", 60, "bob");

        {
            Env env(*this);
            env.fund(XDV(10000), "alice");
            env(noop("alice"),                  require(owners("alice", 0), tickets("alice", 0)));
            env(ticket::create("alice"),        require(owners("alice", 1), tickets("alice", 1)));
            env(ticket::create("alice"),        require(owners("alice", 2), tickets("alice", 2)));
        }

        Env env(*this);
    }

    void testJTxProperties()
    {
        using namespace jtx;
        JTx jt1;
        // Test a straightforward
        // property
        expect(!jt1.get<int>());
        jt1.set<int>(7);
        expect(jt1.get<int>());
        expect(*jt1.get<int>() == 7);
        expect(!jt1.get<Env>());

        // Test that the property is
        // replaced if it exists.
        jt1.set<int>(17);
        expect(jt1.get<int>());
        expect(*jt1.get<int>() == 17);
        expect(!jt1.get<Env>());

        // Test that modifying the
        // returned prop is saved
        *jt1.get<int>() = 42;
        expect(jt1.get<int>());
        expect(*jt1.get<int>() == 42);
        expect(!jt1.get<Env>());

        // Test get() const
        auto const& jt2 = jt1;
        expect(jt2.get<int>());
        expect(*jt2.get<int>() == 42);
        expect(!jt2.get<Env>());
    }

    void testProp()
    {
        using namespace jtx;
        Env env(*this);
        env.memoize("alice");

        auto jt1 = env.jt(noop("alice"));
        expect(!jt1.get<std::uint16_t>());

        auto jt2 = env.jt(noop("alice"),
            prop<std::uint16_t>(-1));
        expect(jt2.get<std::uint16_t>());
        expect(*jt2.get<std::uint16_t>() ==
            65535);

        auto jt3 = env.jt(noop("alice"),
            prop<std::string>(
                "Hello, world!"),
                    prop<bool>(false));
        expect(jt3.get<std::string>());
        expect(*jt3.get<std::string>() ==
            "Hello, world!");
        expect(jt3.get<bool>());
        expect(!*jt3.get<bool>());
    }

    void testJTxCopy()
    {
        using namespace jtx;
        JTx jt1;
        jt1.set<int>(7);
        expect(jt1.get<int>());
        expect(*jt1.get<int>() == 7);
        expect(!jt1.get<Env>());
        JTx jt2(jt1);
        expect(jt2.get<int>());
        expect(*jt2.get<int>() == 7);
        expect(!jt2.get<Env>());
        JTx jt3;
        jt3 = jt1;
        expect(jt3.get<int>());
        expect(*jt3.get<int>() == 7);
        expect(!jt3.get<Env>());
    }

    void testJTxMove()
    {
        using namespace jtx;
        JTx jt1;
        jt1.set<int>(7);
        expect(jt1.get<int>());
        expect(*jt1.get<int>() == 7);
        expect(!jt1.get<Env>());
        JTx jt2(std::move(jt1));
        expect(!jt1.get<int>());
        expect(!jt1.get<Env>());
        expect(jt2.get<int>());
        expect(*jt2.get<int>() == 7);
        expect(!jt2.get<Env>());
        jt1 = std::move(jt2);
        expect(!jt2.get<int>());
        expect(!jt2.get<Env>());
        expect(jt1.get<int>());
        expect(*jt1.get<int>() == 7);
        expect(!jt1.get<Env>());
    }

    void
    testMemo()
    {
        using namespace jtx;
        Env env(*this);
        env.fund(XDV(10000), "alice");
        env(noop("alice"), memodata("data"));
        env(noop("alice"), memoformat("format"));
        env(noop("alice"), memotype("type"));
        env(noop("alice"), memondata("format", "type"));
        env(noop("alice"), memonformat("data", "type"));
        env(noop("alice"), memontype("data", "format"));
        env(noop("alice"), memo("data", "format", "type"));
        env(noop("alice"), memo("data1", "format1", "type1"), memo("data2", "format2", "type2"));
    }

    void
    testAdvance()
    {
        using namespace jtx;
        Env env(*this);
        // Create the LCL as a copy of the Env's
        // ledger. This will have the side effect
        // of skipping one seq in Env.ledger the
        // first time it is advanced. This can be
        // worked around if desired by assigning
        // an advanced ledger back to Env before
        // starting, but it won't matter to most
        // tests.
        std::shared_ptr<Ledger const> lastClosedLedger = 
            std::make_shared<Ledger>(
                *env.ledger, false);

        auto firstSeq = env.ledger->seq();

        expect(lastClosedLedger->seq() == firstSeq);

        advance(env, lastClosedLedger);

        expect(lastClosedLedger->seq() == firstSeq + 1);
        expect(env.ledger->seq() == firstSeq + 2);

        advance(env, lastClosedLedger);

        expect(lastClosedLedger->seq() == firstSeq + 2);
        expect(env.ledger->seq() == firstSeq + 3);
    }

    void
    run()
    {
        // Hack to silence logging
        deprecatedLogs().severity(
            beast::Journal::Severity::kNone);

        testAccount();
        testAmount();
        testEnv();
        testRequire();
        testKeyType();
        testPayments();
        testMultiSign();
        testMultiSign2();
        testTicket();
        testJTxProperties();
        testProp();
        testJTxCopy();
        testJTxMove();
        testMemo();
        testAdvance();
    }
};

BEAST_DEFINE_TESTSUITE(Env,app,divvy)

} // test
} // divvy

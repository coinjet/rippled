//------------------------------------------------------------------------------
/*
    This file is part of divvyd: https://github.com/xdv/divvyd
    Copyright (c) 2012-2014 Ripple Labs Inc.

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
#include <divvy/rpc/impl/Tuning.h>
#include <divvy/app/ledger/LedgerFees.h>
#include <divvy/app/paths/DivvyState.h>
#include <divvy/protocol/TxFlags.h>

namespace divvy {

static void fillTransaction (
    Json::Value& txArray,
    DivvyAddress const& account,
    std::uint32_t& sequence,
    Ledger::ref ledger)
{
    txArray["Sequence"] = Json::UInt (sequence++);
    txArray["Account"] = account.humanAccountID ();
    txArray["Fee"] = Json::UInt (scaleFeeLoad (
        getApp().getFeeTrack(), *ledger, 10, false));
}

// {
//   account: <account>|<account_public_key>
//   account_index: <number>        // optional, defaults to 0.
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
//   limit: integer                 // optional, number of problems
//   role: gateway|user             // account role to assume
//   transactions: true             // optional, reccommend transactions
// }
Json::Value doNoDivvyCheck (RPC::Context& context)
{
    auto const& params (context.params);
    if (! params.isMember (jss::account))
        return RPC::missing_field_error ("account");

    if (! params.isMember ("role"))
        return RPC::missing_field_error ("role");
    bool roleGateway = false;
    {
        std::string const role = params["role"].asString();
        if (role == "gateway")
            roleGateway = true;
        else if (role != "user")
        return RPC::invalid_field_message ("role");
    }

    unsigned int limit = 300;
    if (params.isMember (jss::limit))
    {
        auto const& jvLimit (params[jss::limit]);
        if (! jvLimit.isIntegral ())
            return RPC::expected_field_error ("limit", "unsigned integer");
        limit = jvLimit.isUInt () ? jvLimit.asUInt () :
            std::max (0, jvLimit.asInt ());
        if (context.role != Role::ADMIN)
        {
            limit = std::max (RPC::Tuning::minLinesPerRequest,
                std::min (limit, RPC::Tuning::maxLinesPerRequest));
        }
    }

    bool transactions = false;
    if (params.isMember (jss::transactions))
        transactions = params["transactions"].asBool();

    Ledger::pointer ledger;
    Json::Value result (RPC::lookupLedger (params, ledger, context.netOps));
    if (! ledger)
        return result;

    Json::Value dummy;
    Json::Value& jvTransactions =
        transactions ? (result[jss::transactions] = Json::arrayValue) : dummy;

    std::string strIdent (params[jss::account].asString ());
    bool bIndex (params.isMember (jss::account_index));
    int iIndex (bIndex ? params[jss::account_index].asUInt () : 0);
    DivvyAddress divvyAddress;

    Json::Value const jv = RPC::accountFromString (
        divvyAddress, bIndex, strIdent, iIndex, false);
    if (! jv.empty ())
    {
        for (Json::Value::const_iterator it (jv.begin ()); it != jv.end (); ++it)
            result[it.memberName ()] = it.key ();

        return result;
    }

    AccountState::pointer accountState =
        getAccountState(*ledger, divvyAddress,
            getApp().getSLECache());
    if (! accountState)
        return rpcError (rpcACT_NOT_FOUND);

    std::uint32_t seq = accountState->sle().getFieldU32 (sfSequence);

    Json::Value& problems = (result["problems"] = Json::arrayValue);

    bool bDefaultDivvy = accountState->sle().getFieldU32 (sfFlags) & lsfDefaultDivvy;

    if (bDefaultDivvy & ! roleGateway)
    {
        problems.append ("You appear to have set your default divvy flag even though you "
            "are not a gateway. This is not recommended unless you are experimenting");
    }
    else if (roleGateway & ! bDefaultDivvy)
    {
        problems.append ("You should immediately set your default divvy flag");
        if (transactions)
        {
            Json::Value& tx = jvTransactions.append (Json::objectValue);
            tx["TransactionType"] = "AccountSet";
            tx["SetFlag"] = 8;
            fillTransaction (tx, divvyAddress, seq, ledger);
        }
    }

    auto const accountID = divvyAddress.getAccountID ();

    forEachItemAfter (*ledger, accountID, getApp().getSLECache(),
            uint256(), 0, limit,
        [&](std::shared_ptr<SLE const> const& ownedItem)
        {
            if (ownedItem->getType() == ltRIPPLE_STATE)
            {
                bool const bLow = accountID == ownedItem->getFieldAmount(sfLowLimit).getIssuer();

                bool const bNoDivvy = ownedItem->getFieldU32(sfFlags) &
                   (bLow ? lsfLowNoDivvy : lsfHighNoDivvy);

                std::string problem;
                bool needFix = false;
                if (bNoDivvy & roleGateway)
                {
                    problem = "You should clear the no divvy flag on your ";
                    needFix = true;
                }
                else if (! roleGateway & ! bNoDivvy)
                {
                    problem = "You should probably set the no divvy flag on your ";
                    needFix = true;
                }
                if (needFix)
                {
                    AccountID peer =
                        ownedItem->getFieldAmount (bLow ? sfHighLimit : sfLowLimit).getIssuer();
                    STAmount peerLimit = ownedItem->getFieldAmount (bLow ? sfHighLimit : sfLowLimit);
                    problem += to_string (peerLimit.getCurrency());
                    problem += " line to ";
                    problem += to_string (peerLimit.getIssuer());
                    problems.append (problem);

                    STAmount limitAmount (ownedItem->getFieldAmount (bLow ? sfLowLimit : sfHighLimit));
                    limitAmount.setIssuer (peer);

                    Json::Value& tx = jvTransactions.append (Json::objectValue);
                    tx["TransactionType"] = "TrustSet";
                    tx["LimitAmount"] = limitAmount.getJson (0);
                    tx["Flags"] = bNoDivvy ? tfClearNoDivvy : tfSetNoDivvy;
                    fillTransaction(tx, divvyAddress, seq, ledger);

                    return true;
                }
            }
	    return false;
        });

    return result;
}

} // divvy

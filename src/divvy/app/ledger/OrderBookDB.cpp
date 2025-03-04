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
#include <divvy/app/ledger/OrderBookDB.h>
#include <divvy/app/ledger/LedgerMaster.h>
#include <divvy/app/main/Application.h>
#include <divvy/basics/Log.h>
#include <divvy/core/Config.h>
#include <divvy/core/JobQueue.h>
#include <divvy/protocol/Indexes.h>

namespace divvy {

OrderBookDB::OrderBookDB (Stoppable& parent)
    : Stoppable ("OrderBookDB", parent)
    , mSeq (0)
{
}

void OrderBookDB::invalidate ()
{
    ScopedLockType sl (mLock);
    mSeq = 0;
}

void OrderBookDB::setup (Ledger::ref ledger)
{
    {
        ScopedLockType sl (mLock);
        auto seq = ledger->getLedgerSeq ();

        // Do a full update every 256 ledgers
        if (mSeq != 0)
        {
            if (seq == mSeq)
                return;
            if ((seq > mSeq) && ((seq - mSeq) < 256))
                return;
            if ((seq < mSeq) && ((mSeq - seq) < 16))
                return;
        }

        WriteLog (lsDEBUG, OrderBookDB)
            << "Advancing from " << mSeq << " to " << seq;

        mSeq = seq;
    }

    if (getConfig().RUN_STANDALONE)
        update(ledger);
    else
        getApp().getJobQueue().addJob(jtUPDATE_PF, "OrderBookDB::update",
            std::bind(&OrderBookDB::update, this, ledger));
}

static void updateHelper (SLE::ref entry,
    hash_set< uint256 >& seen,
    OrderBookDB::IssueToOrderBook& destMap,
    OrderBookDB::IssueToOrderBook& sourceMap,
    hash_set< Issue >& XDVBooks,
    int& books)
{
    if (entry->getType () == ltDIR_NODE &&
        entry->isFieldPresent (sfExchangeRate) &&
        entry->getFieldH256 (sfRootIndex) == entry->getIndex())
    {
        Book book;
        book.in.currency.copyFrom (entry->getFieldH160 (sfTakerPaysCurrency));
        book.in.account.copyFrom (entry->getFieldH160 (sfTakerPaysIssuer));
        book.out.account.copyFrom (entry->getFieldH160 (sfTakerGetsIssuer));
        book.out.currency.copyFrom (entry->getFieldH160 (sfTakerGetsCurrency));

        uint256 index = getBookBase (book);
        if (seen.insert (index).second)
        {
            auto orderBook = std::make_shared<OrderBook> (index, book);
            sourceMap[book.in].push_back (orderBook);
            destMap[book.out].push_back (orderBook);
            if (isXDV(book.out))
                XDVBooks.insert(book.in);
            ++books;
        }
    }
}

void OrderBookDB::update (Ledger::pointer ledger)
{
    hash_set< uint256 > seen;
    OrderBookDB::IssueToOrderBook destMap;
    OrderBookDB::IssueToOrderBook sourceMap;
    hash_set< Issue > XDVBooks;

    WriteLog (lsDEBUG, OrderBookDB) << "OrderBookDB::update>";

    // walk through the entire ledger looking for orderbook entries
    int books = 0;

    try
    {
        ledger->visitStateItems(std::bind(&updateHelper, std::placeholders::_1,
                                          std::ref(seen), std::ref(destMap),
            std::ref(sourceMap), std::ref(XDVBooks), std::ref(books)));
    }
    catch (const SHAMapMissingNode&)
    {
        WriteLog (lsINFO, OrderBookDB)
            << "OrderBookDB::update encountered a missing node";
        ScopedLockType sl (mLock);
        mSeq = 0;
        return;
    }

    WriteLog (lsDEBUG, OrderBookDB)
        << "OrderBookDB::update< " << books << " books found";
    {
        ScopedLockType sl (mLock);

        mXDVBooks.swap(XDVBooks);
        mSourceMap.swap(sourceMap);
        mDestMap.swap(destMap);
    }
    getApp().getLedgerMaster().newOrderBookDB();
}

void OrderBookDB::addOrderBook(Book const& book)
{
    bool toXDV = isXDV (book.out);
    ScopedLockType sl (mLock);

    if (toXDV)
    {
        // We don't want to search through all the to-XDV or from-XDV order
        // books!
        for (auto ob: mSourceMap[book.in])
        {
            if (isXDV (ob->getCurrencyOut ())) // also to XDV
                return;
        }
    }
    else
    {
        for (auto ob: mDestMap[book.out])
        {
            if (ob->getCurrencyIn() == book.in.currency &&
                ob->getIssuerIn() == book.in.account)
            {
                return;
            }
        }
    }
    uint256 index = getBookBase(book);
    auto orderBook = std::make_shared<OrderBook> (index, book);

    mSourceMap[book.in].push_back (orderBook);
    mDestMap[book.out].push_back (orderBook);
    if (toXDV)
        mXDVBooks.insert(book.in);
}

// return list of all orderbooks that want this issuerID and currencyID
OrderBook::List OrderBookDB::getBooksByTakerPays (Issue const& issue)
{
    ScopedLockType sl (mLock);
    auto it = mSourceMap.find (issue);
    return it == mSourceMap.end () ? OrderBook::List() : it->second;
}

int OrderBookDB::getBookSize(Issue const& issue) {
    ScopedLockType sl (mLock);
    auto it = mSourceMap.find (issue);
    return it == mSourceMap.end () ? 0 : it->second.size();
}

bool OrderBookDB::isBookToXDV(Issue const& issue)
{
    ScopedLockType sl (mLock);
    return mXDVBooks.count(issue) > 0;
}

BookListeners::pointer OrderBookDB::makeBookListeners (Book const& book)
{
    ScopedLockType sl (mLock);
    auto ret = getBookListeners (book);

    if (!ret)
    {
        ret = std::make_shared<BookListeners> ();

        mListeners [book] = ret;
        assert (getBookListeners (book) == ret);
    }

    return ret;
}

BookListeners::pointer OrderBookDB::getBookListeners (Book const& book)
{
    BookListeners::pointer ret;
    ScopedLockType sl (mLock);

    auto it0 = mListeners.find (book);
    if (it0 != mListeners.end ())
        ret = it0->second;

    return ret;
}

// Based on the meta, send the meta to the streams that are listening.
// We need to determine which streams a given meta effects.
void OrderBookDB::processTxn (
    Ledger::ref ledger, const AcceptedLedgerTx& alTx, Json::Value const& jvObj)
{
    ScopedLockType sl (mLock);

    if (alTx.getResult () == tesSUCCESS)
    {
        // Check if this is an offer or an offer cancel or a payment that
        // consumes an offer.
        // Check to see what the meta looks like.
        for (auto& node : alTx.getMeta ()->getNodes ())
        {
            try
            {
                if (node.getFieldU16 (sfLedgerEntryType) == ltOFFER)
                {
                    SField const* field = nullptr;

                    // We need a field that contains the TakerGets and TakerPays
                    // parameters.
                    if (node.getFName () == sfModifiedNode)
                        field = &sfPreviousFields;
                    else if (node.getFName () == sfCreatedNode)
                        field = &sfNewFields;
                    else if (node.getFName () == sfDeletedNode)
                        field = &sfFinalFields;

                    if (field)
                    {
                        auto data = dynamic_cast<const STObject*> (
                            node.peekAtPField (*field));

                        if (data)
                        {
                            // determine the OrderBook
                            auto listeners = getBookListeners (
                                {data->getFieldAmount (sfTakerGets).issue(),
                                 data->getFieldAmount (sfTakerPays).issue()});

                            if (listeners)
                                listeners->publish (jvObj);
                        }
                    }
                }
            }
            catch (...)
            {
                WriteLog (lsINFO, OrderBookDB)
                    << "Fields not found in OrderBookDB::processTxn";
            }
        }
    }
}

} // divvy

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

#ifndef RIPPLE_PROTOCOL_STLEDGERENTRY_H_INCLUDED
#define RIPPLE_PROTOCOL_STLEDGERENTRY_H_INCLUDED

#include <divvy/protocol/Indexes.h>
#include <divvy/protocol/STObject.h>

namespace divvy {

class STLedgerEntry final
    : public STObject
    , public CountedObject <STLedgerEntry>
{
public:
    static char const* getCountedObjectName () { return "STLedgerEntry"; }

    using pointer = std::shared_ptr<STLedgerEntry>;
    using ref     = const std::shared_ptr<STLedgerEntry>&;

    /** Create an empty object with the given key and type. */
    explicit
    STLedgerEntry (Keylet const& k);

    STLedgerEntry (LedgerEntryType type,
            uint256 const& key)
        : STLedgerEntry(Keylet(type, key))
    {
    }

    STLedgerEntry (const Serializer & s, uint256 const& index);
    
    STLedgerEntry (SerialIter & sit, uint256 const& index);
  
        
    STLedgerEntry (const STObject & object, uint256 const& index);

    STBase*
    copy (std::size_t n, void* buf) const override
    {
        return emplace(n, buf, *this);
    }

    STBase*
    move (std::size_t n, void* buf) override
    {
        return emplace(n, buf, std::move(*this));
    }

    SerializedTypeID getSType () const override
    {
        return STI_LEDGERENTRY;
    }

    std::string getFullText () const override;
    
    std::string getText () const override;
    
    Json::Value getJson (int options) const override;

    /** Returns the 'key' (or 'index') of this item.
        The key identifies this entry's position in
        the SHAMap associative container.
    */
    uint256 const&
    key() const
    {
        return key_;
    }

    // DEPRECATED
    uint256 const& getIndex () const
    {
        return key_;
    }

    void setImmutable ()
    {
        mMutable = false;
    }

    bool isMutable ()
    {
        return mMutable;
    }

    LedgerEntryType getType () const
    {
        return type_;
    }

    std::uint16_t getVersion () const
    {
        return getFieldU16 (sfLedgerEntryType);
    }
    
    bool isThreadedType() const; // is this a ledger entry that can be threaded
    
    bool isThreaded () const;     // is this ledger entry actually threaded
    
    bool hasOneOwner () const;    // This node has one other node that owns it
    
    bool hasTwoOwners () const;   // This node has two nodes that own it (like divvy balance)
    
    DivvyAddress getOwner () const;
    
    DivvyAddress getFirstOwner () const;
    
    DivvyAddress getSecondOwner () const;
    
    uint256 getThreadedTransaction () const;
    
    std::uint32_t getThreadedLedger () const;
    
    bool thread (uint256 const& txID, std::uint32_t ledgerSeq, uint256 & prevTxID,
                 std::uint32_t & prevLedgerID);

private:
    /*  Make STObject comply with the template for this SLE type
        Can throw
    */
    void setSLEType ();

private:
    uint256 key_;
    LedgerEntryType type_;
    LedgerFormats::Item const*  mFormat;
    bool mMutable;
};

using SLE = STLedgerEntry;

} // divvy

#endif

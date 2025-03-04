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
#include <divvy/core/Job.h>

namespace divvy {

Job::Job ()
    : mType (jtINVALID)
    , mJobIndex (0)
{
}

Job::Job (JobType type, std::uint64_t index)
    : mType (type)
    , mJobIndex (index)
{
}

Job::Job (JobType type,
          std::string const& name,
          std::uint64_t index,
          LoadMonitor& lm,
          std::function <void (Job&)> const& job,
          CancelCallback cancelCallback)
    : m_cancelCallback (cancelCallback)
    , mType (type)
    , mJobIndex (index)
    , mJob (job)
    , mName (name)
    , m_queue_time (clock_type::now ())
{
    m_loadEvent = std::make_shared <LoadEvent> (std::ref (lm), name, false);
}

JobType Job::getType () const
{
    return mType;
}

Job::CancelCallback Job::getCancelCallback () const
{
    bassert (m_cancelCallback);
    return m_cancelCallback;
}

Job::clock_type::time_point const& Job::queue_time () const
{
    return m_queue_time;
}

bool Job::shouldCancel () const
{
    if (m_cancelCallback)
        return m_cancelCallback ();
    return false;
}

void Job::doJob ()
{
    m_loadEvent->start ();
    m_loadEvent->reName (mName);

    mJob (*this);
}

void Job::rename (std::string const& newName)
{
    mName = newName;
}

bool Job::operator> (const Job& j) const
{
    if (mType < j.mType)
        return true;

    if (mType > j.mType)
        return false;

    return mJobIndex > j.mJobIndex;
}

bool Job::operator>= (const Job& j) const
{
    if (mType < j.mType)
        return true;

    if (mType > j.mType)
        return false;

    return mJobIndex >= j.mJobIndex;
}

bool Job::operator< (const Job& j) const
{
    if (mType < j.mType)
        return false;

    if (mType > j.mType)
        return true;

    return mJobIndex < j.mJobIndex;
}

bool Job::operator<= (const Job& j) const
{
    if (mType < j.mType)
        return false;

    if (mType > j.mType)
        return true;

    return mJobIndex <= j.mJobIndex;
}

}

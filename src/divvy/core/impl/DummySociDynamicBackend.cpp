//------------------------------------------------------------------------------
/*
    This file is part of divvyd: https://github.com/xdv/divvyd
    Copyright (c) 2012-2015 Ripple Labs Inc.

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

/*  Stub functions for soci dynamic backends.

    Divvy does not use dynamic backends, and inclduing soci's
    dynamic backends compilcates the build (it requires a generated
    header file and some macros to be defined.)
*/

#include <BeastConfig.h>

#include <divvy/core/SociDB.h>

// dummy soci-backend
namespace soci {
namespace dynamic_backends {
// used internally by session
backend_factory const& get (std::string const& name)
{
    throw std::runtime_error ("Not Supported");
};

// provided for advanced user-level management
std::vector<std::string>& search_paths ()
{
    static std::vector<std::string> empty;
    return empty;
};
void register_backend (std::string const&, std::string const&){};
void register_backend (std::string const&, backend_factory const&){};
std::vector<std::string> list_all ()
{
    return {};
};
void unload (std::string const&){};
void unload_all (){};

}  // namespace dynamic_backends
}  // namespace soci


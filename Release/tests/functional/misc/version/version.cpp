/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Basic tests for versioning
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "cpprest/version.h"
#include "unittestpp.h"

namespace tests { namespace functional { namespace misc { namespace versioning {

SUITE(version_test1)
{

TEST(VersionTest1)
{
    // If these tests fail, this means that version.props and version.h are out of sync
    // When the version number if changed, both files must change
    VERIFY_ARE_EQUAL(_VER_REVISION, CPPREST_VERSION_REVISION);
    VERIFY_ARE_EQUAL(_VER_MINOR, CPPREST_VERSION_MINOR);
    VERIFY_ARE_EQUAL(_VER_MAJOR, CPPREST_VERSION_MAJOR);
}

}

}}}}


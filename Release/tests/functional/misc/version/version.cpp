/***
* ==++==
*
* Copyright (c) Microsoft Corporation. All rights reserved. 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* ==--==
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


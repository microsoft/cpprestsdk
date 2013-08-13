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
* iterator_tests.cpp
*
* Tests iterating over JSON values
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"
#include <algorithm>

using namespace web; using namespace utility;

namespace tests { namespace functional { namespace json_tests {

SUITE(iterator_tests)
{

void validate_empty_preincrement(json::value value)
{
    VERIFY_ARE_EQUAL(0, value.size());
    size_t count = 0;
    for (auto iter = value.begin(); iter != value.end(); ++iter)
    {
        count++;
    }
    VERIFY_ARE_EQUAL(0, count);
}

void validate_empty_reverse(json::value value)
{
    VERIFY_ARE_EQUAL(0, value.size());
    size_t count = 0;
    for (auto iter = value.rbegin(); iter != value.rend(); ++iter)
    {
        count++;
    }
    VERIFY_ARE_EQUAL(0, count);
}

void validate_empty_global(json::value value)
{
    VERIFY_ARE_EQUAL(0, value.size());
    size_t count = 0;
    for (auto iter = std::begin(value); iter != std::end(value); ++iter)
    {
        count++;
    }
    VERIFY_ARE_EQUAL(0, count);
}

void validate_empty_const(const json::value& value)
{
    VERIFY_ARE_EQUAL(0, value.size());
    size_t count = 0;
    for (auto iter = value.cbegin(); iter != value.cend(); ++iter)
    {
        count++;
    }
    VERIFY_ARE_EQUAL(0, count);
}

void validate_empty_postincrement(json::value value)
{
    VERIFY_ARE_EQUAL(0, value.size());
    size_t count = 0;
    for (auto iter = value.begin(); iter != value.end(); iter++)
    {
        count++;
    }
    VERIFY_ARE_EQUAL(0, count);
}

TEST(non_composites_member_preincrement)
{
    validate_empty_preincrement(json::value::null());
    validate_empty_preincrement(json::value::number(17));
    validate_empty_preincrement(json::value::boolean(true));
    validate_empty_preincrement(json::value::string(U("Hello!")));
}

TEST(non_composites_member_reverse)
{
    validate_empty_reverse(json::value::null());
    validate_empty_reverse(json::value::number(17));
    validate_empty_reverse(json::value::boolean(true));
    validate_empty_reverse(json::value::string(U("Hello!")));
}

TEST(non_composites_member_postincrement)
{
    validate_empty_postincrement(json::value::null());
    validate_empty_postincrement(json::value::number(17));
    validate_empty_postincrement(json::value::boolean(true));
    validate_empty_postincrement(json::value::string(U("Hello!")));
}

TEST(non_composites_global_function)
{
    validate_empty_global(json::value::null());
    validate_empty_global(json::value::number(17));
    validate_empty_global(json::value::boolean(true));
    validate_empty_global(json::value::string(U("Hello!")));
}

TEST(non_composites_const)
{
    json::value v_null = json::value::null();
    json::value v_number = json::value::number(17);
    json::value v_bool = json::value::boolean(true);
    json::value v_string = json::value::string(U("Hello!"));

    validate_empty_const(v_null);
    validate_empty_const(v_number);
    validate_empty_const(v_bool);
    validate_empty_const(v_string);
}

TEST(objects_constructed)
{
    json::value val1;
    val1[U("a")] = 44;
    val1[U("b")] = json::value(true);
    val1[U("c")] = json::value(false);

    VERIFY_ARE_EQUAL(3, val1.size());

    size_t count = 0;
    for (auto iter = std::begin(val1); iter != std::end(val1); ++iter)
    {
        auto key = iter->first;
        auto& value = iter->second;
        switch(count)
        {
        case 0:
            VERIFY_ARE_EQUAL(U("a"), key.as_string());
            VERIFY_IS_TRUE(value.is_number());
            break;
        case 1:
            VERIFY_ARE_EQUAL(U("b"), key.as_string());
            VERIFY_IS_TRUE(value.is_boolean());
            break;
        case 2:
            VERIFY_ARE_EQUAL(U("c"), key.as_string());
            VERIFY_IS_TRUE(value.is_boolean());
            break;
        }
        count++;
    }
    VERIFY_ARE_EQUAL(3, count);
}

TEST(objects_parsed)
{
    json::value val1 = json::value::parse(U("{\"a\": 44, \"b\": true, \"c\": false}"));

    VERIFY_ARE_EQUAL(3, val1.size());

    size_t count = 0;
    for (auto iter = std::begin(val1); iter != std::end(val1); ++iter)
    {
        auto key = iter->first;
        auto& value = iter->second;
        switch(count)
        {
        default:
            VERIFY_IS_TRUE(value.is_null());
            break;
        case 0:
            VERIFY_ARE_EQUAL(U("a"), key.as_string());
            VERIFY_IS_TRUE(value.is_number());
            VERIFY_ARE_EQUAL(44, value.as_integer());
            break;
        case 1:
            VERIFY_ARE_EQUAL(U("b"), key.as_string());
            VERIFY_IS_TRUE(value.is_boolean());
            VERIFY_IS_TRUE(value.as_bool());
            break;
        case 2:
            VERIFY_ARE_EQUAL(U("c"), key.as_string());
            VERIFY_IS_TRUE(value.is_boolean());
            VERIFY_IS_FALSE(value.as_bool());
            break;
        }
        count++;
    }
    VERIFY_ARE_EQUAL(3, count);
}

TEST(objects_reverse)
{
    json::value val1 = json::value::parse(U("{\"a\": 44, \"b\": true, \"c\": false}"));

    VERIFY_ARE_EQUAL(3, val1.size());

    size_t count = 0;
    for (auto iter = val1.rbegin(); iter != val1.rend(); ++iter)
    {
        auto key = iter->first;
        auto& value = iter->second;
        switch(count)
        {
        case 2:
            VERIFY_ARE_EQUAL(U("a"), key.as_string());
            VERIFY_IS_TRUE(value.is_number());
            VERIFY_ARE_EQUAL(44, value.as_integer());
            break;
        case 1:
            VERIFY_ARE_EQUAL(U("b"), key.as_string());
            VERIFY_IS_TRUE(value.is_boolean());
            VERIFY_IS_TRUE(value.as_bool());
            break;
        case 0:
            VERIFY_ARE_EQUAL(U("c"), key.as_string());
            VERIFY_IS_TRUE(value.is_boolean());
            VERIFY_IS_FALSE(value.as_bool());
            break;
        }
        count++;
    }
    VERIFY_ARE_EQUAL(3, count);
}

TEST(arrays_constructed)
{
    json::value val1;
    val1[0] = 44;
    val1[2] = json::value(true);
    val1[5] = json::value(true);

    VERIFY_ARE_EQUAL(6, val1.size());

    size_t count = 0;
    for (auto iter = std::begin(val1); iter != std::end(val1); ++iter)
    {
        auto key = iter->first;
        auto& value = iter->second;
        VERIFY_ARE_EQUAL(count, key.as_integer());
        switch(count)
        {
        case 0:
            VERIFY_IS_TRUE(value.is_number());
            VERIFY_ARE_EQUAL(44, value.as_integer());
            break;
        case 2:
            VERIFY_IS_TRUE(value.is_boolean());
            VERIFY_IS_TRUE(value.as_bool());
            break;
        case 5:
            VERIFY_IS_TRUE(value.is_boolean());
            VERIFY_IS_TRUE(value.as_bool());
            break;
        }
        count++;
    }
    VERIFY_ARE_EQUAL(6, count);
}

TEST(arrays_parsed)
{
    json::value val1 = json::value::parse(U("[44, true, false]"));

    VERIFY_ARE_EQUAL(3, val1.size());

    size_t count = 0;
    for (auto iter = std::begin(val1); iter != std::end(val1); ++iter)
    {
        auto key = iter->first;
        auto value = iter->second;
        VERIFY_ARE_EQUAL(count, key.as_integer());
        switch(count)
        {
        case 0:
            VERIFY_IS_TRUE(value.is_number());
            VERIFY_ARE_EQUAL(44, value.as_integer());
            break;
        case 1:
            VERIFY_IS_TRUE(value.is_boolean());
            VERIFY_IS_TRUE(value.as_bool());
            break;
        case 2:
            VERIFY_IS_TRUE(value.is_boolean());
            VERIFY_IS_FALSE(value.as_bool());
            break;
        }
        count++;
    }
    VERIFY_ARE_EQUAL(3, count);
}

TEST(arrays_reversed)
{
    json::value val1 = json::value::parse(U("[44, true, false]"));

    VERIFY_ARE_EQUAL(3, val1.size());

    size_t count = 0;
    for (auto iter = val1.rbegin(); iter != val1.rend(); ++iter)
    {
        auto key = iter->first;
        auto value = iter->second;
        switch(count)
        {
        case 2:
            VERIFY_IS_TRUE(value.is_number());
            VERIFY_ARE_EQUAL(44, value.as_integer());
            break;
        case 1:
            VERIFY_IS_TRUE(value.is_boolean());
            VERIFY_IS_TRUE(value.as_bool());
            break;
        case 0:
            VERIFY_IS_TRUE(value.is_boolean());
            VERIFY_IS_FALSE(value.as_bool());
            break;
        }
        count++;
    }
    VERIFY_ARE_EQUAL(3, count);
}

TEST(comparison)
{
    json::value val1;
    val1[U("a")] = 44;
    val1[U("b")] = json::value(true);
    val1[U("c")] = json::value(false);

    auto first = std::begin(val1);
    auto f     = first;
    auto f_1   = first++;
    auto f_2   = ++first;

    VERIFY_ARE_EQUAL(f, f_1);
    VERIFY_ARE_NOT_EQUAL(f_1, f_2);
}

TEST(std_algorithms)
{
    {
        // for_each
        size_t count = 0;
        json::value v_array = json::value::parse(U("[44, true, false]"));
        std::for_each(std::begin(v_array), std::end(v_array),
            [&](json::value::iterator::value_type)
            {
                count++;
            });
        VERIFY_ARE_EQUAL(3, count);
    }
    {
        // find_if
        json::value v_array = json::value::parse(U("[44, true, false]"));
        auto _where = 
            std::find_if(std::begin(v_array), std::end(v_array),
            [&](json::value::iterator::value_type iter)
            {
                return iter.second.is_boolean();
            });
        VERIFY_ARE_NOT_EQUAL(_where, std::end(v_array));
        VERIFY_ARE_EQUAL(_where->first.as_integer(), 1);
    }
    {
        // copy_if
        json::value v_array = json::value::parse(U("[44, true, false]"));
        std::vector<json::value::iterator::value_type> v_target(v_array.size());
        auto _where = 
            std::copy_if(std::begin(v_array), std::end(v_array), std::begin(v_target),
            [&](json::value::iterator::value_type iter)
            {
                return iter.second.is_boolean();
            });
        VERIFY_ARE_EQUAL(2, _where-std::begin(v_target));
        VERIFY_IS_FALSE(v_array.begin()[1].second.is_number());
    }
    {
        // transform
        json::value v_array = json::value::parse(U("[44, true, false]"));
        std::vector<json::value> v_target(v_array.size());
        auto _where = 
            std::transform(std::begin(v_array), std::end(v_array), std::begin(v_target),
            [&](json::value::iterator::value_type) -> json::value
            {
                return json::value::number(17);
            });

        VERIFY_ARE_EQUAL(3, v_target.size());

        for (auto iter = std::begin(v_target); iter != std::end(v_target); ++iter)
        {
            VERIFY_IS_FALSE(iter->is_null());
        }
    }
}

}

}}}
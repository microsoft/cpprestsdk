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
* construction_tests.cpp
*
* Tests creating JSON values.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "stdafx.h"

using namespace web; using namespace utility;

namespace tests { namespace functional { namespace json_tests {

SUITE(construction_tests)
{

#if defined(__cplusplus_winrt)

TEST(winrt_platform_string)
{
    Platform::String ^platformStr = "Hello!";
    json::value jstr = json::value::string(platformStr->Data());
    CHECK(jstr.is_string());
    CHECK_EQUAL(jstr.to_string(), U("\"Hello!\""));
}

#endif

TEST(assignment_op)
{
    json::value arr = json::value::array();
    arr[0] = json::value(true);

    json::value ass_copy = arr;
    VERIFY_IS_TRUE(ass_copy.is_array());
    VERIFY_ARE_EQUAL(U("true"), ass_copy[0].to_string());
    ass_copy[1] = json::value(false);
    VERIFY_ARE_EQUAL(U("false"), ass_copy[1].to_string());
    VERIFY_ARE_EQUAL(U("null"), arr[1].to_string());
}

TEST(copy_ctor_array)
{
    json::value arr = json::value::array();
    arr[0] = json::value(true);

    json::value copy(arr);
    VERIFY_IS_TRUE(copy.is_array());
    VERIFY_ARE_EQUAL(U("true"), copy[0].to_string());
    copy[1] = json::value(false);
    VERIFY_ARE_EQUAL(U("false"), copy[1].to_string());
    VERIFY_ARE_EQUAL(U("null"), arr[1].to_string());
}

TEST(copy_ctor_object)
{
    json::value obj = json::value::object();
    utility::string_t keyName(U("key"));
    obj[keyName] = json::value(false);

    // Copy object that has values added.
    json::value copy(obj);
    VERIFY_IS_TRUE(copy.is_object());
    VERIFY_ARE_EQUAL(U("false"), copy[keyName].to_string());
    obj[keyName] = json::value(true);
    VERIFY_ARE_EQUAL(U("false"), copy[keyName].to_string());
    VERIFY_ARE_EQUAL(U("true"), obj[keyName].to_string());

    // Copy object that parses with value, but none additional added.
    obj = json::value::parse(U("{\"key\": true}"));
    json::value copy2(obj);
    VERIFY_IS_TRUE(copy2.is_object());
    obj[keyName] = json::value(false);
    VERIFY_IS_TRUE(copy2.size() == 1);
    VERIFY_ARE_EQUAL(U("false"), obj[keyName].to_string());
    VERIFY_ARE_EQUAL(U("true"), copy2[keyName].to_string());
}

TEST(copy_ctor_string)
{
    utility::string_t strValue(U("teststr"));
    json::value str = json::value::string(strValue);

    json::value copy(str);
    VERIFY_IS_TRUE(copy.is_string());
    VERIFY_ARE_EQUAL(strValue, copy.as_string());
    str = json::value::string(U("teststr2"));
    VERIFY_ARE_EQUAL(strValue, copy.as_string());
    VERIFY_ARE_EQUAL(U("teststr2"), str.as_string());
}

TEST(move_ctor)
{
    json::value obj;
    obj[U("A")] = json::value(true);

    json::value moved(std::move(obj));
    VERIFY_IS_TRUE(moved.is_object());
    VERIFY_ARE_EQUAL(U("true"), moved[U("A")].to_string());
    moved[U("B")] = json::value(false);
    VERIFY_ARE_EQUAL(U("false"), moved[U("B")].to_string());
}

TEST(move_assignment_op)
{
    json::value obj;
    obj[U("A")] = json::value(true);

    json::value moved;
    moved = std::move(obj);
    VERIFY_IS_TRUE(moved.is_object());
    VERIFY_ARE_EQUAL(U("true"), moved[U("A")].to_string());
    moved[U("B")] = json::value(false);
    VERIFY_ARE_EQUAL(U("false"), moved[U("B")].to_string());
}

TEST(constructor_overloads)
{
    json::value v0;
    json::value v1(17);
    json::value v2(3.1415);
    json::value v3(true);
    const utility::char_t* p4 = U("Hello!");
    json::value v4(p4);

    json::value v5(U("Hello Again!"));
    json::value v6(U("YES YOU KNOW IT"));
    json::value v7(U("HERE ID IS"));

    VERIFY_ARE_EQUAL(v0.type(), json::value::Null);
    VERIFY_IS_TRUE(v0.is_null());
    VERIFY_ARE_EQUAL(v1.type(), json::value::Number);
    VERIFY_IS_TRUE(v1.is_number());
    VERIFY_ARE_EQUAL(v2.type(), json::value::Number);
    VERIFY_IS_TRUE(v2.is_number());
    VERIFY_ARE_EQUAL(v3.type(), json::value::Boolean);
    VERIFY_IS_TRUE(v3.is_boolean());
    VERIFY_ARE_EQUAL(v4.type(), json::value::String);
    VERIFY_IS_TRUE(v4.is_string());
    VERIFY_ARE_EQUAL(v5.type(), json::value::String);
    VERIFY_IS_TRUE(v5.is_string());
    VERIFY_ARE_EQUAL(v6.type(), json::value::String);
    VERIFY_IS_TRUE(v6.is_string());
    VERIFY_ARE_EQUAL(v7.type(), json::value::String);
    VERIFY_IS_TRUE(v7.is_string());
}

TEST(factory_overloads)
{
    json::value v0 = json::value::null();
    json::value v1 = json::value::number(17);
    json::value v2 = json::value::number(3.1415);
    json::value v3 = json::value::boolean(true);
    json::value v4 = json::value::string(U("Hello!"));
    json::value v5 = json::value::string(U("Hello Again!"));
    json::value v6 = json::value::string(U("Hello!"));
    json::value v7 = json::value::string(U("Hello Again!"));
    json::value v8 = json::value::object();
    json::value v9 = json::value::array();

    VERIFY_ARE_EQUAL(v0.type(), json::value::Null);
    VERIFY_ARE_EQUAL(v1.type(), json::value::Number);
    VERIFY_ARE_EQUAL(v2.type(), json::value::Number);
    VERIFY_ARE_EQUAL(v3.type(), json::value::Boolean);
    VERIFY_ARE_EQUAL(v4.type(), json::value::String);
    VERIFY_ARE_EQUAL(v5.type(), json::value::String);
    VERIFY_ARE_EQUAL(v6.type(), json::value::String);
    VERIFY_ARE_EQUAL(v7.type(), json::value::String);
    VERIFY_ARE_EQUAL(v8.type(), json::value::Object);
    VERIFY_IS_TRUE(v8.is_object());
    VERIFY_ARE_EQUAL(v9.type(), json::value::Array);
    VERIFY_IS_TRUE(v9.is_array());
}

TEST(object_construction)
{
    // Factory which takes a map.
    json::value::field_map f;
    f.push_back(std::make_pair(json::value(U("abc")), json::value(true)));
    f.push_back(std::make_pair(json::value(U("xyz")), json::value(44)));
    json::value obj = json::value::object(f);
    
    VERIFY_ARE_EQUAL(f.size(), obj.size());

    obj[U("abc")] = json::value::string(U("str"));
    obj[U("123")] = json::value(false);

    VERIFY_ARE_NOT_EQUAL(f.size(), obj.size());
    VERIFY_ARE_EQUAL(json::value::string(U("str")).to_string(), obj[U("abc")].to_string());
    VERIFY_ARE_EQUAL(json::value(false).to_string(), obj[U("123")].to_string());

    // Tests constructing empty and adding.
    auto val1 = json::value::object();
    val1[U("A")] = 44;
    val1[U("hahah")] = json::value(true);
    VERIFY_ARE_EQUAL(2u, val1.size());
    VERIFY_ARE_EQUAL(U("44"), val1[U("A")].to_string());
    VERIFY_ARE_EQUAL(U("true"), val1[U("hahah")].to_string());

    // Construct as null value, then turn into object.
    json::value val2;
    VERIFY_IS_TRUE(val2.is_null());
    val2[U("A")] = 44;
    val2[U("hahah")] = json::value(true);
    VERIFY_ARE_EQUAL(2u, val2.size());
    VERIFY_ARE_EQUAL(U("44"), val2[U("A")].to_string());
    VERIFY_ARE_EQUAL(U("true"), val2[U("hahah")].to_string());

    const json::value& val2ref = val2;
    VERIFY_ARE_EQUAL(U("44"), val2ref[U("A")].to_string());
    VERIFY_ARE_EQUAL(U("true"), val2ref[U("hahah")].to_string());
    VERIFY_THROWS(val2ref[U("NOTTHERE")], json::json_exception);
}

TEST(array_construction)
{
    // Constructor which takes a vector.
    json::value::element_vector e;
    e.push_back(std::make_pair(json::value(0), json::value(false)));
    e.push_back(std::make_pair(json::value(1), json::value::string(U("hehe"))));
    json::value arr = json::value::array(e);
    VERIFY_ARE_EQUAL(e.size(), arr.size());
    VERIFY_ARE_EQUAL(U("false"), arr[0].to_string());
    arr[3] = json::value(22);
    VERIFY_ARE_NOT_EQUAL(e.size(), arr.size());
    VERIFY_ARE_EQUAL(U("22"), arr[3].to_string());

    // Test empty factory and adding.
    auto arr2 = json::value::array();
    arr2[1] = json::value(false);
    arr2[0] = json::value::object();
    arr2[0][U("A")] = json::value::string(U("HE"));
    VERIFY_ARE_EQUAL(2u, arr2.size());
    VERIFY_ARE_EQUAL(U("false"), arr2[1].to_string());
    VERIFY_ARE_EQUAL(U("\"HE\""), arr2[0][U("A")].to_string());

    // Construct as null value and then add elements.
    json::value arr3;
    VERIFY_IS_TRUE(arr3.is_null());
    arr3[1] = json::value(false);
    // Element [0] should already behave as an object.
    arr3[0][U("A")] = json::value::string(U("HE"));
    VERIFY_ARE_EQUAL(2u, arr3.size());
    VERIFY_ARE_EQUAL(U("false"), arr3[1].to_string());
    VERIFY_ARE_EQUAL(U("\"HE\""), arr3[0][U("A")].to_string());

    // Test factory which takes a size.
    auto arr4 = json::value::array(2);
    VERIFY_IS_TRUE(arr4[0].is_null());
    VERIFY_IS_TRUE(arr4[1].is_null());
    arr4[2] = json::value(true);
    arr4[0] = json::value(false);
    VERIFY_ARE_EQUAL(U("false"), arr4[0].to_string());
    VERIFY_ARE_EQUAL(U("true"), arr4[2].to_string());

    const json::value& arr4ref = arr4;
    VERIFY_ARE_EQUAL(U("false"), arr4ref[0].to_string());
    VERIFY_ARE_EQUAL(U("true"), arr4ref[2].to_string());
    VERIFY_THROWS(arr4ref[17], json::json_exception);
}

// This must not compile on Windows, since the ctor is defined private:
/*
TEST(json_from_string)
{
    json::value strValue = json::value("str");
}
*/
} // SUITE(construction_tests)

}}}


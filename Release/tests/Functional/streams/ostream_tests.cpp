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
* Basic tests for async output stream operations.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

#if defined(__cplusplus_winrt)
using namespace Windows::Storage;
#endif

namespace tests { namespace functional { namespace streams {

using namespace utility;
using namespace ::pplx;

//
// The following two functions will help mask the differences between non-WinRT environments and
// WinRT: on the latter, a file path is typically not used to open files. Rather, a UI element is used
// to get a 'StorageFile' reference and you go from there. However, to test the library properly,
// we need to get a StorageFile reference somehow, and one way to do that is to create all the files
// used in testing in the Documents folder.
//
template<typename _CharType>
pplx::task<concurrency::streams::basic_ostream<_CharType>> OPENSTR_W(const utility::string_t &name, std::ios_base::openmode mode = std::ios_base::out)
{
#if !defined(__cplusplus_winrt)
    return concurrency::streams::file_stream<_CharType>::open_ostream(name, mode);
#else
    auto file = pplx::create_task(
        KnownFolders::DocumentsLibrary->CreateFileAsync(
            ref new Platform::String(name.c_str()), CreationCollisionOption::ReplaceExisting)).get();

    return concurrency::streams::file_stream<_CharType>::open_ostream(file, mode);
#endif
}

#pragma warning(push)
#pragma warning(disable : 4100)  // Because of '_Prot' in WinRT builds.
template<typename _CharType>
pplx::task<concurrency::streams::basic_istream<_CharType>> OPENSTR_R(const utility::string_t &name, std::ios_base::openmode mode = std::ios_base::in)
{
#if !defined(__cplusplus_winrt)
    return concurrency::streams::file_stream<_CharType>::open_istream(name, mode);
#else
    auto file = pplx::create_task(
        KnownFolders::DocumentsLibrary->GetFileAsync(
            ref new Platform::String(name.c_str()))).get();

    return concurrency::streams::file_stream<_CharType>::open_istream(file, mode);
#endif
}

SUITE(ostream_tests)
{

TEST(BasicTest1)
{
    auto open = OPENSTR_W<uint8_t>(U("BasicTest1.txt"));
    auto basic_stream = open.get();
    auto a = basic_stream.print(10);
    auto b = basic_stream.print("-suffix");
    (a && b).wait();
    auto cls = basic_stream.close();

    VERIFY_IS_TRUE(cls.get());
    VERIFY_IS_TRUE(cls.is_done());
}

TEST(BasicTest2)
{
    auto open = OPENSTR_W<uint8_t>(U("BasicTest2.txt"));

    auto cls = 
        open.then(
        [](pplx::task<concurrency::streams::ostream> op) -> pplx::task<bool>
        {
            auto basic_stream = op.get();
            auto a = basic_stream.print(10);
            auto b = basic_stream.print("-suffix");
            (a && b).wait();
            return basic_stream.close();
        });

    VERIFY_IS_TRUE(cls.get());

    VERIFY_IS_TRUE(cls.is_done());
}


TEST(WriteSingleCharTest2)
{ 
    auto open = OPENSTR_W<uint8_t>(U("WriteSingleCharStrTest1.txt"));
    auto stream = open.get();

    VERIFY_IS_TRUE(open.is_done());

    bool elements_equal = true;

    for (uint8_t ch = 'a'; ch <= 'z'; ch++)
    {
        elements_equal = elements_equal && (ch == stream.write(ch).get());
    }

    VERIFY_IS_TRUE(elements_equal);

    auto close = stream.close();
    auto closed = close.get();

    VERIFY_IS_TRUE(close.is_done());
    VERIFY_IS_TRUE(closed);
}

TEST(WriteBufferTest1)
{ 
    auto open = OPENSTR_W<uint8_t>(U("WriteBufferStrTest1.txt"));
    auto stream = open.get();

    VERIFY_IS_TRUE(open.is_done());

    std::vector<uint8_t> vect;

    for (char ch = 'a'; ch <= 'z'; ch++)
    {
        vect.push_back(ch);
    }

    size_t vsz = vect.size();

    concurrency::streams::container_stream<std::vector<uint8_t>>::buffer_type txtbuf(std::move(vect), std::ios_base::in);

    VERIFY_ARE_EQUAL(stream.write(txtbuf, vsz).get(), vsz);

    auto close = stream.close();
    auto closed = close.get();

    VERIFY_IS_TRUE(close.is_done());
    VERIFY_IS_TRUE(closed);
}

TEST(WriteBufferAndSyncTest1, "Ignore", "478760")
{ 
    auto open = OPENSTR_W<uint8_t>(U("WriteBufferAndSyncStrTest1.txt"));
    auto stream = open.get();

    VERIFY_IS_TRUE(open.is_done());

    std::vector<char> vect;

    for (char ch = 'a'; ch <= 'z'; ch++)
    {
        vect.push_back(ch);
    }

    size_t vsz = vect.size();
    concurrency::streams::rawptr_buffer<uint8_t> txtbuf(reinterpret_cast<const uint8_t*>(&vect[0]), vsz);

    auto write = stream.write(txtbuf, vsz);
    auto sync  = stream.flush().get();
    CHECK(sync);

    VERIFY_IS_TRUE(write.is_done());
    VERIFY_ARE_EQUAL(write.get(), vect.size());

    auto close = stream.close();
    auto closed = close.get();

    VERIFY_IS_TRUE(close.is_done());
    VERIFY_IS_TRUE(closed);
}

TEST(tell_bug)
{
    auto count = 
        OPENSTR_W<uint8_t>(U("tell_bug.txt"), std::ios_base::out | std::ios_base::trunc).
            then([=](concurrency::streams::ostream os) -> std::streamoff {
               os.print("A");
               auto val = os.tell();
               os.close().get();
               return val;
            }).get();
    
   VERIFY_ARE_EQUAL(std::streamoff(1), count);
}

TEST(iostream_container_buffer1)
{
    concurrency::streams::container_buffer<std::vector<uint8_t>> buf;

    concurrency::streams::ostream os = buf.create_ostream();
    os.write('a');
    os.write('b');
    os.close();

    concurrency::streams::istream is = 
        concurrency::streams::container_stream<std::vector<uint8_t>>::open_istream(std::move(buf.collection()));
    VERIFY_ARE_EQUAL(is.read().get(), 'a');
    VERIFY_ARE_EQUAL(is.read().get(), 'b');
}

TEST(iostream_container_buffer2)
{
    concurrency::streams::container_buffer<std::vector<uint8_t>> buf;

    {
        concurrency::streams::ostream os = buf.create_ostream();

        os.write('a');
        os.write('b');
        os.close();
    }

    {
        concurrency::streams::istream is = 
            concurrency::streams::container_stream<std::vector<uint8_t>>::open_istream(std::move(buf.collection()));

        is.read().then([&is](concurrency::streams::basic_ostream<uint8_t>::int_type c) -> pplx::task<concurrency::streams::basic_ostream<uint8_t>::int_type>
        {
            VERIFY_ARE_EQUAL(c, 'a');
            return is.read();
        }).then([&is](concurrency::streams::basic_ostream<uint8_t>::int_type c) -> pplx::task<bool>
        {
            VERIFY_ARE_EQUAL(c, 'b');
            return is.close();
        }).wait();
    }
}
TEST(FileSequentialWrite, "Ignore:Linux", "TFS#623951")
{ 
    auto open = OPENSTR_W<uint8_t>(U("WriteFileSequential.txt"), std::ios::app | std::ios::out);
    auto stream = open.get();

    VERIFY_IS_TRUE(open.is_done());

    std::vector<pplx::task<size_t>> v;
    for (int i = 0; i < 100; i++)
    {
        v.push_back(stream.print(i));
        v.push_back(stream.print(' '));
    }
    pplx::when_all(v.begin(), v.end()).wait();
    stream.close().wait();
    auto istream = OPENSTR_R<uint8_t>(U("WriteFileSequential.txt")).get();
    for (int i = 0; i < 100; i++)
    {
        VERIFY_ARE_EQUAL(istream.extract<int>().get(), i);
        istream.read().get();
    }

}


TEST(implied_out_mode)
{
    auto ostr = OPENSTR_W<char>(U("implied_out_mode.txt"), std::ios::ios_base::app).get();

    std::string str = "abcd";
    concurrency::streams::stringstreambuf block(str);

    size_t s = ostr.write(block, str.size()).get();

    VERIFY_ARE_EQUAL(s, str.size());

    auto cls = ostr.close();

    VERIFY_IS_TRUE(cls.get());
    VERIFY_IS_TRUE(cls.is_done());
}

} // SUITE(ostream_tests)

}}}


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
* Basic tests for winrt interop streams.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#include "stdafx.h"

using namespace concurrency::streams;

#if defined(__cplusplus_winrt)
using namespace Windows::Storage;
#endif

namespace tests { namespace functional { namespace streams {

using namespace utility;
using namespace ::pplx;


SUITE(winrt_interop_tests)
{

TEST(read_in)
{
    producer_consumer_buffer<uint8_t> buf;
    auto ostream = buf.create_ostream();
    ostream.print(utility::string_t(U("abcdefghij")));

    auto dr = ref new Windows::Storage::Streams::DataReader(winrt_stream::create_input_stream(buf));
    dr->ByteOrder = Windows::Storage::Streams::ByteOrder::LittleEndian;

    {
        VERIFY_ARE_EQUAL(10, pplx::create_task(dr->LoadAsync(10)).get());

        auto value = dr->ReadString(5);
        VERIFY_ARE_EQUAL(utility::string_t(value->Data()), U("abcde"));
        value = dr->ReadString(5);
        VERIFY_ARE_EQUAL(utility::string_t(value->Data()), U("fghij"));
    }
    {
        ostream.write<int8_t>(11).wait();
        ostream.write<int8_t>(17).wait();

        VERIFY_ARE_EQUAL(2, pplx::create_task(dr->LoadAsync(2)).get());

        auto ival = dr->ReadByte();
        VERIFY_ARE_EQUAL(ival, 11);
        ival = dr->ReadByte();
        VERIFY_ARE_EQUAL(ival, 17);
    }
    {
        ostream.write<int16_t>(13).wait();
        ostream.write<int16_t>(4711).wait();

        VERIFY_ARE_EQUAL(4, pplx::create_task(dr->LoadAsync(4)).get());

        auto ival = dr->ReadInt16();
        VERIFY_ARE_EQUAL(ival, 13);
        ival = dr->ReadInt16();
        VERIFY_ARE_EQUAL(ival, 4711);
    }
    {
        ostream.write<int32_t>(17).wait();
        ostream.write<int32_t>(456231987).wait();

        VERIFY_ARE_EQUAL(8, pplx::create_task(dr->LoadAsync(8)).get());

        auto ival = dr->ReadInt32();
        VERIFY_ARE_EQUAL(ival, 17);
        ival = dr->ReadInt32();
        VERIFY_ARE_EQUAL(ival, 456231987);
    }
    {
        for (int i = 0; i < 100; i++)
        {
            ostream.write<int8_t>(int8_t(i)).wait();
        }

        VERIFY_ARE_EQUAL(100, pplx::create_task(dr->LoadAsync(100)).get());

        auto arr = ref new Platform::Array<unsigned char,1>(100);
        dr->ReadBytes(arr);

        for (int i = 0; i < 100; i++)
        {
            VERIFY_ARE_EQUAL(arr[i], i);
        }
    }
    buf.close(std::ios_base::out);
}

TEST(read_rand)
{
    producer_consumer_buffer<uint8_t> buf;
    auto ostream = buf.create_ostream();
    ostream.print(utility::string_t(U("abcdefghij")));

    auto dr = ref new Windows::Storage::Streams::DataReader(winrt_stream::create_random_access_stream(buf));
    dr->ByteOrder = Windows::Storage::Streams::ByteOrder::LittleEndian;

    {
        VERIFY_ARE_EQUAL(10, pplx::create_task(dr->LoadAsync(10)).get());

        auto value = dr->ReadString(5);
        VERIFY_ARE_EQUAL(utility::string_t(value->Data()), U("abcde"));
        value = dr->ReadString(5);
        VERIFY_ARE_EQUAL(utility::string_t(value->Data()), U("fghij"));
    }
    {
        ostream.write<int8_t>(11).wait();
        ostream.write<int8_t>(17).wait();

        VERIFY_ARE_EQUAL(2, pplx::create_task(dr->LoadAsync(2)).get());

        auto ival = dr->ReadByte();
        VERIFY_ARE_EQUAL(ival, 11);
        ival = dr->ReadByte();
        VERIFY_ARE_EQUAL(ival, 17);
    }
    {
        ostream.write<int16_t>(13).wait();
        ostream.write<int16_t>(4711).wait();

        VERIFY_ARE_EQUAL(4, pplx::create_task(dr->LoadAsync(4)).get());

        auto ival = dr->ReadInt16();
        VERIFY_ARE_EQUAL(ival, 13);
        ival = dr->ReadInt16();
        VERIFY_ARE_EQUAL(ival, 4711);
    }
    {
        ostream.write<int32_t>(17).wait();
        ostream.write<int32_t>(456231987).wait();

        VERIFY_ARE_EQUAL(8, pplx::create_task(dr->LoadAsync(8)).get());

        auto ival = dr->ReadInt32();
        VERIFY_ARE_EQUAL(ival, 17);
        ival = dr->ReadInt32();
        VERIFY_ARE_EQUAL(ival, 456231987);
    }
    {
        for (int i = 0; i < 100; i++)
        {
            ostream.write<int8_t>(int8_t(i)).wait();
        }

        VERIFY_ARE_EQUAL(100, pplx::create_task(dr->LoadAsync(100)).get());
        auto arr = ref new Platform::Array<unsigned char,1>(100);
        dr->ReadBytes(arr);

        for (int i = 0; i < 100; i++)
        {
            VERIFY_ARE_EQUAL(arr[i], i);
        }
    }
    buf.close(std::ios_base::out);
}

pplx::task<bool> StoreAndFlush(Windows::Storage::Streams::DataWriter^ dw)
{
    return pplx::create_task(dw->StoreAsync()).
        then([dw](unsigned int) { return pplx::create_task(dw->FlushAsync()); });
}

TEST(write_out)
{
    producer_consumer_buffer<uint8_t> buf;

    auto dw = ref new Windows::Storage::Streams::DataWriter(winrt_stream::create_output_stream(buf));
    dw->ByteOrder = Windows::Storage::Streams::ByteOrder::LittleEndian;

    auto value = ref new ::Platform::String(U("10 4711 -10.0 hello!"));
    dw->WriteString(value);
    dw->WriteByte(11); // Take care to make this a non-character!
    dw->WriteUInt16(17);
    dw->WriteUInt32(4711);
    VERIFY_IS_TRUE(StoreAndFlush(dw).get());
    buf.close(std::ios_base::out);

    auto istream = buf.create_istream();
    VERIFY_ARE_EQUAL(10, istream.extract<unsigned int>().get());
    VERIFY_ARE_EQUAL(4711, istream.extract<int>().get());
    VERIFY_ARE_EQUAL(-10.0, istream.extract<double>().get());
    VERIFY_ARE_EQUAL(utility::string_t(U("hello!")), istream.extract<utility::string_t>().get());
    VERIFY_ARE_EQUAL(11, istream.read<uint8_t>().get());
    VERIFY_ARE_EQUAL(17, istream.read<uint16_t>().get());
    VERIFY_ARE_EQUAL(4711, istream.read<uint32_t>().get());
}

TEST(write_rand)
{
    producer_consumer_buffer<uint8_t> buf;

    auto dw = ref new Windows::Storage::Streams::DataWriter(winrt_stream::create_random_access_stream(buf));
    dw->ByteOrder = Windows::Storage::Streams::ByteOrder::LittleEndian;

    auto value = ref new ::Platform::String(U("10 4711 -10.0 hello!"));
    dw->WriteString(value);
    dw->WriteByte(11); // Take care to make this a non-character!
    dw->WriteUInt16(17);
    dw->WriteUInt32(4711);
    VERIFY_IS_TRUE(StoreAndFlush(dw).get());
    buf.close(std::ios_base::out);

    auto istream = buf.create_istream();
    VERIFY_ARE_EQUAL(10, istream.extract<unsigned int>().get());
    VERIFY_ARE_EQUAL(4711, istream.extract<int>().get());
    VERIFY_ARE_EQUAL(-10.0, istream.extract<double>().get());
    VERIFY_ARE_EQUAL(utility::string_t(U("hello!")), istream.extract<utility::string_t>().get());
    VERIFY_ARE_EQUAL(11, istream.read<uint8_t>().get());
    VERIFY_ARE_EQUAL(17, istream.read<uint16_t>().get());
    VERIFY_ARE_EQUAL(4711, istream.read<uint32_t>().get());
}

TEST(read_write_attributes)
{
    {
        container_buffer<std::string> buf("test data");
        auto rastr = winrt_stream::create_random_access_stream(buf);
        VERIFY_IS_TRUE(rastr->CanRead);
        VERIFY_IS_FALSE(rastr->CanWrite);
        VERIFY_ARE_EQUAL(rastr->Position, 0);

        VERIFY_ARE_EQUAL(rastr->Size, 9);
        rastr->Size = 1024U;
        VERIFY_ARE_EQUAL(rastr->Size, 9);
    }
    {
        container_buffer<std::string> buf;
        auto rastr = winrt_stream::create_random_access_stream(buf);
        VERIFY_IS_FALSE(rastr->CanRead);
        VERIFY_IS_TRUE(rastr->CanWrite);
        VERIFY_ARE_EQUAL(rastr->Position, 0);

        VERIFY_ARE_EQUAL(rastr->Size, 0);
        rastr->Size = 1024U;
        VERIFY_ARE_EQUAL(rastr->Size, 1024U);
        VERIFY_ARE_EQUAL(buf.collection().size(), 1024U);
    }
    {
        producer_consumer_buffer<uint8_t> buf;
        auto rastr = winrt_stream::create_random_access_stream(buf);
        VERIFY_IS_TRUE(rastr->CanRead);
        VERIFY_IS_TRUE(rastr->CanWrite);
        VERIFY_ARE_EQUAL(rastr->Position, 0);

        VERIFY_ARE_EQUAL(rastr->Size, 0);
        rastr->Size = 1024U;
        VERIFY_ARE_EQUAL(rastr->Size, 1024U);
    }
}

TEST(cant_write)
{
    container_buffer<std::string> buf("test data");

    auto ostr = winrt_stream::create_output_stream(buf);
    auto dw = ref new Windows::Storage::Streams::DataWriter(ostr);
    dw->ByteOrder = Windows::Storage::Streams::ByteOrder::LittleEndian;

    auto value = ref new ::Platform::String(U("10 4711 -10.0 hello!"));
    dw->WriteString(value);

    VERIFY_IS_FALSE(StoreAndFlush(dw).get());
}

TEST(cant_read)
{
    container_buffer<std::string> buf;
    auto ostream = buf.create_ostream();
    ostream.print<int>(10);

    auto istr = winrt_stream::create_input_stream(buf);
    auto dr = ref new Windows::Storage::Streams::DataReader(istr);
    dr->ByteOrder = Windows::Storage::Streams::ByteOrder::LittleEndian;

    VERIFY_ARE_EQUAL(0, pplx::create_task(dr->LoadAsync(2)).get());
}

} // SUITE

}}} // namespaces
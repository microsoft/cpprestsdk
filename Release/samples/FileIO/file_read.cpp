/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
*file_read.cpp - Simple cmd line application demonstrating the filestream creation from file handle and reading from it.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include <Windows.h>

#include "cpprest/filestream.h"
#include "cpprest/containerstream.h"
#include "cpprest/producerconsumerstream.h"

using namespace std;
using namespace utility;
using namespace concurrency::streams;

void read_test(string_t file_name, long chunk_size)
{
	size_t total_bytes_read = 0;
	size_t bytes_read = 0;

	try {
		auto start = clock();
		concurrency::streams::container_buffer<std::vector<uint8_t>> file_read_buffer;

        HANDLE file_handle = CreateFileW(file_name.c_str(), GENERIC_READ,
            FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, 0);

		auto fs = file_stream<uint8_t>::open_istream(file_handle).get();
		fs.seek(std::ios_base::beg);

		while ((bytes_read = fs.read(file_read_buffer, chunk_size).get()) != 0)
		{
			//cout << total_bytes_read << endl;
			total_bytes_read += bytes_read;
			file_read_buffer.seekpos(std::ios_base::beg, std::ios_base::out);
		}
		fs.close();
		auto end = clock();
		auto time_taken = float(end - start) / CLOCKS_PER_SEC;
		auto throughput = (total_bytes_read / (1024.0 * 1024.0)) / time_taken;
		cout << "Bytes read: " << total_bytes_read << endl;
		cout << "Time taken: " << time_taken << endl;
		cout << "Throughput: " << throughput << "MB/s" << endl;
	}
	catch (exception e)
	{
		cout << e.what();
	}
}

int wmain(int argc, wchar_t *argv[])
{
	try {
		string_t file_name = string_t(argv[1]);
		wcout << "Filename: " << argv[1] << endl;

		int iterations = 1;
		if (argc > 2) {
			iterations = _wtoi(argv[2]);
		}

		long chunk_size = (4*1024*1024);
		if (argc > 3) {
			chunk_size = _wtol(argv[3]);
		}

		for (int i = 0; i < iterations; i++) {
			cout << endl;
			cout << "Test iteration: " << i+1 << endl;
			read_test(file_name, chunk_size);
		}
	}
	catch (exception e) {
		cout << "Exception" << endl;
		cout << e.what() << endl;
	}
}
//#define BOOST_ASIO_SAMPLE

#ifdef BOOST_ASIO_SAMPLE

#include <boost/asio.hpp>
#include <iostream>

boost::asio::io_service ioservice;
boost::asio::windows::stream_handle in(ioservice);

using namespace std;

int main()
{
    HANDLE pipe = INVALID_HANDLE_VALUE;
    try
    {
        //create pipe
        pipe = CreateNamedPipe(L"\\\\.\\pipe\\dsc",
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
            255, 50000, 50000, 0, 0);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            cerr << "CreateNamedPipe" << GetLastError() << endl;;
            return 1;
        }
        in.assign(pipe);
        std::cout << "Created pipe" << std::endl;

        OVERLAPPED overlapped = { 0 };
        overlapped.hEvent = CreateEvent(0, TRUE, FALSE, 0);
        if (ConnectNamedPipe(pipe, &overlapped) == FALSE)
        {
            unsigned error = GetLastError();
            if (error != ERROR_PIPE_CONNECTED &&
                error != ERROR_IO_PENDING)
            {
                cerr << "ConnectNamedPipe" << GetLastError() << endl;;
                return 1;
            }
        }
        WaitForSingleObject(overlapped.hEvent, INFINITE);
        CloseHandle(overlapped.hEvent);
        std::cout << "Pipe connected" << std::endl;

        for (int i = 0; i < 100; ++i)
        {
            boost::system::error_code error;
            unsigned n = i * 5;
            boost::asio::write(in, boost::asio::buffer((char*)&n, sizeof(unsigned)), boost::asio::transfer_all(), error);
            if (error)throw boost::system::system_error(error);
        }
        std::cout << "Sent data" << std::endl;

        FlushFileBuffers(pipe);

        DisconnectNamedPipe(pipe);

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cout << e.what() << std::endl;
        if (pipe != INVALID_HANDLE_VALUE)
            DisconnectNamedPipe(pipe);

        return -1;
    }
}

#else

#include <iostream>
#include <string>   

#include "cpprest/http_msg.h"
#include "cpprest/http_listener.h"

using namespace std;
using namespace utility;
using namespace web;
using namespace http;
using namespace http::experimental::listener;

int main (void)
{
    try
    {
#define DSC_FORCE_HTTP_LISTENER_NAMED_PIPE 1

#if defined (DSC_FORCE_HTTP_LISTENER_NAMED_PIPE)
        http_listener listener(U("http://pipe/dsc"));
#else
        http_listener listener(U("http://localhost:34568"));
#endif
        listener.support(methods::PUT, [](http_request request) {
            request.extract_string().then([=](string_t body) {
                http_response response;
                response.set_status_code(status_codes::OK);
                response.set_body(body);
                response.headers().add(U("DSC-Header"), U("Some Value"));
                request.reply(response);
            });
        });

        listener.open().wait();

        cout << "Press ENTER to exit." << endl;

        string line;
        getline(cin, line);
    }
    catch(const exception& e)
    {
        cerr << "Internal Error: " << e.what() << endl;
    }

    return 0;
}

#endif
//#define BOOST_ASIO_SAMPLE

#ifdef BOOST_ASIO_SAMPLE

#include <boost/asio.hpp>
#include <iostream>

using namespace std;
using namespace boost;

boost::asio::io_service ioservice;
boost::asio::windows::stream_handle in(ioservice);

int main()
{
    try
    {
        WaitNamedPipe(L"\\\\.\\pipe\\dsc", NMPWAIT_WAIT_FOREVER);

        std::cout << "Pipe avaible" << std::endl;
        HANDLE pipe = CreateFile(L"\\\\.\\pipe\\dsc",
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL
        );
        if (pipe == INVALID_HANDLE_VALUE)
        {
            cerr << "CreateNamedPipe: " << GetLastError() << endl;;
            return -1;
        }

        in.assign(pipe);
        std::cout << "Pipe connected" << std::endl;

        for (int i = 0; i < 100; ++i)
        {
            std::cout << "i: " << i << std::endl;
            boost::system::error_code error;
            unsigned n;
            asio::read(in, asio::buffer((char*)&n, sizeof(unsigned)), asio::transfer_all(), error);
            if (error)throw boost::system::system_error(error);
        }
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cout << e.what() << std::endl;
        return -1;
    }
}

#else

#include <iostream>
#include <cpprest/http_client.h>

using namespace std;

using namespace utility; 
using namespace web::http;
using namespace web::http::client;

int main()
{
    try
    {
#define DSC_FORCE_HTTP_LISTENER_NAMED_PIPE 1

#if defined (DSC_FORCE_HTTP_LISTENER_NAMED_PIPE)
        http_client client(U("http://pipe/dsc/metaconfiguration"));
#else
        http_client client(U("http://localhost:34568/metaconfiguration"));
#endif
        http_request request(methods::PUT);
        request.headers().add(U("Content-Type"), U("application/json"));
        web::json::value body;
        body[U("hello")] = web::json::value::string(U("there"));
        request.set_body(body);

        client.request(request).then([=](http_response response)
        {
            for (auto h : response.headers())
                ucout << h.first << U(": ") << h.second << std::endl;

            return response.extract_string(true);
        }).then([=](string_t response)
        {
            ucout << response << endl;
        }).wait();
    }
    catch (const std::exception &e)
    {
        cerr << e.what() << endl;
    }

    return 0;
}

#endif

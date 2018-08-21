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
        http_client np_client(U("http://localsocket/home/nam/socket/metaconfiguration"));

        http_request request(methods::PUT);
        request.headers().add(U("Content-Type"), U("application/json"));
        web::json::value body;
        body[U("hello")] = web::json::value::string(U("there"));
        request.set_body(body);

        np_client.request(request).then([=](http_response response)
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

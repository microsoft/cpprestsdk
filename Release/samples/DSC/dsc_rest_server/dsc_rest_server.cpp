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
        http_listener listener(U("http://socket-home-nam-socket"));

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


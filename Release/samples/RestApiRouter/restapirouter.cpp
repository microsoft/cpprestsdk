#include "apis.h"
#include <iostream>
#include <cpprest/http_listener.h>
using namespace utility;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;
class Server
{
private:
    static const void route_registeration()
    {
        rest_api_router::register_route<api1>("/api/v1/api1/action1", &api1::Action1);
        rest_api_router::register_route<api1>("/api/v1/api1/action2", &api1::Action2);
        rest_api_router::register_route<api2>("/api/v1/api2/action1", &api2::Action2);
        rest_api_router::register_route<api2>("/api/v1/api2/action2", &api2::Action2);
    }

public:
    const void Listen(void)
    {
        string_t baseUrl = "http://0.0.0.0:7777";
        http_listener baseListener(baseUrl);
        baseListener.support_rest_api_routing(route_registeration);
        try
        {
            baseListener.open();
            std::cout << "Server is running on http://0.0.0.0:7777" << std::endl;
            std::string line;
            std::getline(std::cin, line);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
};
int main()
{
    Server s;
    s.Listen();
    return 0;
}
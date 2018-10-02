#pragma once

#include <memory>
#include "cpprest/details/http_server.h"

namespace web
{
namespace http
{
namespace experimental
{
namespace details
{

utility::unique_ptr<http_server> make_http_httpsys_server();
utility::unique_ptr<http_server> make_http_asio_server();

}}}}

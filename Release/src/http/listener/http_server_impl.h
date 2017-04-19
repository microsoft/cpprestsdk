#pragma once

namespace web
{
namespace http
{
namespace experimental
{
namespace details
{

std::unique_ptr<http_server> make_http_httpsys_server();
std::unique_ptr<http_server> make_http_asio_server();

}}}}

#pragma once
#ifndef _APIS_SAMPLE
#define _APIS_SAMPLE
#endif
#include <cpprest/json.h>
#include <cpprest/details/http_helpers.h>
class api1
{
public:
   web::json::value Action1(const web::http::http_request &request)
   {
      web::json::value result;
      result["method"] = web::json::value::string(request.method());
      result["endpoint"] = web::json::value::string(request.relative_uri().to_string());
      result["body"] = request.extract_json().get();
      return result;
   }
   web::json::value Action2(const web::http::http_request &request)
   {
      web::json::value result;
      result["method"] = web::json::value::string(request.method());
      result["endpoint"] = web::json::value::string(request.relative_uri().to_string());
      result["body"] = request.extract_json().get();
      return result;
   }
};
class api2
{
public:
   web::json::value Action1(const web::http::http_request &request)
   {
      web::json::value result;
      result["method"] = web::json::value::string(request.method());
      result["endpoint"] = web::json::value::string(request.relative_uri().to_string());
      result["body"] = request.extract_json().get();
      return result;
   }
   web::json::value Action2(const web::http::http_request &request)
   {
      web::json::value result;
      result["method"] = web::json::value::string(request.method());
      result["endpoint"] = web::json::value::string(request.relative_uri().to_string());
      result["body"] = request.extract_json().get();
      return result;
   }
};
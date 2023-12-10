**cpprestsdk is in maintenance mode and we do not recommend its use in new projects. We will continue to fix critical bugs and address security issues.**

## Welcome!

The C++ REST SDK is a Microsoft project for cloud-based client-server communication in native code using a modern asynchronous C++ API design. This project aims to help C++ developers connect to and interact with services.  

## Getting Started

[![Vcpkg package](https://repology.org/badge/version-for-repo/vcpkg/cpprestsdk.svg)](https://repology.org/metapackage/cpprestsdk)</br>
[![Homebrew package](https://repology.org/badge/version-for-repo/homebrew/cpprestsdk.svg)](https://repology.org/metapackage/cpprestsdk)</br>
[![Ubuntu 18.04 package](https://repology.org/badge/version-for-repo/ubuntu_18_04/cpprestsdk.svg)](https://repology.org/metapackage/cpprestsdk)</br>
[![Fedora Rawhide package](https://repology.org/badge/version-for-repo/fedora_rawhide/cpprestsdk.svg)](https://repology.org/metapackage/cpprestsdk)</br>
[![openSUSE Tumbleweed package](https://repology.org/badge/version-for-repo/opensuse_tumbleweed/cpprestsdk.svg)](https://repology.org/metapackage/cpprestsdk)</br>
[![Debian Testing package](https://repology.org/badge/version-for-repo/debian_testing/cpprestsdk.svg)](https://repology.org/metapackage/cpprestsdk)</br>

[![Build Status](https://dev.azure.com/vclibs/cpprestsdk/_apis/build/status/Microsoft.cpprestsdk.Ubuntu)](https://dev.azure.com/vclibs/cpprestsdk/_build/latest?definitionId=1)

With [vcpkg](https://github.com/Microsoft/vcpkg) on Windows
```
PS> vcpkg install cpprestsdk cpprestsdk:x64-windows
```
With [apt-get](https://launchpad.net/ubuntu/+source/casablanca/2.8.0-2build2) on Debian/Ubuntu
```
$ sudo apt-get install libcpprest-dev
```
With [dnf](https://apps.fedoraproject.org/packages/cpprest) on Fedora
```
$ sudo dnf install cpprest-devel
```
With [brew](https://github.com/Homebrew/homebrew-core/blob/master/Formula/cpprestsdk.rb) on OSX
```
$ brew install cpprestsdk
```
With [NuGet](https://www.nuget.org/packages/cpprestsdk.android/) on Windows for Android
```
PM> Install-Package cpprestsdk.android
```
For other platforms, install options, how to build from source, and more, take a look at our [Documentation](https://github.com/Microsoft/cpprestsdk/wiki).

Once you have the library, look at our [tutorial](https://github.com/Microsoft/cpprestsdk/wiki/Getting-Started-Tutorial) to use the http_client. It walks through how to setup a project to use the C++ Rest SDK and make a basic Http request.

To use from CMake:
```cmake
cmake_minimum_required(VERSION 3.9)
project(main)

find_package(cpprestsdk REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE cpprestsdk::cpprest)
```

## What's in the SDK:

*   Features - HTTP client/server, JSON, URI, asynchronous streams, WebSockets client, oAuth
*   PPL Tasks - A powerful model for composing asynchronous operations based on C++ 11 features
*   Platforms - Windows desktop, Windows Store (UWP), Linux, OS X, Unix, iOS, and Android
*   Support for [Visual Studio 2015 and 2017](https://visualstudio.microsoft.com/) with debugger visualizers

## Contribute Back!

Is there a feature missing that you'd like to see, or found a bug that you have a fix for? Or do you have an idea or just interest in helping out in building the library? Let us know and we'd love to work with you. For a good starting point on where we are headed and feature ideas, take a look at our [requested features and bugs](https://github.com/Microsoft/cpprestsdk/issues).  

Big or small we'd like to take your [contributions](https://github.com/Microsoft/cpprestsdk/wiki/Make-a-contribution-and-report-issues) back to help improve the C++ Rest SDK for everyone.

## Having Trouble?

We'd love to get your review score, whether good or bad, but even more than that, we want to fix your problem. If you submit your issue as a Review, we won't be able to respond to your problem and ask any follow-up questions that may be necessary. The most efficient way to do that is to open an issue in our [issue tracker](https://github.com/Microsoft/cpprestsdk/issues).  

### Quick Links

*   [FAQ](https://github.com/Microsoft/cpprestsdk/wiki/FAQ)
*   [Documentation](https://github.com/Microsoft/cpprestsdk/wiki)
*   [Issue Tracker](https://github.com/Microsoft/cpprestsdk/issues)

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Rest API Router:
You can find sources under "Release/samples/RestApiRouter". I've created a sample only for showing you how easy it is to use,
I'm using gcc under WSL (debian) and Postman for testing ednpoints:
Compile & Build:
```bash
g++ -std=c++11 restapirouter.cpp -o sample -lcrypto -lcpprest -lpthread
```
```bash
./sample
```
![image](https://user-images.githubusercontent.com/56490683/152418716-dcb77f03-4fa1-4891-9714-cfdda05b919e.png)

Use Postman to send a request (Method: GET , Body: {"name":"test"}, Endpoint: /api/v1/api1/action1):

![image](https://user-images.githubusercontent.com/56490683/152419500-1c3f3515-45b5-42a8-9c60-0be4a124db7f.png)

Use Postman to send a request (Method: POST , Body: {"name":"test", "username":"testuser"}, Endpoint: /api/v1/api2/action2):

![image](https://user-images.githubusercontent.com/56490683/152419758-a303b6ae-da49-445a-967e-ba6e82529ae4.png)

What if we requested a not defined endpoint:
Use Postman to send a request (Method: POST , Body: {"name":"test", "username":"testuser"}, Endpoint: /api/v1/api2/**action3**):

![image](https://user-images.githubusercontent.com/56490683/152419929-ba197ee6-112d-458d-8259-ff1035b57b36.png)

Thats all.

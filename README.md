## Welcome!

The C++ REST SDK is a Microsoft project for cloud-based client-server communication in native code using a modern asynchronous C++ API design. This project aims to help C++ developers connect to and interact with services.  

## Getting Started

With [vcpkg](https://github.com/Microsoft/vcpkg) on Windows
```
PS> vcpkg install cpprestsdk cpprestsdk:x64-windows
```
With [apt-get](https://launchpad.net/ubuntu/+source/casablanca/2.8.0-2build2) on Debian/Ubuntu
```
$ sudo apt-get install libcpprest-dev
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
cmake_minimum_required(VERSION 3.7)
project(main)

find_package(cpprestsdk REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE cpprestsdk::cpprest)
```

## What's in the SDK:

*   Features - HTTP client/server, JSON, URI, asynchronous streams, WebSockets client, oAuth
*   PPL Tasks - A powerful model for composing asynchronous operations based on C++ 11 features
*   Platforms - Windows desktop, Windows Store, Windows Phone, Ubuntu, OS X, iOS, and Android
*   Support for Visual Studio 2012, 2013, and 2015 with debugger visualizers
*   NuGet package with binaries for Windows and Android platforms

## Contribute Back!

Is there a feature missing that you'd like to see, or found a bug that you have a fix for? Or do you have an idea or just interest in helping out in building the library? Let us know and we'd love to work with you. For a good starting point on where we are headed and feature ideas, take a look at our [requested features and bugs](https://github.com/Microsoft/cpprestsdk/issues).  

Big or small we'd like to take your [contributions](https://github.com/Microsoft/cpprestsdk/wiki/Make-a-contribution-and-report-issues) back to help improve the C++ Rest SDK for everyone. If interested contact us askcasablanca at Microsoft dot com.  

## Having Trouble?

We'd love to get your review score, whether good or bad, but even more than that, we want to fix your problem. If you submit your issue as a Review, we won't be able to respond to your problem and ask any follow-up questions that may be necessary. The most efficient way to do that is to open a an issue in our [issue tracker](https://github.com/Microsoft/cpprestsdk/issues).  

### Quick Links

*   [FAQ](https://github.com/Microsoft/cpprestsdk/wiki/FAQ)
*   [Documentation](https://github.com/Microsoft/cpprestsdk/wiki)
*   [Issue Tracker](https://github.com/Microsoft/cpprestsdk/issues)
*   Directly contact us: <askcasablanca@microsoft.com>

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

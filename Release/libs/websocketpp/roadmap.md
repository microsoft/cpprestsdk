Complete & Tested:
- Server and client roles pass all Autobahn v0.5.9 test suite tests strictly
- Streaming UTF8 validation
- random number generation
- iostream based transport
- C++11 support
- LLVM/Clang support
- GCC support
- 64 bit support
- 32 bit support
- Logging
- Client role
- message_handler
- ping_handler
- pong_handler
- open_handler
- close_handler
- echo_server & echo_server_tls

Implemented, needs more testing
- TLS support
- External io_service support
- socket_init_handler
- tls_init_handler
- tcp_init_handler
- exception/error handling
- Subprotocol negotiation
- Hybi 00/Hixie 76 legacy protocol support
- Performance tuning
- Outgoing Proxy Support
- PowerPC support
- Visual Studio / Windows support
- Timeouts
- CMake build/install support

- validate_handler
- http_handler

Future feature roadmap
- Extension support
- permessage_compress extension
- Message buffer pool
- flow control
- tutorials & documentation

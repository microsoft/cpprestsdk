/*
 * Copyright (c) 2013, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WEBSOCKETPP_LOGGER_STUB_HPP
#define WEBSOCKETPP_LOGGER_STUB_HPP

#include <iostream>

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/logger/levels.hpp>

namespace websocketpp {
namespace log {

/// Stub logger that ignores all input
class stub {
public:
    explicit stub(std::ostream * out) {}
    stub(level c, std::ostream * out) {}
    _WEBSOCKETPP_CONSTEXPR_TOKEN_ stub() {}

    void set_channels(level channels) {}
    void clear_channels(level channels) {}

    void write(level channel, std::string const & msg) {}
    void write(level channel, char const * msg) {}

    _WEBSOCKETPP_CONSTEXPR_TOKEN_ bool static_test(level channel) const {
        return false;
    }
    bool dynamic_test(level channel) {
        return false;
    }
};

} // log
} // websocketpp

#endif // WEBSOCKETPP_LOGGER_STUB_HPP

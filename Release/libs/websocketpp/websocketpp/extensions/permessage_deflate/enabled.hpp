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

#ifndef WEBSOCKETPP_PROCESSOR_EXTENSION_PERMESSAGEDEFLATE_HPP
#define WEBSOCKETPP_PROCESSOR_EXTENSION_PERMESSAGEDEFLATE_HPP


#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/platforms.hpp>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/error.hpp>

#include <websocketpp/extensions/extension.hpp>

#include "zlib.h"

#include <algorithm>
#include <string>
#include <vector>

namespace websocketpp {
namespace extensions {

/// Implimentation of the draft permessage-deflate WebSocket extension
/**
 * ### permessage-deflate interface
 *
 * **is_implemented**\n
 * `bool is_implemented()`\n
 * Returns whether or not the object impliments the extension or not
 *
 * **is_enabled**\n
 * `bool is_enabled()`\n
 * Returns whether or not the extension was negotiated for the current
 * connection
 *
 * **generate_offer**\n
 * `std::string generate_offer() const`\n
 * Create an extension offer string based on local policy
 *
 * **validate_response**\n
 * `lib::error_code validate_response(http::attribute_list const & response)`\n
 * Negotiate the parameters of extension use
 *
 * **negotiate**\n
 * `err_str_pair negotiate(http::attribute_list const & attributes)`\n
 * Negotiate the parameters of extension use
 *
 * **compress**\n
 * `lib::error_code compress(std::string const & in, std::string & out)`\n
 * Compress the bytes in `in` and append them to `out`
 *
 * **decompress**\n
 * `lib::error_code decompress(uint8_t const * buf, size_t len, std::string &
 * out)`\n
 * Decompress `len` bytes from `buf` and append them to string `out`
 */
namespace permessage_deflate {

/// Permessage deflate error values
namespace error {
enum value {
    /// Catch all
    general = 1,

    /// Invalid extension attributes
    invalid_attributes,

    /// Invalid extension attribute value
    invalid_attribute_value,

    /// Invalid megotiation mode
    invalid_mode,

    /// Unsupported extension attributes
    unsupported_attributes,

    /// Invalid value for max_window_bits
    invalid_max_window_bits,

    /// ZLib Error
    zlib_error,

    /// Uninitialized
    uninitialized,
};

/// Permessage-deflate error category
class category : public lib::error_category {
public:
    category() {}

    char const * name() const _WEBSOCKETPP_NOEXCEPT_TOKEN_ {
        return "websocketpp.extension.permessage-deflate";
    }

    std::string message(int value) const {
        switch(value) {
            case general:
                return "Generic permessage-compress error";
            case invalid_attributes:
                return "Invalid extension attributes";
            case invalid_attribute_value:
                return "Invalid extension attribute value";
            case invalid_mode:
                return "Invalid permessage-deflate negotiation mode";
            case unsupported_attributes:
                return "Unsupported extension attributes";
            case invalid_max_window_bits:
                return "Invalid value for max_window_bits";
            case zlib_error:
                return "A zlib function returned an error";
            case uninitialized:
                return "Object must be initialized before use";
            default:
                return "Unknown permessage-compress error";
        }
    }
};

/// Get a reference to a static copy of the permessage-deflate error category
lib::error_category const & get_category() {
    static category instance;
    return instance;
}

/// Create an error code in the permessage-deflate category
lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace permessage_deflate
} // namespace extensions
} // namespace websocketpp

_WEBSOCKETPP_ERROR_CODE_ENUM_NS_START_
template<> struct is_error_code_enum
    <websocketpp::extensions::permessage_deflate::error::value>
{
    static bool const value = true;
};
_WEBSOCKETPP_ERROR_CODE_ENUM_NS_END_
namespace websocketpp {
namespace extensions {
namespace permessage_deflate {

/// Default value for s2c_max_window_bits as defined by RFC6455
static uint8_t const default_s2c_max_window_bits = 15;
/// Minimum value for s2c_max_window_bits as defined by RFC6455
static uint8_t const min_s2c_max_window_bits = 8;
/// Maximum value for s2c_max_window_bits as defined by RFC6455
static uint8_t const max_s2c_max_window_bits = 15;

/// Default value for c2s_max_window_bits as defined by RFC6455
static uint8_t const default_c2s_max_window_bits = 15;
/// Minimum value for c2s_max_window_bits as defined by RFC6455
static uint8_t const min_c2s_max_window_bits = 8;
/// Maximum value for c2s_max_window_bits as defined by RFC6455
static uint8_t const max_c2s_max_window_bits = 15;

namespace mode {
enum value {
    /// Accept any value the remote endpoint offers
    accept = 1,
    /// Decline any value the remote endpoint offers. Insist on defaults.
    decline,
    /// Use the largest value common to both offers
    largest,
    /// Use the smallest value common to both offers
    smallest
};
} // namespace mode

template <typename config>
class enabled {
public:
    enabled()
      : m_enabled(false)
      , m_s2c_no_context_takeover(false)
      , m_c2s_no_context_takeover(false)
      , m_s2c_max_window_bits(15)
      , m_c2s_max_window_bits(15)
      , m_s2c_max_window_bits_mode(mode::accept)
      , m_c2s_max_window_bits_mode(mode::accept)
      , m_initialized(false)
      , m_compress_buffer_size(16384)
    {
        m_dstate.zalloc = Z_NULL;
        m_dstate.zfree = Z_NULL;
        m_dstate.opaque = Z_NULL;

        m_istate.zalloc = Z_NULL;
        m_istate.zfree = Z_NULL;
        m_istate.opaque = Z_NULL;
        m_istate.avail_in = 0;
        m_istate.next_in = Z_NULL;
    }

    ~enabled() {
        if (!m_initialized) {
            return;
        }

        int ret = deflateEnd(&m_dstate);

        if (ret != Z_OK) {
            //std::cout << "error cleaning up zlib compression state"
            //          << std::endl;
        }

        ret = inflateEnd(&m_istate);

        if (ret != Z_OK) {
            //std::cout << "error cleaning up zlib decompression state"
            //          << std::endl;
        }
    }

    /// Initialize zlib state
    /**
     *
     * @todo memory level, strategy, etc are hardcoded
     * @todo server detection is hardcoded
     */
    lib::error_code init() {
        uint8_t deflate_bits;
        uint8_t inflate_bits;

        if (true /*is_server*/) {
            deflate_bits = m_s2c_max_window_bits;
            inflate_bits = m_c2s_max_window_bits;
        } else {
            deflate_bits = m_c2s_max_window_bits;
            inflate_bits = m_s2c_max_window_bits;
        }

        int ret = deflateInit2(
            &m_dstate,
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            -1*deflate_bits,
            8, // memory level 1-9
            /*Z_DEFAULT_STRATEGY*/Z_FIXED
        );

        if (ret != Z_OK) {
            return make_error_code(error::zlib_error);
        }

        ret = inflateInit2(
            &m_istate,
            -1*inflate_bits
        );

        if (ret != Z_OK) {
            return make_error_code(error::zlib_error);
        }

        m_compress_buffer.reset(new unsigned char[m_compress_buffer_size]);
        m_initialized = true;
        return lib::error_code();
    }

    /// Test if this object impliments the permessage-deflate specification
    /**
     * Because this object does impliment it, it will always return true.
     *
     * @return Whether or not this object impliments permessage-deflate
     */
    bool is_implemented() const {
        return true;
    }

    /// Test if the extension was negotiated for this connection
    /**
     * Retrieves whether or not this extension is in use based on the initial
     * handshake extension negotiations.
     *
     * @return Whether or not the extension is in use
     */
    bool is_enabled() const {
        return m_enabled;
    }

    /// Reset server's outgoing LZ77 sliding window for each new message
    /**
     * Enabling this setting will cause the server's compressor to reset the
     * compression state (the LZ77 sliding window) for every message. This
     * means that the compressor will not look back to patterns in previous
     * messages to improve compression. This will reduce the compression
     * efficiency for large messages somewhat and small messages drastically.
     *
     * This option may reduce server compressor memory usage and client
     * decompressor memory usage.
     * @todo Document to what extent memory usage will be reduced
     *
     * For clients, this option is dependent on server support. Enabling it
     * via this method does not guarantee that it will be successfully
     * negotiated, only that it will be requested.
     *
     * For servers, no client support is required. Enabling this option on a
     * server will result in its use. The server will signal to clients that
     * the option will be in use so they can optimize resource usage if they
     * are able.
     */
    void enable_s2c_no_context_takeover() {
        m_s2c_no_context_takeover = true;
    }

    /// Reset client's outgoing LZ77 sliding window for each new message
    /**
     * Enabling this setting will cause the client's compressor to reset the
     * compression state (the LZ77 sliding window) for every message. This
     * means that the compressor will not look back to patterns in previous
     * messages to improve compression. This will reduce the compression
     * efficiency for large messages somewhat and small messages drastically.
     *
     * This option may reduce client compressor memory usage and server
     * decompressor memory usage.
     * @todo Document to what extent memory usage will be reduced
     *
     * This option is supported by all compliant clients and servers. Enabling
     * it via either endpoint should be sufficient to ensure it is used.
     */
    void enable_c2s_no_context_takeover() {
        m_c2s_no_context_takeover = true;
    }

    /// Limit server LZ77 sliding window size
    /**
     * The bits setting is the base 2 logarithm of the maximum window size that
     * the server must use to compress outgoing messages. The permitted range
     * is 8 to 15 inclusive. 8 represents a 256 byte window and 15 a 32KiB
     * window. The default setting is 15.
     *
     * Mode Options:
     * - accept: Accept whatever the remote endpoint offers.
     * - decline: Decline any offers to deviate from the defaults
     * - largest: Accept largest window size acceptable to both endpoints
     * - smallest: Accept smallest window size acceptiable to both endpoints
     *
     * This setting is dependent on server support. A client requesting this
     * setting may be rejected by the server or have the exact value used
     * adjusted by the server. A server may unilaterally set this value without
     * client support.
     *
     * @param bits The size to request for the outgoing window size
     * @param mode The mode to use for negotiating this parameter
     * @return A status code
     */
    lib::error_code set_s2c_max_window_bits(uint8_t bits, mode::value mode) {
        if (bits < min_s2c_max_window_bits || bits > max_s2c_max_window_bits) {
            return error::make_error_code(error::invalid_max_window_bits);
        }
        m_s2c_max_window_bits = bits;
        m_s2c_max_window_bits_mode = mode;

        return lib::error_code();
    }

    /// Limit client LZ77 sliding window size
    /**
     * The bits setting is the base 2 logarithm of the window size that the
     * client must use to compress outgoing messages. The permitted range is 8
     * to 15 inclusive. 8 represents a 256 byte window and 15 a 32KiB window.
     * The default setting is 15.
     *
     * Mode Options:
     * - accept: Accept whatever the remote endpoint offers.
     * - decline: Decline any offers to deviate from the defaults
     * - largest: Accept largest window size acceptable to both endpoints
     * - smallest: Accept smallest window size acceptiable to both endpoints
     *
     * This setting is dependent on client support. A client may limit its own
     * outgoing window size unilaterally. A server may only limit the client's
     * window size if the remote client supports that feature.
     *
     * @param bits The size to request for the outgoing window size
     * @param mode The mode to use for negotiating this parameter
     * @return A status code
     */
    lib::error_code set_c2s_max_window_bits(uint8_t bits, mode::value mode) {
        if (bits < min_c2s_max_window_bits || bits > max_c2s_max_window_bits) {
            return error::make_error_code(error::invalid_max_window_bits);
        }
        m_c2s_max_window_bits = bits;
        m_c2s_max_window_bits_mode = mode;

        return lib::error_code();
    }

    /// Generate extension offer
    /**
     * Creates an offer string to include in the Sec-WebSocket-Extensions
     * header of outgoing client requests.
     *
     * @return A WebSocket extension offer string for this extension
     */
    std::string generate_offer() const {
        return "";
    }

    /// Validate extension response
    /**
     * Confirm that the server has negotiated settings compatible with our
     * original offer and apply those settings to the extension state.
     *
     * @param response The server response attribute list to validate
     * @return Validation error or 0 on success
     */
    lib::error_code validate_offer(http::attribute_list const & response) {
        return make_error_code(error::general);
    }

    /// Negotiate extension
    /**
     * Confirm that the client's extension negotiation offer has settings
     * compatible with local policy. If so, generate a reply and apply those
     * settings to the extension state.
     *
     * @param offer Attribute from client's offer
     * @return Status code and value to return to remote endpoint
     */
    err_str_pair negotiate(http::attribute_list const & offer) {
        err_str_pair ret;

        http::attribute_list::const_iterator it;
        for (it = offer.begin(); it != offer.end(); ++it) {
            if (it->first == "s2c_no_context_takeover") {
                negotiate_s2c_no_context_takeover(it->second,ret.first);
            } else if (it->first == "c2s_no_context_takeover") {
                negotiate_c2s_no_context_takeover(it->second,ret.first);
            } else if (it->first == "s2c_max_window_bits") {
                negotiate_s2c_max_window_bits(it->second,ret.first);
            } else if (it->first == "c2s_max_window_bits") {
                negotiate_c2s_max_window_bits(it->second,ret.first);
            } else {
                ret.first = make_error_code(error::invalid_attributes);
            }

            if (ret.first) {
                break;
            }
        }

        if (ret.first == lib::error_code()) {
            m_enabled = true;
            ret.second = generate_response();
        }

        return ret;
    }

    /// Compress bytes
    /**
     * @param [in] in String to compress
     * @param [out] out String to append compressed bytes to
     * @return Error or status code
     */
    lib::error_code compress(std::string const & in, std::string & out) {
        if (!m_initialized) {
            return make_error_code(error::uninitialized);
        }

        size_t output;

        m_dstate.avail_out = m_compress_buffer_size;
        m_dstate.next_in = (unsigned char *)(const_cast<char *>(in.data()));

        do {
            // Output to local buffer
            m_dstate.avail_out = m_compress_buffer_size;
            m_dstate.next_out = m_compress_buffer.get();

            deflate(&m_dstate, Z_SYNC_FLUSH);

            output = m_compress_buffer_size - m_dstate.avail_out;

            out.append((char *)(m_compress_buffer.get()),output);
        } while (m_dstate.avail_out == 0);

        return lib::error_code();
    }

    /// Decompress bytes
    /**
     * @param buf Byte buffer to decompress
     * @param len Length of buf
     * @param out String to append decompressed bytes to
     * @return Error or status code
     */
    lib::error_code decompress(uint8_t const * buf, size_t len, std::string &
        out)
    {
        if (!m_initialized) {
            return make_error_code(error::uninitialized);
        }

        int ret;

        m_istate.avail_in = len;
        m_istate.next_in = const_cast<unsigned char *>(buf);

        do {
            m_istate.avail_out = m_compress_buffer_size;
            m_istate.next_out = m_compress_buffer.get();

            ret = inflate(&m_istate, Z_SYNC_FLUSH);

            if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                return make_error_code(error::zlib_error);
            }

            out.append(
                reinterpret_cast<char *>(m_compress_buffer.get()),
                m_compress_buffer_size - m_istate.avail_out
            );
        } while (m_istate.avail_out == 0);

        return lib::error_code();
    }
private:
    /// Generate negotiation response
    /**
     * @return Generate extension negotiation reponse string to send to client
     */
    std::string generate_response() {
        std::string ret = "permessage-deflate";

        if (m_s2c_no_context_takeover) {
            ret += "; s2c_no_context_takeover";
        }

        if (m_c2s_no_context_takeover) {
            ret += "; c2s_no_context_takeover";
        }

        if (m_s2c_max_window_bits < default_s2c_max_window_bits) {
            std::stringstream s;
            s << int(m_s2c_max_window_bits);
            ret += "; s2c_max_window_bits="+s.str();
        }

        if (m_c2s_max_window_bits < default_c2s_max_window_bits) {
            std::stringstream s;
            s << int(m_c2s_max_window_bits);
            ret += "; c2s_max_window_bits="+s.str();
        }

        return ret;
    }

    /// Negotiate s2c_no_context_takeover attribute
    /**
     * @param [in] value The value of the attribute from the offer
     * @param [out] ec A reference to the error code to return errors via
     */
    void negotiate_s2c_no_context_takeover(std::string const & value,
        lib::error_code & ec)
    {
        if (!value.empty()) {
            ec = make_error_code(error::invalid_attribute_value);
            return;
        }

        m_s2c_no_context_takeover = true;
    }

    /// Negotiate c2s_no_context_takeover attribute
    /**
     * @param [in] value The value of the attribute from the offer
     * @param [out] ec A reference to the error code to return errors via
     */
    void negotiate_c2s_no_context_takeover(std::string const & value,
        lib::error_code & ec)
    {
        if (!value.empty()) {
            ec = make_error_code(error::invalid_attribute_value);
            return;
        }

        m_c2s_no_context_takeover = true;
    }

    /// Negotiate s2c_max_window_bits attribute
    /**
     * When this method starts, m_s2c_max_window_bits will contain the server's
     * preferred value and m_s2c_max_window_bits_mode will contain the mode the
     * server wants to use to for negotiation. `value` contains the value the
     * client requested that we use.
     *
     * options:
     * - decline (refuse to use the attribute)
     * - accept (use whatever the client says)
     * - largest (use largest possible value)
     * - smallest (use smallest possible value)
     *
     * @param [in] value The value of the attribute from the offer
     * @param [out] ec A reference to the error code to return errors via
     */
    void negotiate_s2c_max_window_bits(std::string const & value,
        lib::error_code & ec)
    {
        uint8_t bits = uint8_t(atoi(value.c_str()));

        if (bits < min_s2c_max_window_bits || bits > max_s2c_max_window_bits) {
            ec = make_error_code(error::invalid_attribute_value);
            m_s2c_max_window_bits = default_s2c_max_window_bits;
            return;
        }

        switch (m_s2c_max_window_bits_mode) {
            case mode::decline:
                m_s2c_max_window_bits = default_s2c_max_window_bits;
                break;
            case mode::accept:
                m_s2c_max_window_bits = bits;
                break;
            case mode::largest:
                m_s2c_max_window_bits = std::min(bits,m_s2c_max_window_bits);
                break;
            case mode::smallest:
                m_s2c_max_window_bits = min_s2c_max_window_bits;
                break;
            default:
                ec = make_error_code(error::invalid_mode);
                m_s2c_max_window_bits = default_s2c_max_window_bits;
        }
    }

    /// Negotiate c2s_max_window_bits attribute
    /**
     * When this method starts, m_c2s_max_window_bits and m_c2s_max_window_mode
     * will contain the server's preferred values for window size and
     * negotiation mode.
     *
     * options:
     * - decline (refuse to use the attribute)
     * - accept (use whatever the client says)
     * - largest (use largest possible value)
     * - smallest (use smallest possible value)
     *
     * @param [in] value The value of the attribute from the offer
     * @param [out] ec A reference to the error code to return errors via
     */
    void negotiate_c2s_max_window_bits(std::string const & value,
            lib::error_code & ec)
    {
        uint8_t bits = uint8_t(atoi(value.c_str()));

        if (value.empty()) {
            bits = default_c2s_max_window_bits;
        } else if (bits < min_c2s_max_window_bits ||
                   bits > max_c2s_max_window_bits)
        {
            ec = make_error_code(error::invalid_attribute_value);
            m_c2s_max_window_bits = default_c2s_max_window_bits;
            return;
        }

        switch (m_c2s_max_window_bits_mode) {
            case mode::decline:
                m_c2s_max_window_bits = default_c2s_max_window_bits;
                break;
            case mode::accept:
                m_c2s_max_window_bits = bits;
                break;
            case mode::largest:
                m_c2s_max_window_bits = std::min(bits,m_c2s_max_window_bits);
                break;
            case mode::smallest:
                m_c2s_max_window_bits = min_c2s_max_window_bits;
                break;
            default:
                ec = make_error_code(error::invalid_mode);
                m_c2s_max_window_bits = default_c2s_max_window_bits;
        }
    }

    bool m_enabled;
    bool m_s2c_no_context_takeover;
    bool m_c2s_no_context_takeover;
    uint8_t m_s2c_max_window_bits;
    uint8_t m_c2s_max_window_bits;
    mode::value m_s2c_max_window_bits_mode;
    mode::value m_c2s_max_window_bits_mode;

    bool m_initialized;
    size_t m_compress_buffer_size;
    lib::unique_ptr_uchar_array m_compress_buffer;
    z_stream m_dstate;
    z_stream m_istate;
};

} // namespace permessage_deflate
} // namespace extensions
} // namespace websocketpp

#endif // WEBSOCKETPP_PROCESSOR_EXTENSION_PERMESSAGEDEFLATE_HPP

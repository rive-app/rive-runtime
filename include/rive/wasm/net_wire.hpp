#ifndef _RIVE_WASM_NET_WIRE_HPP_
#define _RIVE_WASM_NET_WIRE_HPP_

#include "rive/scriptnet/http.hpp"
#include "rive/span.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace rive
{

/// fetch() across the module boundary: the request rides rive_net_v1.fetch
/// (module to host), the response the module's host_fetch_resolved export
/// (host to module). Each is one byte stream of little-endian u32 counts and
/// lengths, every string or byte run after its length:
///   request:  timeoutMs, method, url, header count, (name, value)*, body
///   response: status, statusText, url, header count, (name, value)*, body
/// The module side is script controlled, so decode checks every length
/// against what is left.
struct NetWire
{
    /// The stream's length, to size the buffer encode writes.
    template <typename Message> static size_t encodedSize(const Message& value)
    {
        Writer writer;
        writer.message(value);
        return writer.size;
    }

    /// Writes exactly encodedSize(value) bytes to out.
    template <typename Message>
    static void encode(const Message& value, uint8_t* out)
    {
        Writer writer;
        writer.out = out;
        writer.message(value);
    }

    /// False on a truncated or inconsistent stream.
    static bool decode(Span<const uint8_t> bytes,
                       scriptnet::HttpRequest& request)
    {
        Reader reader(bytes);
        return reader.u32(request.timeoutMs) && reader.run(request.method) &&
               reader.run(request.url) && reader.headers(request.headers) &&
               reader.run(request.body) && reader.atEnd();
    }

private:
    // Only measures while out is null.
    struct Writer
    {
        uint8_t* out = nullptr;
        size_t size = 0;

        void bytes(const void* data, size_t length)
        {
            if (out != nullptr && length > 0)
            {
                memcpy(out + size, data, length);
            }
            size += length;
        }
        void u32(uint32_t value)
        {
            uint8_t le[4] = {(uint8_t)value,
                             (uint8_t)(value >> 8),
                             (uint8_t)(value >> 16),
                             (uint8_t)(value >> 24)};
            bytes(le, 4);
        }
        template <typename Container> void run(const Container& value)
        {
            u32((uint32_t)value.size());
            bytes(value.data(), value.size());
        }
        void headers(const scriptnet::HttpHeaders& headers)
        {
            u32((uint32_t)headers.size());
            for (const scriptnet::HttpHeader& header : headers)
            {
                run(header.name);
                run(header.value);
            }
        }
        void message(const scriptnet::HttpRequest& request)
        {
            u32(request.timeoutMs);
            run(request.method);
            run(request.url);
            headers(request.headers);
            run(request.body);
        }
        void message(const scriptnet::HttpResponse& response)
        {
            u32(response.status);
            run(response.statusText);
            run(response.url);
            headers(response.headers);
            run(response.body);
        }
    };

    struct Reader
    {
        explicit Reader(Span<const uint8_t> bytes) :
            m_data(bytes.data()), m_left(bytes.size())
        {}

        bool atEnd() const { return m_left == 0; }

        bool u32(uint32_t& value)
        {
            if (m_left < 4)
            {
                return false;
            }
            value = (uint32_t)m_data[0] | ((uint32_t)m_data[1] << 8) |
                    ((uint32_t)m_data[2] << 16) | ((uint32_t)m_data[3] << 24);
            m_data += 4;
            m_left -= 4;
            return true;
        }
        template <typename Container> bool run(Container& value)
        {
            uint32_t length = 0;
            if (!u32(length) || length > m_left)
            {
                return false;
            }
            value.assign((const typename Container::value_type*)m_data,
                         (const typename Container::value_type*)m_data +
                             length);
            m_data += length;
            m_left -= length;
            return true;
        }
        bool headers(scriptnet::HttpHeaders& headers)
        {
            uint32_t count = 0;
            // Every header is at least its two lengths, which bounds a
            // crafted count before anything is reserved.
            if (!u32(count) || count > m_left / 8)
            {
                return false;
            }
            headers.resize(count);
            for (scriptnet::HttpHeader& header : headers)
            {
                if (!run(header.name) || !run(header.value))
                {
                    return false;
                }
            }
            return true;
        }

    private:
        const uint8_t* m_data;
        size_t m_left;
    };
};

} // namespace rive

#endif

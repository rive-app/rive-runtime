#ifndef _RIVE_SIGNED_CONTENT_HEADER_HPP_
#define _RIVE_SIGNED_CONTENT_HEADER_HPP_

#include "rive/span.hpp"
#include <cstdint>
#include <cstddef>

namespace rive
{

/// Size of the signature in bytes (hydro_sign_BYTES from libhydrogen).
static constexpr size_t kSignatureSize = 64;

/// Lightweight view into signed content data with a header.
/// Does not copy the underlying data - just provides accessors.
///
/// This envelope is shared by all text-backed assets (ScriptAsset for Luau
/// bytecode, ShaderAsset for RSTB blobs). The inner content type is
/// determined by the concrete asset class, not by this header.
///
/// Header format:
///   [flags:1] [signature:64 if signed] [content:N]
///
/// Flags byte layout:
///   Bits 0-6: Version number (0-127)
///   Bit 7: isSigned flag
class SignedContentHeader
{
public:
    /// Constructs a SignedContentHeader from raw data (header + content).
    /// The data must outlive this object.
    explicit SignedContentHeader(Span<const uint8_t> data) : m_data(data)
    {
        if (!data.empty())
        {
            m_flags = data[0];
        }
    }

    /// @returns true if the header indicates the content is signed
    bool isSigned() const { return (m_flags & 0x80) != 0; }

    /// @returns the version number from the header (0-127)
    uint8_t version() const { return m_flags & 0x7F; }

    /// @returns true if the data is valid (has at least the header byte and
    /// enough bytes for signature if signed)
    bool isValid() const { return m_data.size() >= contentOffset(); }

    /// @returns the offset where content starts (1 if unsigned, 65 if signed)
    size_t contentOffset() const
    {
        return isSigned() ? (1 + kSignatureSize) : 1;
    }

    /// @returns a span of the signature bytes, or empty span if not signed
    Span<const uint8_t> signature() const
    {
        if (!isSigned() || m_data.size() < 1 + kSignatureSize)
        {
            return Span<const uint8_t>();
        }
        return Span<const uint8_t>(m_data.data() + 1, kSignatureSize);
    }

    /// @returns a span of the actual content (without header)
    Span<const uint8_t> content() const
    {
        size_t offset = contentOffset();
        if (m_data.size() < offset)
        {
            return Span<const uint8_t>();
        }
        return Span<const uint8_t>(m_data.data() + offset,
                                   m_data.size() - offset);
    }

private:
    Span<const uint8_t> m_data;
    uint8_t m_flags = 0;
};

} // namespace rive
#endif

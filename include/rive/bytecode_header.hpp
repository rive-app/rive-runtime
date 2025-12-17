#ifndef _RIVE_BYTECODE_HEADER_HPP_
#define _RIVE_BYTECODE_HEADER_HPP_

#include "rive/span.hpp"
#include <cstdint>
#include <cstddef>

namespace rive
{

/// Size of the signature in bytes (hydro_sign_BYTES from libhydrogen).
static constexpr size_t kSignatureSize = 64;

/// Lightweight view into bytecode data with header.
/// Does not copy the underlying data - just provides accessors.
///
/// Header format:
///   [flags:1] [signature:64 if signed] [luau_bytecode:N]
///
/// Flags byte layout:
///   Bits 0-6: Version number (0-127)
///   Bit 7: isSigned flag
class BytecodeHeader
{
public:
    /// Constructs a BytecodeHeader from raw data (header + bytecode).
    /// The data must outlive this object.
    explicit BytecodeHeader(Span<const uint8_t> data) : m_data(data)
    {
        if (!data.empty())
        {
            m_flags = data[0];
        }
    }

    /// @returns true if the header indicates the bytecode is signed
    bool isSigned() const { return (m_flags & 0x80) != 0; }

    /// @returns the version number from the header (0-127)
    uint8_t version() const { return m_flags & 0x7F; }

    /// @returns true if the data is valid (has at least the header byte and
    /// enough bytes for signature if signed)
    bool isValid() const { return m_data.size() >= bytecodeOffset(); }

    /// @returns the offset where bytecode starts (1 if unsigned, 65 if signed)
    size_t bytecodeOffset() const
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

    /// @returns a span of the actual bytecode (without header)
    Span<const uint8_t> bytecode() const
    {
        size_t offset = bytecodeOffset();
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

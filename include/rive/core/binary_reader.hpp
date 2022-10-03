
#ifndef _RIVE_CORE_BINARY_READER_HPP_
#define _RIVE_CORE_BINARY_READER_HPP_

#include <string>
#include <vector>
#include "rive/span.hpp"
#include "rive/core/type_conversions.hpp"

namespace rive
{
class BinaryReader
{
private:
    Span<const uint8_t> m_Bytes;
    const uint8_t* m_Position;
    bool m_Overflowed;
    bool m_IntRangeError;

    void overflow();
    void intRangeError();

public:
    explicit BinaryReader(Span<const uint8_t>);
    bool didOverflow() const;
    bool didIntRangeError() const;
    bool hasError() const { return m_Overflowed || m_IntRangeError; }
    bool reachedEnd() const;

    size_t lengthInBytes() const;
    const uint8_t* position() const;

    std::string readString();
    Span<const uint8_t> readBytes();
    float readFloat32();
    uint8_t readByte();
    uint32_t readUint32();
    uint64_t readVarUint64(); // Reads a LEB128 encoded uint64_t

    // This will cast the uint read to the requested size, but if the
    // raw value was out-of-range, instead returns 0 and sets the IntRangeError.
    template <typename T> T readVarUintAs()
    {
        auto value = this->readVarUint64();
        if (!fitsIn<T>(value))
        {
            value = 0;
            this->intRangeError();
        }
        return static_cast<T>(value);
    }
};
} // namespace rive

#endif

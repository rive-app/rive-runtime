#ifndef _RIVE_CORE_BINARY_STREAM_HPP_
#define _RIVE_CORE_BINARY_STREAM_HPP_

#include <cstdint>
#include <cstddef>

namespace rive
{
// Used to write binary chunks to an underlying stream, makes no assumptions
// regarding storage/streaming it can flush the contents as it needs.
class BinaryStream
{
public:
    virtual void write(const uint8_t* bytes, std::size_t length) = 0;
    virtual void flush() = 0;
    virtual void clear() = 0;
};
} // namespace rive

#endif
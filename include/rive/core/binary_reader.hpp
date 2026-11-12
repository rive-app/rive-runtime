
#ifndef _RIVE_CORE_BINARY_READER_HPP_
#define _RIVE_CORE_BINARY_READER_HPP_

#include <string>
#include <vector>
#include "rive/span.hpp"
#include "rive/core/type_conversions.hpp"

namespace rive
{
class DataResolver;
class BinaryReader
{
private:
  Span<const uint8_t> m_Bytes;
  const uint8_t* m_Position;
  bool m_Overflowed;
  bool m_IntRangeError;
  DataResolver* m_dataResolver = nullptr;

  void overflow();
  void intRangeError();
  size_t readLength();

public:
  explicit BinaryReader(Span<const uint8_t>);
  bool didOverflow() const;
  bool didIntRangeError() const;
  bool hasError() const { return m_Overflowed || m_IntRangeError; }
  bool reachedEnd() const;

  size_t lengthInBytes() const;
  const uint8_t* position() const;
  /// One past the last byte the reader can read.
  const uint8_t* endPosition() const { return m_Bytes.end(); }

  std::string readString();
  std::string readString(size_t length);
  Span<const uint8_t> readBytes();
  Span<const uint8_t> readBytes(size_t length);
  float readFloat32();
#if defined(WITH_RIVE_TOOLS) || defined(WITH_RIVE_EDITOR)
  double readFloat64();
#endif
  uint8_t readByte();
  uint16_t readUint16();
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
  void reset();

  // How many LEB128 varuints [bytes] holds: each ends at a byte without its
  // high bit set.
  static size_t countVarUints(Span<const uint8_t> bytes)
  {
    size_t count = 0;
    for (uint8_t byte : bytes)
    {
      count += (byte & 0x80) == 0 ? 1 : 0;
    }
    return count;
  }

  /// The file's manifest, once it has decoded, so a property that indexes
  /// its names can resolve while the object is deserialized. Null before
  /// then, and in a file without one.
  DataResolver* dataResolver() const { return m_dataResolver; }
  void dataResolver(DataResolver* value) { m_dataResolver = value; }
};
} // namespace rive

#endif

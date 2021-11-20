
#ifndef _RIVE_CORE_BINARY_READER_HPP_
#define _RIVE_CORE_BINARY_READER_HPP_

#include <string>
#include <vector>

namespace rive
{
	class BinaryReader
	{
	private:
		uint8_t* m_Position;
		uint8_t* m_End;
		bool m_Overflowed;
		size_t m_Length;

		void overflow();

	public:
		BinaryReader(uint8_t* bytes, size_t length);
		bool didOverflow() const;
		bool reachedEnd() const;

		size_t lengthInBytes() const;

		std::string readString();
		std::vector<uint8_t> readBytes();
		double readFloat64();
		float readFloat32();
		uint8_t readByte();
		uint32_t readUint32();
		uint64_t readVarUint64(); // Reads a LEB128 encoded uint64_t
	};
} // namespace rive

#endif
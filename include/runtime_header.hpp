#ifndef _RIVE_RUNTIME_HEADER_HPP_
#define _RIVE_RUNTIME_HEADER_HPP_

#include "core/binary_reader.hpp"

namespace rive
{
	/// Rive file runtime header. The header is fonud at the beginning of every
	/// Rive runtime file, and begins with a specific 4-byte format: "RIVE".
	/// This is followed by the major and minor version of Rive used to create
	/// the file. Finally the owner and file ids are at the end of header; these
	/// unsigned integers may be zero.
	class RuntimeHeader
	{
	private:
		static constexpr char fingerprint[] = "RIVE";

		int m_MajorVersion;
		int m_MinorVersion;
		int m_OwnerId;
		int m_FileId;

	public:
		/// @returns the file's major version
		int majorVersion() const { return m_MajorVersion; }
		/// @returns the file's minor version
		int minorVersion() const { return m_MinorVersion; }
		/// @returns the id of the file's owner; may be zero
		int ownerId() const { return m_OwnerId; }
		/// @returns the file's id; may be zero
		int fileId() const { return m_FileId; }

		/// Reads the header from a binary buffer/
		/// @param reader the binary reader attached to the buffer
		/// @param header a pointer to the header where the data will be stored.
		/// @returns true if the header is successfully read
		static bool read(BinaryReader& reader, RuntimeHeader& header)
		{
			for (int i = 0; i < 4; i++)
			{
				auto b = reader.readByte();
				if (fingerprint[i] != b)
				{
					return false;
				}
			}

			header.m_MajorVersion = reader.readVarUint();
			if (reader.didOverflow())
			{
				return false;
			}
			header.m_MinorVersion = reader.readVarUint();
			if (reader.didOverflow())
			{
				return false;
			}

			header.m_OwnerId = reader.readVarUint();
			header.m_FileId = reader.readVarUint();

			return true;
		}
	};
} // namespace rive
#endif
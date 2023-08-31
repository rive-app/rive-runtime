#ifndef _RIVE_RUNTIME_HEADER_HPP_
#define _RIVE_RUNTIME_HEADER_HPP_

#include "rive/core/binary_reader.hpp"
#include <unordered_map>

namespace rive
{

/// Rive file runtime header. The header is fonud at the beginning of every
/// Rive runtime file, and begins with a specific 4-byte format: "RIVE".
/// This is followed by the major and minor version of Rive used to create
/// the file. Finally the owner and file ids are at the end of header; these
/// unsigned integers may be zero.
static constexpr char kRuntimeHeaderFingerprint[] = "RIVE";
class RuntimeHeader
{
private:
    int m_MajorVersion;
    int m_MinorVersion;
    int m_FileId;
    std::unordered_map<int, int> m_PropertyToFieldIndex;

public:
    /// @returns the file's major version
    int majorVersion() const { return m_MajorVersion; }
    /// @returns the file's minor version
    int minorVersion() const { return m_MinorVersion; }
    /// @returns the file's id; may be zero
    int fileId() const { return m_FileId; }

    int propertyFieldId(int propertyKey) const
    {
        auto itr = m_PropertyToFieldIndex.find(propertyKey);
        if (itr == m_PropertyToFieldIndex.end())
        {
            return -1;
        }

        return itr->second;
    }

    /// Reads the header from a binary buffer/
    /// @param reader the binary reader attached to the buffer
    /// @param header a pointer to the header where the data will be stored.
    /// @returns true if the header is successfully read
    static bool read(BinaryReader& reader, RuntimeHeader& header)
    {
        for (int i = 0; i < 4; i++)
        {
            auto b = reader.readByte();
            if (kRuntimeHeaderFingerprint[i] != b)
            {
                return false;
            }
        }

        header.m_MajorVersion = reader.readVarUintAs<int>();
        if (reader.didOverflow())
        {
            return false;
        }
        header.m_MinorVersion = reader.readVarUintAs<int>();
        if (reader.didOverflow())
        {
            return false;
        }

        header.m_FileId = reader.readVarUintAs<int>();

        if (reader.didOverflow())
        {
            return false;
        }

        std::vector<int> propertyKeys;
        for (int propertyKey = reader.readVarUintAs<int>(); propertyKey != 0;
             propertyKey = reader.readVarUintAs<int>())
        {
            propertyKeys.push_back(propertyKey);
            if (reader.didOverflow())
            {
                return false;
            }
        }

        int currentInt = 0;
        int currentBit = 8;
        for (auto propertyKey : propertyKeys)
        {
            if (currentBit == 8)
            {
                currentInt = reader.readUint32();
                currentBit = 0;
            }
            int fieldIndex = (currentInt >> currentBit) & 3;
            header.m_PropertyToFieldIndex[propertyKey] = fieldIndex;
            currentBit += 2;
            if (reader.didOverflow())
            {
                return false;
            }
        }

        return true;
    }
};
} // namespace rive
#endif

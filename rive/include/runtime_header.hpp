#ifndef _RIVE_RUNTIME_HEADER_HPP_
#define _RIVE_RUNTIME_HEADER_HPP_

#include "core/binary_reader.hpp"

namespace rive 
{
    class RuntimeHeader 
    {
    private:
        static constexpr char fingerprint[] = "RIVE";

        int m_MajorVersion;
        int m_MinorVersion;
        int m_OwnerId;
        int m_FileId;

    public:
        int majorVersion() const { return m_MajorVersion; }
        int minorVersion() const { return m_MinorVersion; }
        int ownerId() const { return m_OwnerId; }
        int fileId() const { return m_FileId; }

        static bool read(BinaryReader& reader, RuntimeHeader& header) 
        {
            for(int i = 0; i < 4; i++) 
            {
                auto b = reader.readByte();
                if(fingerprint[i] != b) 
                {
                    return false;
                }
            }
            
            header.m_MajorVersion = reader.readVarUint();
            if(reader.didOverflow()) 
            {
                return false;
            }
            header.m_MinorVersion = reader.readVarUint();
            if(reader.didOverflow()) 
            {
                return false;
            }

            header.m_OwnerId = reader.readVarUint();
            header.m_FileId = reader.readVarUint();

            return true;
        }
    };
}
#endif
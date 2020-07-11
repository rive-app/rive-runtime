#ifndef _RIVE_FILE_HPP_
#define _RIVE_FILE_HPP_

#include "runtime_header.hpp"
#include "core/binary_reader.hpp"

namespace rive 
{
    class File 
    {
        static const int majorVersion = 1;
        static const int minorVersion = 0;

    public:
        static File* import(BinaryReader& reader) 
        {
            RuntimeHeader header;
            if(!RuntimeHeader::read(reader, header))
            {
                printf("Bad header\n");
                // Bad header.
                return nullptr;
            }
            if(header.majorVersion() != majorVersion) 
            {
                printf("Unsupported version %u expected %u.\n", majorVersion, header.majorVersion());
                // Unsupported major version.
                return nullptr;
            }
            printf("IS IT BACK %llu == 23?", reader.readVarUint());
            return new File();
        }
    };
}
#endif
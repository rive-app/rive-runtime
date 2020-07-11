#ifndef _RIVE_CORE_HPP_
#define _RIVE_CORE_HPP_

#include "core/binary_reader.hpp"

namespace rive 
{
    class Core 
    {
    public:
        virtual int coreType() const = 0;
        virtual bool deserialize(int propertyKey, BinaryReader& reader) = 0;
    };
}
#endif
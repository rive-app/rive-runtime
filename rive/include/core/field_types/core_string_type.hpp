#ifndef _RIVE_CORE_STRING_TYPE_HPP_
#define _RIVE_CORE_STRING_TYPE_HPP_

#include <string>

namespace rive
{
    class BinaryReader;
    class CoreStringType
	{
    public:
        static std::string deserialize(BinaryReader& reader);
	};
} // namespace rive
#endif
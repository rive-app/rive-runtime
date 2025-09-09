#ifndef _RIVE_CORE_STRING_TYPE_HPP_
#define _RIVE_CORE_STRING_TYPE_HPP_

#include <string>

namespace rive
{
class BinaryReader;
class CoreStringType
{
public:
    static const int id = 1;
    static std::string deserialize(BinaryReader& reader);
#ifdef WITH_RIVE_TOOLS
    static std::string deserializeRev(BinaryReader& reader);
#endif
};
} // namespace rive
#endif
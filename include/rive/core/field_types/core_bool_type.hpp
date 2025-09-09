#ifndef _RIVE_CORE_BOOL_TYPE_HPP_
#define _RIVE_CORE_BOOL_TYPE_HPP_

namespace rive
{
class BinaryReader;
class CoreBoolType
{
public:
    static const int id = 4;
    static bool deserialize(BinaryReader& reader);
#ifdef WITH_RIVE_TOOLS
    static bool deserializeRev(BinaryReader& reader);
#endif
};
} // namespace rive
#endif
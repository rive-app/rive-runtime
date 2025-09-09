#ifndef _RIVE_CORE_DOUBLE_TYPE_HPP_
#define _RIVE_CORE_DOUBLE_TYPE_HPP_

namespace rive
{
class BinaryReader;
class CoreDoubleType
{
public:
    static const int id = 2;
    static float deserialize(BinaryReader& reader);
#ifdef WITH_RIVE_TOOLS
    static float deserializeRev(BinaryReader& reader);
#endif
};
} // namespace rive
#endif
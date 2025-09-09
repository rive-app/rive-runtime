#ifndef _RIVE_CORE_COLOR_TYPE_HPP_
#define _RIVE_CORE_COLOR_TYPE_HPP_

namespace rive
{
class BinaryReader;
class CoreColorType
{
public:
    static const int id = 3;
    static int deserialize(BinaryReader& reader);
#ifdef WITH_RIVE_TOOLS
    static int deserializeRev(BinaryReader& reader);
#endif
};
} // namespace rive
#endif
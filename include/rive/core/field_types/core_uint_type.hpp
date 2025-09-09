#ifndef _RIVE_CORE_UINT_TYPE_HPP_
#define _RIVE_CORE_UINT_TYPE_HPP_

namespace rive
{
class BinaryReader;
class CoreUintType
{
public:
    static const int id = 0;
    static unsigned int deserialize(BinaryReader& reader);
#ifdef WITH_RIVE_TOOLS
    static unsigned int deserializeRev(BinaryReader& reader);
#endif
};
} // namespace rive
#endif
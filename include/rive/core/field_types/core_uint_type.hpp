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
};
} // namespace rive
#endif
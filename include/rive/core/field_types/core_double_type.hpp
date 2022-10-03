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
};
} // namespace rive
#endif
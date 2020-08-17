#ifndef _RIVE_CORE_BOOL_TYPE_HPP_
#define _RIVE_CORE_BOOL_TYPE_HPP_

namespace rive
{
    class BinaryReader;
    class CoreBoolType
	{
    public:
        static bool deserialize(BinaryReader& reader);
	};
} // namespace rive
#endif
#ifndef _RIVE_CORE_INT_TYPE_HPP_
#define _RIVE_CORE_INT_TYPE_HPP_

namespace rive
{
    class BinaryReader;
    class CoreIntType
	{
    public:
        static int deserialize(BinaryReader& reader);
	};
} // namespace rive
#endif
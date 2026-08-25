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
#if defined(WITH_RIVE_TOOLS) || defined(WITH_RIVE_EDITOR)
    // Coop wire variant: the editor encodes colors as a varuint, while
    // the runtime `.riv` wire stays raw uint32. Used by generator-
    // emitted `applyChange`.
    static int deserializeRev(BinaryReader& reader);
#endif
};
} // namespace rive
#endif
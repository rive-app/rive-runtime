#ifndef _RIVE_NESTED_ARTBOARD_ORIGIN_HPP_
#define _RIVE_NESTED_ARTBOARD_ORIGIN_HPP_
#include "rive/generated/nested_artboard_origin_base.hpp"

namespace rive
{
/// Optional child of a NestedArtboard that overrides the origin of the mounted
/// artboard instance. Its mere presence signals "override origin" — nested
/// artboards without this child pay no per-instance cost.
class NestedArtboardOrigin : public NestedArtboardOriginBase
{
protected:
    void originXChanged() override;
    void originYChanged() override;

private:
    void reapply();
};

} // namespace rive

#endif

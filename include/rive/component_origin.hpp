#ifndef _RIVE_COMPONENT_ORIGIN_HPP_
#define _RIVE_COMPONENT_ORIGIN_HPP_
#include "rive/generated/component_origin_base.hpp"

namespace rive
{
/// Optional child that gives its parent an origin. Its mere presence signals
/// "this component has an origin" — a parent without this child carries no
/// per-instance origin state, which is the overwhelmingly common case.
///
/// On a NestedArtboard it overrides the origin of the mounted artboard; on a
/// LayoutComponent it is the pivot that rotation and scale compose about.
/// Artboards are the exception: they keep their origin inline, since nearly
/// every artboard sets one and the property predates this object.
class ComponentOrigin : public ComponentOriginBase
{
protected:
    void originXChanged() override;
    void originYChanged() override;

private:
    void reapply();
};

} // namespace rive

#endif

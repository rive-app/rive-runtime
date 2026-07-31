#include "rive/component_origin.hpp"
#include "rive/artboard.hpp"
#include "rive/layout_component.hpp"
#include "rive/nested_artboard.hpp"

using namespace rive;

void ComponentOrigin::reapply()
{
    auto* owner = parent();
    if (owner == nullptr)
    {
        return;
    }
    if (owner->is<NestedArtboard>())
    {
        auto nested = owner->as<NestedArtboard>();
        if (auto instance = nested->artboardInstance())
        {
            instance->originX(originX());
            instance->originY(originY());
        }
        return;
    }
    // A layout pivots its rotation/scale about this origin, so a change has to
    // recompose its world transform. Artboards read their own inline origin and
    // never consult this child.
    if (owner->is<LayoutComponent>() && !owner->is<Artboard>())
    {
        owner->as<LayoutComponent>()->markWorldTransformDirty();
    }
}

void ComponentOrigin::originXChanged() { reapply(); }
void ComponentOrigin::originYChanged() { reapply(); }

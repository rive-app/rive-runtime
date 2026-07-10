#include "rive/nested_artboard_origin.hpp"
#include "rive/nested_artboard.hpp"

using namespace rive;

void NestedArtboardOrigin::reapply()
{
    if (parent() != nullptr && parent()->is<NestedArtboard>())
    {
        auto nested = parent()->as<NestedArtboard>();
        if (auto instance = nested->artboardInstance())
        {
            instance->originX(originX());
            instance->originY(originY());
        }
    }
}

void NestedArtboardOrigin::originXChanged() { reapply(); }
void NestedArtboardOrigin::originYChanged() { reapply(); }

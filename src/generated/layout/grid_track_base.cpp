#include "rive/generated/layout/grid_track_base.hpp"
#include "rive/layout/grid_track.hpp"

using namespace rive;

Core* GridTrackBase::clone() const
{
    auto cloned = new GridTrack();
    cloned->copy(*this);
    return cloned;
}

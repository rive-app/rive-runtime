#include "rive/generated/layout/layout_participant_base.hpp"
#include "rive/layout/layout_participant.hpp"

using namespace rive;

Core* LayoutParticipantBase::clone() const
{
    auto cloned = new LayoutParticipant();
    cloned->copy(*this);
    return cloned;
}

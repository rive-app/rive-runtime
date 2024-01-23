#include "rive/generated/audio_event_base.hpp"
#include "rive/audio_event.hpp"

using namespace rive;

Core* AudioEventBase::clone() const
{
    auto cloned = new AudioEvent();
    cloned->copy(*this);
    return cloned;
}

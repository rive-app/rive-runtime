#include "rive/generated/animation/listener_types/listener_input_type_text_base.hpp"
#include "rive/animation/listener_types/listener_input_type_text.hpp"

using namespace rive;

Core* ListenerInputTypeTextBase::clone() const
{
    auto cloned = new ListenerInputTypeText();
    cloned->copy(*this);
    return cloned;
}

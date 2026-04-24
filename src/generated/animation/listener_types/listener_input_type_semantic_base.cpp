#include "rive/generated/animation/listener_types/listener_input_type_semantic_base.hpp"
#include "rive/animation/listener_types/listener_input_type_semantic.hpp"

using namespace rive;

Core* ListenerInputTypeSemanticBase::clone() const
{
    auto cloned = new ListenerInputTypeSemantic();
    cloned->copy(*this);
    return cloned;
}

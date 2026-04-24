#include "rive/generated/inputs/semantic_input_base.hpp"
#include "rive/inputs/semantic_input.hpp"

using namespace rive;

Core* SemanticInputBase::clone() const
{
    auto cloned = new SemanticInput();
    cloned->copy(*this);
    return cloned;
}

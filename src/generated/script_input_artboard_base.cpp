#include "rive/generated/script_input_artboard_base.hpp"
#include "rive/script_input_artboard.hpp"

using namespace rive;

Core* ScriptInputArtboardBase::clone() const
{
    auto cloned = new ScriptInputArtboard();
    cloned->copy(*this);
    return cloned;
}

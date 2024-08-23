#include "rive/generated/bones/skin_base.hpp"
#include "rive/bones/skin.hpp"

using namespace rive;

Core* SkinBase::clone() const
{
    auto cloned = new Skin();
    cloned->copy(*this);
    return cloned;
}

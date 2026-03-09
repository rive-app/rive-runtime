#include "rive/generated/focus_data_base.hpp"
#include "rive/focus_data.hpp"

using namespace rive;

Core* FocusDataBase::clone() const
{
    auto cloned = new FocusData();
    cloned->copy(*this);
    return cloned;
}

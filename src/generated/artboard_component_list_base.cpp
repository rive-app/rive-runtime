#include "rive/generated/artboard_component_list_base.hpp"
#include "rive/artboard_component_list.hpp"

using namespace rive;

Core* ArtboardComponentListBase::clone() const
{
    auto cloned = new ArtboardComponentList();
    cloned->copy(*this);
    return cloned;
}

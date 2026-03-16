#include "rive/animation/listener_types/listener_input_type_viewmodel.hpp"

using namespace rive;

void ListenerInputTypeViewModel::decodeViewModelPathIds(
    Span<const uint8_t> value)
{
    decodeDataBindPath(value);
}

void ListenerInputTypeViewModel::copyViewModelPathIds(
    const ListenerInputTypeViewModelBase& object)
{
    copyDataBindPath(object.as<ListenerInputTypeViewModel>()->dataBindPath());
}

std::vector<uint32_t> ListenerInputTypeViewModel::viewModelPathIdsBuffer() const
{
    return dataBindPath()->path();
}
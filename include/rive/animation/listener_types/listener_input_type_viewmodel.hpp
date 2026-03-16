#ifndef _RIVE_LISTENER_INPUT_TYPE_VIEW_MODEL_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_VIEW_MODEL_HPP_
#include "rive/generated/animation/listener_types/listener_input_type_viewmodel_base.hpp"
#include "rive/data_bind_path_referencer.hpp"
#include <stdio.h>
namespace rive
{
class ListenerInputTypeViewModel : public ListenerInputTypeViewModelBase,
                                   public DataBindPathReferencer
{
public:
    void decodeViewModelPathIds(Span<const uint8_t> value) override;
    void copyViewModelPathIds(
        const ListenerInputTypeViewModelBase& object) override;
    std::vector<uint32_t> viewModelPathIdsBuffer() const;
};
} // namespace rive

#endif
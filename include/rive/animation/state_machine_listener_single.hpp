#ifndef _RIVE_STATE_MACHINE_LISTENER_SINGLE_HPP_
#define _RIVE_STATE_MACHINE_LISTENER_SINGLE_HPP_
#include "rive/generated/animation/state_machine_listener_single_base.hpp"
#include "rive/data_bind_path_referencer.hpp"
namespace rive
{
class StateMachineListenerSingle : public StateMachineListenerSingleBase,
                                   public DataBindPathReferencer
{
public:
    StatusCode import(ImportStack& importStack) override;
    bool hasListener(ListenerType listenerType) const override
    {
        return listenerTypeValue() == (int)listenerType;
    }
    void decodeViewModelPathIds(Span<const uint8_t> value) override;
    void copyViewModelPathIds(
        const StateMachineListenerSingleBase& object) override;
    std::vector<uint32_t> viewModelPathIdsBuffer() const;
};
} // namespace rive

#endif
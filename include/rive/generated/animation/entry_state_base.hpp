#ifndef _RIVE_ENTRY_STATE_BASE_HPP_
#define _RIVE_ENTRY_STATE_BASE_HPP_
#include "rive/animation/layer_state.hpp"
namespace rive
{
class EntryStateBase : public LayerState
{
protected:
    typedef LayerState Super;

public:
    static const uint16_t typeKey = 63;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif
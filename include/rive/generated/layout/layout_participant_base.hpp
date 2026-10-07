#ifndef _RIVE_LAYOUT_PARTICIPANT_BASE_HPP_
#define _RIVE_LAYOUT_PARTICIPANT_BASE_HPP_
#include "rive/layout/layout_node_style.hpp"
namespace rive
{
class LayoutParticipantBase : public LayoutNodeStyle
{
protected:
    typedef LayoutNodeStyle Super;

public:
    static const uint16_t typeKey = 1066;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif
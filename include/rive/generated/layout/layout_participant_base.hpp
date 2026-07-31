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

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case LayoutParticipantBase::typeKey:
            case LayoutNodeStyleBase::typeKey:
            case LayoutSizingStyleBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif
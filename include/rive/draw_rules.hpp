#ifndef _RIVE_DRAW_RULES_HPP_
#define _RIVE_DRAW_RULES_HPP_
#include "rive/generated/draw_rules_base.hpp"
#include <stdio.h>
namespace rive
{
class DrawTarget;
class DrawRules : public DrawRulesBase
{
private:
    DrawTarget* m_ActiveTarget = nullptr;

public:
    DrawTarget* activeTarget() const { return m_ActiveTarget; }

    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

protected:
    void drawTargetIdChanged() override;
};
} // namespace rive

#endif
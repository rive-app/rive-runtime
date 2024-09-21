#ifndef _RIVE_DASH_HPP_
#define _RIVE_DASH_HPP_
#include "rive/generated/shapes/paint/dash_base.hpp"
#include <stdio.h>
namespace rive
{
class Dash : public DashBase
{
public:
    Dash();
    Dash(float value, bool percentage);

    float normalizedLength(float length) const;

    StatusCode onAddedClean(CoreContext* context) override;

    void lengthChanged() override;
    void lengthIsPercentageChanged() override;
};
} // namespace rive

#endif
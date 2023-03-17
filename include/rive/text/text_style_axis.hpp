#ifndef _RIVE_TEXT_STYLE_AXIS_HPP_
#define _RIVE_TEXT_STYLE_AXIS_HPP_
#include "rive/generated/text/text_style_axis_base.hpp"
#include <stdio.h>
namespace rive
{
class TextStyleAxis : public TextStyleAxisBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
    void tagChanged() override;
    void axisValueChanged() override;
};
} // namespace rive

#endif
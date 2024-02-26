#ifndef _RIVE_TEXT_VARIATION_MODIFIER_HPP_
#define _RIVE_TEXT_VARIATION_MODIFIER_HPP_
#include "rive/generated/text/text_variation_modifier_base.hpp"

namespace rive
{
class TextVariationModifier : public TextVariationModifierBase
{
public:
    float modify(Font* font,
                 std::unordered_map<uint32_t, float>& variations,
                 float fontSize,
                 float strength) const override;
    void axisValueChanged() override;
};
} // namespace rive

#endif
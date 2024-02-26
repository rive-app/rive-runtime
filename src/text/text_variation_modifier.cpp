#include "rive/text/text_variation_modifier.hpp"
#include "rive/text/text_modifier_group.hpp"
#include "rive/text_engine.hpp"

using namespace rive;

float TextVariationModifier::modify(Font* font,
                                    std::unordered_map<uint32_t, float>& variations,
                                    float fontSize,
                                    float strength) const
{
    auto itr = variations.find(axisTag());

    float fromValue = itr != variations.end() ? itr->second : font->getAxisValue(axisTag());
    variations[axisTag()] = fromValue * (1 - strength) + axisValue() * strength;
    return fontSize;
}

void TextVariationModifier::axisValueChanged()
{
    parent()->as<TextModifierGroup>()->shapeModifierChanged();
}
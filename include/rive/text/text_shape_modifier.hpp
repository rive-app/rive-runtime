#ifndef _RIVE_TEXT_SHAPE_MODIFIER_HPP_
#define _RIVE_TEXT_SHAPE_MODIFIER_HPP_
#include "rive/generated/text/text_shape_modifier_base.hpp"
#include <unordered_map>

namespace rive
{
class Font;
class TextShapeModifier : public TextShapeModifierBase
{
public:
    virtual float modify(Font* font,
                         std::unordered_map<uint32_t, float>& variations,
                         float fontSize,
                         float strength) const = 0;
};
} // namespace rive

#endif
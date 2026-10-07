#ifndef _RIVE_TEXT_INPUT_DRAWABLE_BASE_HPP_
#define _RIVE_TEXT_INPUT_DRAWABLE_BASE_HPP_
#include "rive/drawable.hpp"
namespace rive
{
class TextInputDrawableBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 570;

    uint16_t coreType() const override { return typeKey; }
};
} // namespace rive

#endif
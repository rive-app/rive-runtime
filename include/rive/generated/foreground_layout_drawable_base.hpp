#ifndef _RIVE_FOREGROUND_LAYOUT_DRAWABLE_BASE_HPP_
#define _RIVE_FOREGROUND_LAYOUT_DRAWABLE_BASE_HPP_
#include "rive/drawable.hpp"
namespace rive
{
class ForegroundLayoutDrawableBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 513;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif
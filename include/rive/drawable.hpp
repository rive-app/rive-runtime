#ifndef _RIVE_DRAWABLE_HPP_
#define _RIVE_DRAWABLE_HPP_
#include "rive/generated/drawable_base.hpp"
#include "rive/hit_info.hpp"
#include "rive/renderer.hpp"
#include "rive/clip_result.hpp"
#include <vector>

namespace rive
{
class ClippingShape;
class Artboard;
class DrawRules;

class Drawable : public DrawableBase
{
    friend class Artboard;

private:
    std::vector<ClippingShape*> m_ClippingShapes;

    /// Used exclusively by the artboard;
    DrawRules* flattenedDrawRules = nullptr;
    Drawable* prev = nullptr;
    Drawable* next = nullptr;

public:
    BlendMode blendMode() const { return (BlendMode)blendModeValue(); }
    ClipResult clip(Renderer* renderer) const;
    virtual void draw(Renderer* renderer) = 0;
    virtual Core* hitTest(HitInfo*, const Mat2D&) = 0;
    void addClippingShape(ClippingShape* shape);
    inline const std::vector<ClippingShape*>& clippingShapes() const { return m_ClippingShapes; }

    inline bool isHidden() const
    {
        // For now we have a single drawable flag, when we have more we can
        // make an actual enum for this.
        return (drawableFlags() & 0x1) == 0x1 || hasDirt(ComponentDirt::Collapsed);
    }
};
} // namespace rive

#endif

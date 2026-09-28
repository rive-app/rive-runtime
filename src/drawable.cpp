#include "rive/drawable.hpp"
#include "rive/artboard.hpp"
#include "rive/custom_property.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/path_composer.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/clip_result.hpp"
#include "rive/shapes/paint/paint_outset.hpp"

using namespace rive;

bool Drawable::hasCustomProperties()
{
#ifdef WITH_RIVE_EDITOR
    // Children change under the editor, so nothing cached stays true.
    for (auto child : children())
    {
        if (CustomProperty::tagging(child) != nullptr)
        {
            return true;
        }
    }
    return false;
#else
    return runtimeFlag(RuntimeFlags::hasCustomProperties);
#endif
}

CustomProperty* Drawable::customProperty(uint32_t nameId)
{
    for (auto child : children())
    {
        // Only tagging ones: the key of a missing name is what every other
        // property holds, a scripted drawable's inputs among them.
        auto property = CustomProperty::tagging(child);
        if (property != nullptr && property->nameId() == nameId)
        {
            return property;
        }
    }
    return nullptr;
}

StatusCode Drawable::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    auto blendMode = static_cast<rive::BlendMode>(blendModeValue());
    switch (blendMode)
    {
        case rive::BlendMode::srcOver:
        case rive::BlendMode::additive:
        case rive::BlendMode::screen:
        case rive::BlendMode::overlay:
        case rive::BlendMode::darken:
        case rive::BlendMode::lighten:
        case rive::BlendMode::colorDodge:
        case rive::BlendMode::colorBurn:
        case rive::BlendMode::hardLight:
        case rive::BlendMode::softLight:
        case rive::BlendMode::difference:
        case rive::BlendMode::exclusion:
        case rive::BlendMode::multiply:
        case rive::BlendMode::hue:
        case rive::BlendMode::saturation:
        case rive::BlendMode::color:
        case rive::BlendMode::luminosity:
            return StatusCode::Ok;
    }
    return StatusCode::InvalidObject;
}

void Drawable::addClippingShape(ClippingShape* shape)
{
    m_ClippingShapes.push_back(shape);
}

void Drawable::addLayerMask(LayerMask* mask) { m_layerMasks.push_back(mask); }

void Drawable::addSourceOfLayerMask(LayerMask* mask)
{
    m_sourceOfLayerMasks.push_back(mask);
}

bool Drawable::willDraw() { return !isHidden(); }

bool Drawable::isChildOfLayout(LayoutComponent* layout)
{
    for (ContainerComponent* parent = this; parent != nullptr;
         parent = parent->parent())
    {
        if (parent->is<LayoutComponent>() &&
            parent->as<LayoutComponent>() == layout)
        {
            return true;
        }
    }
    return false;
}

bool Drawable::hitTestPoint(const Vec2D& position,
                            bool skipOnUnclipped,
                            bool isPrimaryHit)
{
    if (isHidden())
    {
        return false;
    }
    auto hComponent = hittableComponent();
    if (hComponent != this && hComponent != nullptr)
    {
        return hComponent->hitTestPoint(position,
                                        skipOnUnclipped,
                                        isPrimaryHit);
    }
    return Component::hitTestPoint(position, skipOnUnclipped, isPrimaryHit);
}

Drawable* DrawableProxy::hittableComponent()
{
    return static_cast<LayoutComponent*>(proxyDrawing());
}

bool DrawableProxy::isTargetOpaque()
{
    return hittableComponent()->isTargetOpaque();
}

BoundsFidelity Drawable::paintedBoundsFromLocal(
    const AABB& local,
    const Mat2D& world,
    const ShapePaintContainer* paints,
    AABB* out)
{
    if (local.isEmptyOrNaN())
    {
        return BoundsFidelity::none;
    }
    AABB bounds = world.mapBoundingBox(local);
    if (bounds.isEmptyOrNaN())
    {
        // A degenerate transform -- scale 0, or a collapsed layout fold --
        // flattens the box to nothing. That is a real answer, not a failure:
        // nothing is painted there.
        *out = AABB();
        return BoundsFidelity::exact;
    }
    bool trustworthy = true;
    if (paints != nullptr)
    {
        const PaintReach reach = shapePaintsWorldReach(paints, world);
        if (reach.worldOutset > 0.0f)
        {
            bounds = bounds.outset(reach.worldOutset, reach.worldOutset);
        }
        trustworthy = reach.trustworthy;
    }
    *out = bounds;
    return trustworthy ? BoundsFidelity::exact : BoundsFidelity::approximate;
}

BoundsFidelity DrawableProxy::paintedWorldBounds(AABB* out)
{
    Drawable* target = hittableComponent();
    // hittableComponent() returns `this` on the base Drawable, and a proxy that
    // stands in for nothing has nothing to measure. Guarding also keeps this
    // from recursing forever if that ever changes.
    if (target == nullptr || target == this)
    {
        return BoundsFidelity::none;
    }
    return target->paintedWorldBounds(out);
}

const std::vector<LayerMask*>& DrawableProxy::layerMasks() const
{
    // const_cast because hittableComponent() is non-const; it is a pure
    // downcast of proxyDrawing() and mutates nothing.
    return const_cast<DrawableProxy*>(this)->hittableComponent()->layerMasks();
}

const std::vector<LayerMask*>& DrawableProxy::sourceOfLayerMasks() const
{
    return const_cast<DrawableProxy*>(this)
        ->hittableComponent()
        ->sourceOfLayerMasks();
}

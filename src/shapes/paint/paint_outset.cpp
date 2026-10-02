#include "rive/shapes/paint/paint_outset.hpp"

#include "rive/math/math_types.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/transform_space.hpp"

#include <algorithm>
#include <cmath>

namespace rive
{

float paintBoundsOutset(const std::optional<StrokeParams>& stroke,
                        float feather)
{
    float outset = 0.0f;
    if (stroke.has_value())
    {
        // A centered stroke straddles the path, reaching half its thickness.
        // The renderer draws an outside stroke as a centered stroke twice as
        // thick, clipped to the outside, so it reaches the full thickness (and
        // its miter twice as far). An inside one is clipped to the path and
        // reaches nothing past it; half is a safe over-estimate.
        outset = stroke->thickness *
                 (stroke->position == StrokePosition::outside ? 1.0f : .5f);
        if (stroke->join == StrokeJoin::miter)
        {
            // Miter joins may be longer than the stroke radius.
            outset *= kMiterLimit;
        }
        else if (stroke->cap == StrokeCap::square)
        {
            // The diagonal of a square cap is longer than the stroke radius.
            outset *= math::SQRT2;
        }
    }
    if (feather != 0.0f)
    {
        outset += featherRadiusFromFeather(feather);
    }
    return outset;
}

ShapePaintOutset shapePaintOutset(const ShapePaint* paint)
{
    ShapePaintOutset out;
    // shouldDraw(), not isVisible(): a paint whose mutator is fully transparent
    // reaches nowhere, and Stroke::isVisible() additionally folds in
    // thickness > 0 so a zero-width stroke costs nothing here.
    if (paint == nullptr || !paint->shouldDraw())
    {
        return out;
    }

    std::optional<StrokeParams> stroke;
    if (paint->is<Stroke>())
    {
        const Stroke* s = paint->as<Stroke>();
        // Mirrors Stroke::applyTo, which is what the renderer actually sees,
        // position included: an outside stroke reaches twice as far.
        StrokeParams params;
        params.thickness = s->thickness();
        params.join = static_cast<StrokeJoin>(s->join());
        params.cap = static_cast<StrokeCap>(s->cap());
        params.position = s->strokePosition();
        stroke = params;
        // Stroke::pickPath: the local path when the transform affects the
        // stroke, the already-world path otherwise.
        out.pathIsLocal = s->transformAffectsStroke();
    }

    float feather = 0.0f;
    if (const Feather* f = paint->feather())
    {
        // inner() only ever moves the blur inward, so treating every feather as
        // outward keeps this an over-estimate -- the safe direction, since the
        // cost of over-estimating is a few wasted texels and the cost of
        // under-estimating is a visible crop.
        feather = f->strength();
        out.featherOffset = {f->offsetX(), f->offsetY()};
        out.featherOffsetInWorld =
            static_cast<TransformSpace>(f->spaceValue()) ==
            TransformSpace::world;
    }

    out.outset = paintBoundsOutset(stroke, feather);
    out.trustworthy = !paint->hasEffects();
    return out;
}

PaintReach shapePaintsWorldReach(const ShapePaintContainer* container,
                                 const Mat2D& world)
{
    PaintReach reach;
    if (container == nullptr)
    {
        return reach;
    }
    const float worldScale = world.findMaxScale();
    for (const ShapePaint* paint : container->shapePaints())
    {
        const ShapePaintOutset po = shapePaintOutset(paint);
        reach.trustworthy = reach.trustworthy && po.trustworthy;

        // The outset is expressed in the space of the path the paint actually
        // draws, which pickPath decides. A local-path outset has to be pushed
        // through the shape's scale to mean anything against a world box; a
        // world-path one is already in world units.
        float distance = po.pathIsLocal ? po.outset * worldScale : po.outset;

        // A feather's offset is not symmetric, but padding per axis would have
        // to account for rotation redistributing an x offset into y. Its length
        // covers every orientation.
        Vec2D offset = po.featherOffset;
        if (!po.featherOffsetInWorld)
        {
            // A displacement, not a position: linear part only.
            offset = Vec2D::transformDir(offset, world);
        }
        distance += offset.length();

        if (std::isfinite(distance))
        {
            reach.worldOutset = std::max(reach.worldOutset, distance);
        }
    }
    return reach;
}

} // namespace rive

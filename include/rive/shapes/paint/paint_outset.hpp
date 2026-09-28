#ifndef _RIVE_PAINT_OUTSET_HPP_
#define _RIVE_PAINT_OUTSET_HPP_

#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/renderer.hpp"

#include <optional>

namespace rive
{
class ShapePaint;
class ShapePaintContainer;

// How far past its own geometry a paint actually reaches.
//
// A shape's bounds -- Shape::worldBounds(), RawPath::bounds() -- describe
// geometry only. A stroke straddles the path, a miter join overshoots the
// corner, a square cap overshoots the end, and a feather bleeds a blur radius
// in every direction. Anything sizing a surface from a shape's bounds has to
// add all of that or it crops, and a crop at a raster edge is a hard line
// across the middle of someone's artwork.
//
// This is the one definition of that arithmetic. The renderer's
// RiveRenderPath::calculateBoundsOutset and gpu::featherRadiusFromFeather both
// forward here, so the raster sizing and the draw culling can never disagree
// about how much room a paint needs.
namespace paint_outset_detail
{
// Mirrors of two shader constants, which core cannot include: constants.glsl
// lives under renderer/src/shaders/ and is not on any public include path.
// rive_render_path.cpp is the one translation unit that sees both it and this
// header, and it static_asserts that they still agree -- a silent divergence
// here would crop feathers at a raster edge, which is exactly the kind of bug
// that survives review.
constexpr float kMiterLimit = 4.0f; // RIVE_MITER_LIMIT
constexpr float kGaussianIntegralStdDevs =
    3.0f; // GAUSSIAN_INTEGRAL_TEXTURE_STDDEVS
} // namespace paint_outset_detail

using paint_outset_detail::kGaussianIntegralStdDevs;
using paint_outset_detail::kMiterLimit;

// Blur magnitudes in design tools are customarily the width of two standard
// deviations -- the length of the range -1stddev .. +1stddev -- so an authored
// feather of N reaches 1.5N in every direction.
constexpr float featherRadiusFromFeather(float feather)
{
    return feather * (kGaussianIntegralStdDevs / 2);
}

// The symmetric distance a stroke and/or a feather adds to a path's bounding
// box, in whatever space the path itself is expressed in.
//
// Note what is deliberately absent: the extra pixel of antialias bleed that
// RiveRenderPath::calculatePixelBounds adds on top. That is a pixel-space
// quantity and this function does not know the raster scale; callers sizing a
// surface cover it with a guard band instead.
float paintBoundsOutset(const std::optional<StrokeParams>& stroke,
                        float feather);

// What one ShapePaint adds to its shape's bounds.
struct ShapePaintOutset
{
    // Symmetric, in the space `pathIsLocal` names.
    float outset = 0.0f;
    // A Feather can be offset, which is not symmetric, so it is kept apart from
    // `outset` rather than folded in as a worst case on all four sides.
    Vec2D featherOffset = {0.0f, 0.0f};
    // Whether featherOffset applies in world space (concatenated before the
    // shape transform) or in the shape's local space (after it).
    bool featherOffsetInWorld = false;
    // Which space `outset` is in. A Fill always paints the local path; a Stroke
    // paints the local path only when the transform affects it, and the world
    // path otherwise. Getting this backwards scales the outset by the shape's
    // scale twice, or not at all.
    bool pathIsLocal = true;
    // False when this paint carries a StrokeEffect that can place geometry
    // outside the raw path it was built from, so bounds measured off that raw
    // path understate what gets painted. Trim and dash only ever subset the
    // source path, but nothing distinguishes them here yet.
    bool trustworthy = true;
};

// A paint that would not draw contributes nothing and is trustworthy about it.
ShapePaintOutset shapePaintOutset(const ShapePaint* paint);

// How far past its geometry a whole container's paints reach, in world units.
struct PaintReach
{
    // The furthest-reaching visible paint. The max rather than the sum: every
    // paint reaches outward from the same geometry, so they overlap.
    float worldOutset = 0.0f;
    // False when any paint can place geometry outside the raw path a bounding
    // box was measured from, so that box understates what is painted.
    bool trustworthy = true;
};

// `world` is the container's world transform, needed to get a local-space
// outset (a Fill's, or a Stroke's when the transform affects it) into the same
// units as a world-space bounding box.
PaintReach shapePaintsWorldReach(const ShapePaintContainer* container,
                                 const Mat2D& world);

} // namespace rive

#endif

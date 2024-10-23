#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/layout/axis.hpp"
#include "rive/layout/n_sliced_node.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/n_slicer_helpers.hpp"

using namespace rive;

std::vector<float> NSlicerHelpers::uvStops(const std::vector<Axis*>& axes,
                                           float size)
{
    std::vector<float> uvs = {0.0};
    for (const Axis* axis : axes)
    {
        if (axis == nullptr)
        {
            continue;
        }
        if (axis->normalized())
        {
            uvs.emplace_back(math::clamp(axis->offset(), 0, 1));
        }
        else
        {
            uvs.emplace_back(math::clamp(axis->offset() / size, 0, 1));
        }
    }
    uvs.emplace_back(1.0);
    std::sort(uvs.begin(), uvs.end());
    return uvs;
}

std::vector<float> NSlicerHelpers::pxStops(const std::vector<Axis*>& axes,
                                           float size)
{
    std::vector<float> result = {0.0};
    for (const Axis* axis : axes)
    {
        if (axis == nullptr)
        {
            continue;
        }
        if (axis->normalized())
        {
            result.emplace_back(math::clamp(axis->offset(), 0, 1) * size);
        }
        else
        {
            result.emplace_back(math::clamp(axis->offset(), 0, size));
        }
    }
    result.emplace_back(size);
    std::sort(result.begin(), result.end());
    return result;
}

ScaleInfo NSlicerHelpers::analyzeUVStops(const std::vector<float>& uvs,
                                         float size,
                                         float scale)
{
    assert(size >= 0 && scale >= 0);
    // Find the percentage of the dimension that should be fixed / not scaled.
    float fixedPct = 0.0;
    int numEmptyPatch = 0;
    for (int i = 0; i < (int)uvs.size() - 1; i++)
    {
        float range = uvs[i + 1] - uvs[i];
        if (isFixedSegment(i))
        {
            fixedPct += range;
        }
        else
        {
            numEmptyPatch += (range == 0 ? 1 : 0);
        }
    }

    // Find the scale that the scalable segments are bearing.
    // If the unscaled image is 300px, how to divide it up between scaled and
    // unscaled patches.
    float fixedSize = fixedPct * size;
    float scalableSize = size - fixedSize;

    // Considering the image's scale, how much should the scalable patches scale
    // by. And if all scalable patches were empty, how much whitespace to leave
    // in those patches.
    bool useScale = scalableSize != 0;
    float scaleFactor =
        useScale ? (size * scale - fixedSize) / scalableSize : 0;

    float fallbackSize = 0;
    if (!useScale && numEmptyPatch != 0)
    {
        fallbackSize = (size - fixedSize / scale) / numEmptyPatch;
    }

    return {useScale, scaleFactor, fallbackSize};
}

float NSlicerHelpers::mapValue(const std::vector<float>& stops,
                               const ScaleInfo& scaleInfo,
                               float value)
{
    if (value < (stops.front() - 0.001) || value > (stops.back() + 0.001))
    {
        return value;
    }

    float result = 0;
    for (int i = 0; i < (int)stops.size() - 1; i++)
    {
        bool found = value <= stops[i + 1];
        float span = found ? value - stops[i] : stops[i + 1] - stops[i];
        if (isFixedSegment(i))
        {
            result += span;
        }
        else
        {
            result += scaleInfo.useScale ? scaleInfo.scaleFactor * span : 0;
        }
        if (found)
        {
            break;
        }
    }

    return result;
}

bool NSlicerHelpers::deformLocalRenderPathWithNSlicer(
    const NSlicedNode& nslicedNode,
    RawPath& localPath,
    const Mat2D& world,
    const Mat2D& inverseWorld)
{
    RawPath tempWorld = localPath.transform(world);
    if (!deformWorldRenderPathWithNSlicer(nslicedNode, tempWorld))
    {
        // Do not change localPath if sth went wrong.
        return false;
    }

    // No need to shrink the commands buffers
    localPath.rewind();
    localPath.addPath(tempWorld, &inverseWorld);
    return true;
}

bool NSlicerHelpers::deformWorldRenderPathWithNSlicer(
    const NSlicedNode& nslicedNode,
    RawPath& worldPath)
{
    if (nslicedNode.initialHeight() <= 0 || nslicedNode.initialWidth() <= 0)
    {
        return false;
    }

    const Mat2D& world = nslicedNode.worldTransform();
    Mat2D inverseWorld;
    if (!world.invert(&inverseWorld))
    {
        return false;
    }

    Vec2D size = Vec2D(nslicedNode.initialWidth(), nslicedNode.initialHeight());
    Vec2D scale = nslicedNode.scaleForNSlicer();

    // When doing calcualtions, we assume scale is always non-negative to keep
    // everything in NSlicer space.
    scale.x = std::abs(scale.x);
    scale.y = std::abs(scale.y);

    std::vector<float> xPxStops = pxStops(nslicedNode.xs(), size.x);
    std::vector<float> yPxStops = pxStops(nslicedNode.ys(), size.y);

    std::vector<float> xUVStops = uvStops(nslicedNode.xs(), size.x);
    std::vector<float> yUVStops = uvStops(nslicedNode.ys(), size.y);

    ScaleInfo xScaleInfo = analyzeUVStops(xUVStops, size.x, scale.x);
    ScaleInfo yScaleInfo = analyzeUVStops(yUVStops, size.x, scale.y);

    auto nslicerWorldPoint = [&world,
                              &inverseWorld,
                              &nslicedNode,
                              &scale,
                              &xPxStops,
                              &xScaleInfo,
                              &yPxStops,
                              &yScaleInfo](Vec2D& worldP) {
        // 1. Map from world space to the effective NSlicer's space
        Vec2D localP = inverseWorld * worldP;

        // 2. N-Slice it in the NSlicer's space
        Vec2D slicedP = Vec2D(
            scale.x == 0 ? 0.0
                         : mapValue(xPxStops, xScaleInfo, localP.x) / scale.x,
            scale.y == 0 ? 0.0
                         : mapValue(yPxStops, yScaleInfo, localP.y) / scale.y);

        // 3. Map it to the bounds space
        Vec2D boundsP = nslicedNode.boundsTransform() * slicedP;
        boundsP.x = math::clamp(boundsP.x,
                                std::min(0.0f, nslicedNode.width()),
                                std::max(0.0f, nslicedNode.width()));
        boundsP.y = math::clamp(boundsP.y,
                                std::min(0.0f, nslicedNode.height()),
                                std::max(0.0f, nslicedNode.height()));

        // 4. Finally to world space
        worldP = world * boundsP;
    };

    // Map every point, then copy it back to worldPath
    for (Vec2D& point : worldPath.points())
    {
        nslicerWorldPoint(point);
    }
    return true;
}

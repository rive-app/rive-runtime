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
    if (value < (stops.front() - 0.01) || value > (stops.back() + 0.01))
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

void NSlicerHelpers::deformLocalRenderPathWithNSlicer(
    const NSlicedNode& nslicedNode,
    RawPath& localPath,
    const Mat2D& world,
    const Mat2D& inverseWorld)
{
    RawPath tempWorld = localPath.transform(world);
    deformWorldRenderPathWithNSlicer(nslicedNode, tempWorld);
    localPath.rewind();
    localPath.addPath(tempWorld, &inverseWorld);
}

void NSlicerHelpers::deformWorldRenderPathWithNSlicer(
    const NSlicedNode& nslicedNode,
    RawPath& worldPath)
{
    for (Vec2D& point : worldPath.points())
    {
        nslicedNode.mapWorldPoint(point);
    }
}

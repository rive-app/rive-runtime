#ifndef _RIVE_N_SLICER_HELPERS_HPP_
#define _RIVE_N_SLICER_HELPERS_HPP_
#include <vector>

namespace rive
{
class Axis;
class NSlicedNode;
class RawPath;
class Mat2D;

struct ScaleInfo
{
    bool useScale;
    float scaleFactor;
    float fallbackSize;
};

struct NSlicerHelpers
{
    static std::vector<float> uvStops(const std::vector<Axis*>& axes,
                                      float size);
    static std::vector<float> pxStops(const std::vector<Axis*>& axes,
                                      float size);

    static ScaleInfo analyzeUVStops(const std::vector<float>& uvs,
                                    float size,
                                    float scale);

    static float mapValue(const std::vector<float>& stops,
                          const ScaleInfo& scaleInfo,
                          float size,
                          float value);

    static bool isFixedSegment(int i) { return i % 2 == 0; };
    static void deformLocalRenderPathWithNSlicer(const NSlicedNode& nslicedNode,
                                                 RawPath& localPath,
                                                 const Mat2D& world,
                                                 const Mat2D& inverseWorld);
    static void deformWorldRenderPathWithNSlicer(const NSlicedNode& nslicedNode,
                                                 RawPath& worldPath);
};
} // namespace rive
#endif

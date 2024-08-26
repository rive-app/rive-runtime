#ifndef _RIVE_SLICE_MESH_HPP_
#define _RIVE_SLICE_MESH_HPP_
#include "rive/shapes/mesh_drawable.hpp"
#include "rive/renderer.hpp"

namespace rive
{
class NSlicer;
enum class AxisType : int;

struct SliceMeshVertex
{
    int id = -1;
    Vec2D uv;
    Vec2D vertex;
};

struct Corner
{
    int x;
    int y;
};

// A temporary mesh that backs up an NSlicer.
class SliceMesh : public MeshDrawable
{
private:
    NSlicer* m_nslicer;

    // More readable representation of the render buffers.
    std::vector<Vec2D> m_uvs;
    std::vector<Vec2D> m_vertices;
    std::vector<uint16_t> m_indices;

    std::vector<float> uvStops(AxisType forAxis);
    std::vector<float> vertexStops(const std::vector<float>& normalizedStops, AxisType forAxis);

    uint16_t tileRepeat(std::vector<SliceMeshVertex>& vertices,
                        std::vector<uint16_t>& indices,
                        const std::vector<SliceMeshVertex>& box,
                        uint16_t start);

    // Update the member (non-render) buffers.
    void calc();

    // Copy the member (non-render) buffers into the render buffers.
    void updateBuffers();

    static const uint16_t triangulation[];
    static const Corner patchCorners[];
    bool isFixedSegment(int i) const;

protected:
public:
    SliceMesh(NSlicer* nslicer);
    void draw(Renderer* renderer,
              const RenderImage* image,
              BlendMode blendMode,
              float opacity) override;
    void onAssetLoaded(RenderImage* renderImage) override;

    void update(); // not Component::update() as SliceMesh is not in core.
};
} // namespace rive

#endif

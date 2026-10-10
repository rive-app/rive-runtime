#ifndef _RIVE_NESTED_ARTBOARD_MESH_HOST_HPP_
#define _RIVE_NESTED_ARTBOARD_MESH_HOST_HPP_
#include "rive/shapes/mesh_host.hpp"
#ifdef RIVE_CANVAS
#include "rive/offscreen_raster.hpp"
#endif

namespace rive
{
class Artboard;
class ArtboardInstance;
class NestedArtboard;
class Renderer;

// Mesh state of a Node-mode NestedArtboard, which owns it.
class NestedArtboardMeshHost final : public MeshHost
{
public:
    explicit NestedArtboardMeshHost(NestedArtboard* owner) : m_owner(owner) {}

    void setMesh(MeshDrawable* mesh) override;
    MeshDrawable* hostedMesh() const override { return m_mesh; }
    float meshHostWidth() const override;
    float meshHostHeight() const override;
    Vec2D meshHostOrigin() const override;
    float meshHostScaleX() const override;
    float meshHostScaleY() const override;
    Mat2D meshHostWorldTransform() const override;
    bool meshHostReady() const override;
    Mat2D meshHostUVTransform() const override;
#ifdef WITH_RIVE_EDITOR
    void setMeshForEditor(MeshDrawable* mesh) override { setMesh(mesh); }
    void clearMeshIfForEditor(MeshDrawable* expected) override
    {
        if (m_mesh == expected)
        {
            setMesh(nullptr);
        }
    }
#endif

    // The artboard's Bitmap Cache resolution, else 1.
    float meshRasterResolution() const;
    void resetRaster();
#ifdef RIVE_CANVAS
    // False when no raster could be made; the caller draws vectors.
    bool draw(Renderer* renderer);
    bool compositeDropsClip() const;
#endif
#ifdef TESTING
    bool rasterHeld() const
    {
#ifdef RIVE_CANVAS
        return m_raster.canvas != nullptr;
#else
        return false;
#endif
    }
#endif

private:
    ArtboardInstance* instance() const;
    const Artboard* sizeSource() const;
#ifdef RIVE_CANVAS
    bool underClip() const;
#endif

    NestedArtboard* m_owner;
    MeshDrawable* m_mesh = nullptr;
#ifdef RIVE_CANVAS
    offscreen::CachedRaster m_raster;
    // A data-bound swap or resize rebuilds the raster and UVs.
    const Artboard* m_rasterSource = nullptr;
    Mat2D m_uvTransform;
    bool m_uvValid = false;
#endif
};
} // namespace rive

#endif

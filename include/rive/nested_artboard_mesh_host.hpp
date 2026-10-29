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

  // Whether pointers follow the mesh: it drew the last frame, rather than
  // the artboard falling back to its undeformed vectors.
  bool pointersFollowMesh() const;
  // Maps a host-space point into the artboard (the space
  // NestedArtboard::worldToLocal returns) through the mesh. `offMesh` is
  // true past the mesh's edge, where the mapping extends the nearest
  // triangle. False when pointers don't follow the mesh, or it has no
  // triangles to map through. `toLocal`, when given, is set to the affine
  // map through the triangle used.
  bool localThroughMesh(
      Vec2D world,
      Vec2D* local,
      bool* offMesh,
      Mat2D* toLocal = nullptr);
  // Where a point in the artboard's content space is drawn, in host space.
  bool worldThroughMesh(Vec2D content, Vec2D* world) const;
#ifdef RIVE_CANVAS
  // False when no raster could be made; the caller draws vectors.
  bool draw(Renderer* renderer);
  bool compositeDropsClip() const;
  // Whether the last draw went through the mesh, so pointers follow it.
  void drewThroughMesh(bool value) { m_drewThroughMesh = value; }
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
  // Until a draw says otherwise, pointers follow the mesh.
  bool m_drewThroughMesh = true;
  // The last pointer localThroughMesh mapped, where it was and where it
  // landed in content space (see worldThroughMesh).
  Vec2D m_pointerWorld;
  Vec2D m_pointerContent;
  bool m_hasPointer = false;
#endif
};
} // namespace rive

#endif

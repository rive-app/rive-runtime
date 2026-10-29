#ifndef _RIVE_MESH_DRAWABLE_HPP_
#define _RIVE_MESH_DRAWABLE_HPP_
#include "rive/refcnt.hpp"
#include "rive/renderer.hpp"
#include "rive/math/mat2d.hpp"
#include <vector>

namespace rive
{
class RenderImage;
enum class MeshType : uint8_t
{
  vertex = 0,
  nslice = 1,
};
class MeshDrawable
{
protected:
  class IndexBuffer : public std::vector<uint16_t>, public RefCnt<IndexBuffer>
  {};
  rcp<RenderBuffer> m_IndexRenderBuffer;
  rcp<RenderBuffer> m_VertexRenderBuffer;
  rcp<RenderBuffer> m_UVRenderBuffer;

public:
  virtual MeshType type() { return MeshType::vertex; }
  virtual ~MeshDrawable() = default;
  virtual void onAssetLoaded(RenderImage* image) = 0;
  // For textures made at draw time (an artboard bitmap). Rebuilds UVs and
  // indices against `uvTransform`.
  virtual void uvTransformChanged(const Mat2D& uvTransform) = 0;
  virtual void draw(
      Renderer* renderer,
      const RenderImage* image,
      ImageSampler,
      BlendMode blendMode,
      float opacity,
      float additiveness) = 0;
  // Whether `position` (in the same space as the host's world transform)
  // lands on a triangle draw() would fill.
  virtual bool hitTest(Vec2D position) = 0;
  // The 0..1 UV drawn at `position`, in the same space as hitTest. `inside`
  // is false when no triangle covers it; the UV then extends the nearest
  // one. False when there are no triangles to map through. `toUV`, when
  // given, is set to the affine map from position to UV through the
  // triangle used.
  virtual bool uvAt(Vec2D position, Vec2D* uv, bool* inside, Mat2D* toUV) = 0;
  // Where `uv` is drawn, the inverse of uvAt.
  virtual bool positionAtUV(Vec2D uv, Vec2D* position) = 0;
};
} // namespace rive

#endif

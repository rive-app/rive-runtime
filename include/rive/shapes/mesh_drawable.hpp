#ifndef _RIVE_MESH_DRAWABLE_HPP_
#define _RIVE_MESH_DRAWABLE_HPP_
#include "rive/refcnt.hpp"
#include "rive/renderer.hpp"
#include <vector>

namespace rive
{
class RenderImage;
class MeshDrawable
{
protected:
    class IndexBuffer : public std::vector<uint16_t>, public RefCnt<IndexBuffer>
    {};
    rcp<RenderBuffer> m_IndexRenderBuffer;
    rcp<RenderBuffer> m_VertexRenderBuffer;
    rcp<RenderBuffer> m_UVRenderBuffer;

public:
    virtual ~MeshDrawable() = default;
    virtual void onAssetLoaded(RenderImage* image) = 0;
    virtual void draw(Renderer* renderer,
                      const RenderImage* image,
                      BlendMode blendMode,
                      float opacity) = 0;
};
} // namespace rive

#endif

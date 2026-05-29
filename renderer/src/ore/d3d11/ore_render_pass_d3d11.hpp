#pragma once
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "ore_pipeline_d3d11.hpp"
#include "rive/refcnt.hpp"
#include <d3d11.h>
#include <cstdint>

namespace rive::ore
{
class ContextD3D11;

class RenderPassD3D11 : public RenderPass
{
public:
    void setPipeline(Pipeline* pipeline) override;
    void setVertexBuffer(uint32_t slot,
                         Buffer* buffer,
                         uint32_t offset = 0) override;
    void setIndexBuffer(Buffer* buffer,
                        IndexFormat format,
                        uint32_t offset = 0) override;
    void setBindGroup(uint32_t groupIndex,
                      BindGroup* bg,
                      const uint32_t* dynamicOffsets = nullptr,
                      uint32_t dynamicOffsetCount = 0) override;
    void setViewport(float x,
                     float y,
                     float width,
                     float height,
                     float minDepth = 0.0f,
                     float maxDepth = 1.0f) override;
    void setScissorRect(uint32_t x,
                        uint32_t y,
                        uint32_t width,
                        uint32_t height) override;
    void setStencilReference(uint32_t ref) override;
    void setBlendColor(float r, float g, float b, float a) override;
    void draw(uint32_t vertexCount,
              uint32_t instanceCount = 1,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0) override;
    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount = 1,
                     uint32_t firstIndex = 0,
                     int32_t baseVertex = 0,
                     uint32_t firstInstance = 0) override;
    void finish() override;

    void validate() const override;

    RenderPassD3D11() = default;
    RenderPassD3D11(Context* context) : RenderPass(context) {}
    ~RenderPassD3D11() override;
    RenderPassD3D11(RenderPassD3D11&& other) noexcept;
    RenderPassD3D11& operator=(RenderPassD3D11&&) noexcept;

    struct D3D11ResolveEntry
    {
        ID3D11Resource* resolve = nullptr;
        UINT resolveSubresource = 0;
        ID3D11Resource* msaa = nullptr;
        UINT msaaSubresource = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    };

private:
    friend class ContextD3D11;

    ID3D11DeviceContext* m_d3d11Context = nullptr; // weak ref
    rcp<PipelineD3D11> m_currentPipeline;
    D3D11_PRIMITIVE_TOPOLOGY m_d3d11Topology =
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    DXGI_FORMAT m_d3d11IndexFormat = DXGI_FORMAT_R16_UINT;
    uint32_t m_d3d11IndexOffset = 0;
    uint32_t m_d3d11StencilRef = 0;
    float m_d3d11BlendFactor[4] = {};
    uint32_t m_d3d11ResolveCount = 0;
    D3D11ResolveEntry m_d3d11Resolves[4];
};
} // namespace rive::ore

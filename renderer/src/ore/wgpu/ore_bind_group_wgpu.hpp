#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include <webgpu/webgpu_cpp.h>
#include <string>
#include <vector>

namespace rive::ore
{
class ContextWGPU;
class BufferWGPU;

class BindGroupWGPU : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupWGPU)
{
public:
    BindGroupWGPU() = default;
    ~BindGroupWGPU() override =
        default; // wgpu::BindGroup RAII destructor releases

    // Bind group for the current backings of this group's UBOs. A UBO that
    // orphaned onto a fresh backing gets a freshly built bind group, since
    // WebGPU bind groups are immutable once created.
    const wgpu::BindGroup& resolveBindGroup();

    // Stamp every UBO's current backing as bound this frame.
    void markUBOsBound();

private:
    friend class ContextWGPU;
    friend class RenderPassWGPU;

    // Replayable recipe captured at makeBindGroup so a bind group can be
    // rebuilt against a UBO's new backing. Buffers stay alive via
    // m_retainedBuffers; views/samplers via m_retainedViews/m_retainedSamplers.
    struct UBOEntry
    {
        BufferWGPU* buffer;
        uint32_t binding;
        uint64_t offset;
        uint64_t size;
    };
    struct TexEntry
    {
        uint32_t binding;
        wgpu::TextureView view;
    };
    struct SampEntry
    {
        uint32_t binding;
        wgpu::Sampler sampler;
    };
    std::vector<UBOEntry> m_uboEntries;
    std::vector<TexEntry> m_texEntries;
    std::vector<SampEntry> m_sampEntries;
    wgpu::BindGroupLayout m_wgpuBGL;
    std::string m_label; // propagated to rebuilt bind groups for GPU captures

    // One bind group per distinct combination of UBO backings, bounded by
    // frames-in-flight. Keyed on the current WGPUBuffer of each UBO.
    struct CachedGroup
    {
        std::vector<WGPUBuffer> key;
        wgpu::BindGroup bindGroup;
    };
    std::vector<CachedGroup> m_cache;

    // Returned (null) when CreateBindGroup fails so the failure is not cached
    // and a later call can retry.
    wgpu::BindGroup m_nullBindGroup;
};
} // namespace rive::ore

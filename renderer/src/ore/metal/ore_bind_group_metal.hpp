#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#import <Metal/Metal.h>
#include <vector>

namespace rive::ore
{
class ContextMetal;
class RenderPassMetal;

class BindGroupMetal : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupMetal)
{
public:
    struct MTLBufferBinding
    {
        id<MTLBuffer> buffer = nil;
        uint32_t offset = 0;
        uint32_t binding = 0; // WGSL @binding, for sort
        bool hasDynamicOffset = false;
        uint16_t vsSlot = BindingMap::kAbsent;
        uint16_t fsSlot = BindingMap::kAbsent;
    };
    struct MTLTextureBinding
    {
        id<MTLTexture> texture = nil;
        uint16_t vsSlot = BindingMap::kAbsent;
        uint16_t fsSlot = BindingMap::kAbsent;
    };
    struct MTLSamplerBinding
    {
        id<MTLSamplerState> sampler = nil;
        uint16_t vsSlot = BindingMap::kAbsent;
        uint16_t fsSlot = BindingMap::kAbsent;
    };

    BindGroupMetal() = default;
    ~BindGroupMetal() override = default; // ARC releases Metal objects

private:
    friend class ContextMetal;
    friend class RenderPassMetal;
    std::vector<MTLBufferBinding> m_mtlBuffers;
    std::vector<MTLTextureBinding> m_mtlTextures;
    std::vector<MTLSamplerBinding> m_mtlSamplers;
};
} // namespace rive::ore

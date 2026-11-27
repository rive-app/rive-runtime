#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#import <Metal/Metal.h>
#include <vector>

namespace rive::ore
{
class ContextMetal;
class RenderPassMetal;
class ComputePassMetal;
class BufferMetal;

using SizedBindings = std::vector<BindingMap::SizedBinding>;

// What `module`'s shaders size their arrays from, none for a stage left out.
inline SizedBindings sizedBindingsOf(const ShaderModule* module)
{
  return module != nullptr ? module->m_bindingMap.sizedBindings()
                           : SizedBindings();
}

class BindGroupMetal : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupMetal)
{
public:
  struct MTLBufferBinding
  {
    // Source buffer, not a raw handle, so the live backing resolves at
    // encode time. Kept alive by m_retainedBuffers.
    BufferMetal* src = nullptr;
    uint32_t offset = 0;
    // Bound bytes, kept for storage buffers alone.
    uint32_t size = 0;
    uint32_t binding = 0; // WGSL @binding, for sort
    bool hasDynamicOffset = false;
  };
  struct MTLTextureBinding
  {
    id<MTLTexture> texture = nil;
    uint32_t binding = 0;
  };
  struct MTLSamplerBinding
  {
    id<MTLSamplerState> sampler = nil;
    uint32_t binding = 0;
  };

  BindGroupMetal() = default;
  ~BindGroupMetal() override = default; // ARC releases Metal objects

  // The bound bytes of each of `sized` among `groups`, 0 where none is bound,
  // as a shader reads them at kMetalReservedBufferSlot. False when it reads
  // none.
  static bool boundSizes(
      const SizedBindings& sized,
      const rcp<BindGroup>* groups,
      std::vector<uint32_t>& out);

private:
  friend class ContextMetal;
  friend class RenderPassMetal;
  friend class ComputePassMetal;
  std::vector<MTLBufferBinding> m_mtlBuffers;
  std::vector<MTLBufferBinding> m_mtlStorageBuffers;
  std::vector<MTLTextureBinding> m_mtlTextures;
  std::vector<MTLSamplerBinding> m_mtlSamplers;
};
} // namespace rive::ore

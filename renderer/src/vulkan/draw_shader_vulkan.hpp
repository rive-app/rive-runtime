/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/gpu.hpp"
#include <vulkan/vulkan.h>

namespace rive::gpu
{
namespace spirv
{
// Draw setup shaders.
extern rive::Span<const uint32_t> color_ramp_vert;
extern rive::Span<const uint32_t> color_ramp_frag;
extern rive::Span<const uint32_t> tessellate_vert;
extern rive::Span<const uint32_t> tessellate_frag;
extern rive::Span<const uint32_t> render_atlas_vert;
extern rive::Span<const uint32_t> render_atlas_fill_frag;
extern rive::Span<const uint32_t> render_atlas_stroke_frag;

// InterlockMode::rasterOrdering shaders.
extern rive::Span<const uint32_t> draw_path_vert;
extern rive::Span<const uint32_t> draw_path_frag;
extern rive::Span<const uint32_t> draw_interior_triangles_vert;
extern rive::Span<const uint32_t> draw_interior_triangles_frag;
extern rive::Span<const uint32_t> draw_atlas_blit_vert;
extern rive::Span<const uint32_t> draw_atlas_blit_frag;
extern rive::Span<const uint32_t> draw_image_mesh_vert;
extern rive::Span<const uint32_t> draw_image_mesh_frag;

// InterlockMode::atomics shaders.
extern rive::Span<const uint32_t> atomic_draw_path_vert;
extern rive::Span<const uint32_t> atomic_draw_path_frag;
extern rive::Span<const uint32_t> atomic_draw_path_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_draw_interior_triangles_vert;
extern rive::Span<const uint32_t> atomic_draw_interior_triangles_frag;
extern rive::Span<const uint32_t>
    atomic_draw_interior_triangles_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_draw_atlas_blit_vert;
extern rive::Span<const uint32_t> atomic_draw_atlas_blit_frag;
extern rive::Span<const uint32_t> atomic_draw_atlas_blit_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_draw_image_rect_vert;
extern rive::Span<const uint32_t> atomic_draw_image_rect_frag;
extern rive::Span<const uint32_t> atomic_draw_image_rect_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_draw_image_mesh_vert;
extern rive::Span<const uint32_t> atomic_draw_image_mesh_frag;
extern rive::Span<const uint32_t> atomic_draw_image_mesh_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_resolve_vert;
extern rive::Span<const uint32_t> atomic_resolve_frag;
extern rive::Span<const uint32_t> atomic_resolve_fixedcolor_frag;
extern rive::Span<const uint32_t> atomic_resolve_coalesced_vert;
extern rive::Span<const uint32_t> atomic_resolve_coalesced_frag;

// InterlockMode::clockwiseAtomic shaders.
extern rive::Span<const uint32_t> draw_clockwise_path_vert;
extern rive::Span<const uint32_t> draw_clockwise_path_frag;
extern rive::Span<const uint32_t> draw_clockwise_interior_triangles_vert;
extern rive::Span<const uint32_t> draw_clockwise_interior_triangles_frag;
extern rive::Span<const uint32_t> draw_clockwise_atlas_blit_vert;
extern rive::Span<const uint32_t> draw_clockwise_atlas_blit_frag;
extern rive::Span<const uint32_t> draw_clockwise_image_mesh_vert;
extern rive::Span<const uint32_t> draw_clockwise_image_mesh_frag;

// InterlockMode::msaa shaders.
extern rive::Span<const uint32_t> draw_msaa_path_vert;
extern rive::Span<const uint32_t> draw_msaa_path_frag;
extern rive::Span<const uint32_t> draw_msaa_path_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_msaa_stencil_vert;
extern rive::Span<const uint32_t> draw_msaa_stencil_frag;
extern rive::Span<const uint32_t> draw_msaa_stencil_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_msaa_atlas_blit_vert;
extern rive::Span<const uint32_t> draw_msaa_atlas_blit_frag;
extern rive::Span<const uint32_t> draw_msaa_atlas_blit_fixedcolor_frag;
extern rive::Span<const uint32_t> draw_msaa_image_mesh_vert;
extern rive::Span<const uint32_t> draw_msaa_image_mesh_frag;
extern rive::Span<const uint32_t> draw_msaa_image_mesh_fixedcolor_frag;

// Reload global SPIRV buffers from runtime data.
void hotload_shaders(rive::Span<const uint32_t> spirvData);
}; // namespace spirv

class VulkanContext;

// Wraps vertex and fragment shader modules for a specific combination of
// DrawType, InterlockMode, and ShaderFeatures.
class DrawShaderVulkan
{
public:
    DrawShaderVulkan(VulkanContext*,
                     gpu::DrawType,
                     gpu::InterlockMode,
                     gpu::ShaderFeatures,
                     gpu::ShaderMiscFlags);

    ~DrawShaderVulkan();

    VkShaderModule vertexModule() const { return m_vertexModule; }
    VkShaderModule fragmentModule() const { return m_fragmentModule; }

private:
    const rcp<VulkanContext> m_vk;
    VkShaderModule m_vertexModule = VK_NULL_HANDLE;
    VkShaderModule m_fragmentModule = VK_NULL_HANDLE;
};
} // namespace rive::gpu

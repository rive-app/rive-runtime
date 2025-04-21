/*
 * Copyright 2023 Rive
 */

#include "draw_shader_vulkan.hpp"

#include "rive/renderer/vulkan/vkutil.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include "shaders/constants.glsl"

namespace rive::gpu
{
namespace spirv_embedded
{
// Draw setup shaders.
#include "generated/shaders/spirv/color_ramp.vert.h"
#include "generated/shaders/spirv/color_ramp.frag.h"
#include "generated/shaders/spirv/tessellate.vert.h"
#include "generated/shaders/spirv/tessellate.frag.h"
#include "generated/shaders/spirv/render_atlas.vert.h"
#include "generated/shaders/spirv/render_atlas_fill.frag.h"
#include "generated/shaders/spirv/render_atlas_stroke.frag.h"

// InterlockMode::rasterOrdering shaders.
#include "generated/shaders/spirv/draw_path.vert.h"
#include "generated/shaders/spirv/draw_path.frag.h"
#include "generated/shaders/spirv/draw_interior_triangles.vert.h"
#include "generated/shaders/spirv/draw_interior_triangles.frag.h"
#include "generated/shaders/spirv/draw_atlas_blit.vert.h"
#include "generated/shaders/spirv/draw_atlas_blit.frag.h"
#include "generated/shaders/spirv/draw_image_mesh.vert.h"
#include "generated/shaders/spirv/draw_image_mesh.frag.h"

// InterlockMode::atomics shaders.
#include "generated/shaders/spirv/atomic_draw_path.vert.h"
#include "generated/shaders/spirv/atomic_draw_path.frag.h"
#include "generated/shaders/spirv/atomic_draw_path.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_draw_interior_triangles.vert.h"
#include "generated/shaders/spirv/atomic_draw_interior_triangles.frag.h"
#include "generated/shaders/spirv/atomic_draw_interior_triangles.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_draw_atlas_blit.vert.h"
#include "generated/shaders/spirv/atomic_draw_atlas_blit.frag.h"
#include "generated/shaders/spirv/atomic_draw_atlas_blit.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_draw_image_rect.vert.h"
#include "generated/shaders/spirv/atomic_draw_image_rect.frag.h"
#include "generated/shaders/spirv/atomic_draw_image_rect.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_draw_image_mesh.vert.h"
#include "generated/shaders/spirv/atomic_draw_image_mesh.frag.h"
#include "generated/shaders/spirv/atomic_draw_image_mesh.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_resolve.vert.h"
#include "generated/shaders/spirv/atomic_resolve.frag.h"
#include "generated/shaders/spirv/atomic_resolve.fixedcolor_frag.h"
#include "generated/shaders/spirv/atomic_resolve_coalesced.vert.h"
#include "generated/shaders/spirv/atomic_resolve_coalesced.frag.h"

// InterlockMode::clockwiseAtomic shaders.
#include "generated/shaders/spirv/draw_clockwise_path.vert.h"
#include "generated/shaders/spirv/draw_clockwise_path.frag.h"
#include "generated/shaders/spirv/draw_clockwise_interior_triangles.vert.h"
#include "generated/shaders/spirv/draw_clockwise_interior_triangles.frag.h"
#include "generated/shaders/spirv/draw_clockwise_atlas_blit.vert.h"
#include "generated/shaders/spirv/draw_clockwise_atlas_blit.frag.h"
#include "generated/shaders/spirv/draw_clockwise_image_mesh.vert.h"
#include "generated/shaders/spirv/draw_clockwise_image_mesh.frag.h"
}; // namespace spirv_embedded

namespace spirv
{
rive::Span<const uint32_t> color_ramp_vert =
    rive::make_span(spirv_embedded::color_ramp_vert);
rive::Span<const uint32_t> color_ramp_frag =
    rive::make_span(spirv_embedded::color_ramp_frag);
rive::Span<const uint32_t> tessellate_vert =
    rive::make_span(spirv_embedded::tessellate_vert);
rive::Span<const uint32_t> tessellate_frag =
    rive::make_span(spirv_embedded::tessellate_frag);
rive::Span<const uint32_t> render_atlas_vert =
    rive::make_span(spirv_embedded::render_atlas_vert);
rive::Span<const uint32_t> render_atlas_fill_frag =
    rive::make_span(spirv_embedded::render_atlas_fill_frag);
rive::Span<const uint32_t> render_atlas_stroke_frag =
    rive::make_span(spirv_embedded::render_atlas_stroke_frag);
rive::Span<const uint32_t> draw_path_vert =
    rive::make_span(spirv_embedded::draw_path_vert);
rive::Span<const uint32_t> draw_path_frag =
    rive::make_span(spirv_embedded::draw_path_frag);
rive::Span<const uint32_t> draw_interior_triangles_vert =
    rive::make_span(spirv_embedded::draw_interior_triangles_vert);
rive::Span<const uint32_t> draw_interior_triangles_frag =
    rive::make_span(spirv_embedded::draw_interior_triangles_frag);
rive::Span<const uint32_t> draw_atlas_blit_vert =
    rive::make_span(spirv_embedded::draw_atlas_blit_vert);
rive::Span<const uint32_t> draw_atlas_blit_frag =
    rive::make_span(spirv_embedded::draw_atlas_blit_frag);
rive::Span<const uint32_t> draw_image_mesh_vert =
    rive::make_span(spirv_embedded::draw_image_mesh_vert);
rive::Span<const uint32_t> draw_image_mesh_frag =
    rive::make_span(spirv_embedded::draw_image_mesh_frag);
rive::Span<const uint32_t> atomic_draw_path_vert =
    rive::make_span(spirv_embedded::atomic_draw_path_vert);
rive::Span<const uint32_t> atomic_draw_path_frag =
    rive::make_span(spirv_embedded::atomic_draw_path_frag);
rive::Span<const uint32_t> atomic_draw_path_fixedcolor_frag = rive::make_span(
    spirv_embedded::atomic_draw_path_fixedcolor_frag,
    std::size(spirv_embedded::atomic_draw_path_fixedcolor_frag));
rive::Span<const uint32_t> atomic_draw_interior_triangles_vert =
    rive::make_span(spirv_embedded::atomic_draw_interior_triangles_vert);
rive::Span<const uint32_t> atomic_draw_interior_triangles_frag =
    rive::make_span(spirv_embedded::atomic_draw_interior_triangles_frag);
rive::Span<const uint32_t> atomic_draw_interior_triangles_fixedcolor_frag =
    rive::make_span(
        spirv_embedded::atomic_draw_interior_triangles_fixedcolor_frag);
rive::Span<const uint32_t> atomic_draw_atlas_blit_vert =
    rive::make_span(spirv_embedded::atomic_draw_atlas_blit_vert);
rive::Span<const uint32_t> atomic_draw_atlas_blit_frag =
    rive::make_span(spirv_embedded::atomic_draw_atlas_blit_frag);
rive::Span<const uint32_t> atomic_draw_atlas_blit_fixedcolor_frag =
    rive::make_span(spirv_embedded::atomic_draw_atlas_blit_fixedcolor_frag);
rive::Span<const uint32_t> atomic_draw_image_rect_vert =
    rive::make_span(spirv_embedded::atomic_draw_image_rect_vert);
rive::Span<const uint32_t> atomic_draw_image_rect_frag =
    rive::make_span(spirv_embedded::atomic_draw_image_rect_frag);
rive::Span<const uint32_t> atomic_draw_image_rect_fixedcolor_frag =
    rive::make_span(spirv_embedded::atomic_draw_image_rect_fixedcolor_frag);
rive::Span<const uint32_t> atomic_draw_image_mesh_vert =
    rive::make_span(spirv_embedded::atomic_draw_image_mesh_vert);
rive::Span<const uint32_t> atomic_draw_image_mesh_frag =
    rive::make_span(spirv_embedded::atomic_draw_image_mesh_frag);
rive::Span<const uint32_t> atomic_draw_image_mesh_fixedcolor_frag =
    rive::make_span(spirv_embedded::atomic_draw_image_mesh_fixedcolor_frag);
rive::Span<const uint32_t> atomic_resolve_vert =
    rive::make_span(spirv_embedded::atomic_resolve_vert);
rive::Span<const uint32_t> atomic_resolve_frag =
    rive::make_span(spirv_embedded::atomic_resolve_frag);
rive::Span<const uint32_t> atomic_resolve_fixedcolor_frag =
    rive::make_span(spirv_embedded::atomic_resolve_fixedcolor_frag,
                    std::size(spirv_embedded::atomic_resolve_fixedcolor_frag));
rive::Span<const uint32_t> atomic_resolve_coalesced_vert =
    rive::make_span(spirv_embedded::atomic_resolve_coalesced_vert);
rive::Span<const uint32_t> atomic_resolve_coalesced_frag =
    rive::make_span(spirv_embedded::atomic_resolve_coalesced_frag);
rive::Span<const uint32_t> draw_clockwise_path_vert =
    rive::make_span(spirv_embedded::draw_clockwise_path_vert);
rive::Span<const uint32_t> draw_clockwise_path_frag =
    rive::make_span(spirv_embedded::draw_clockwise_path_frag);
rive::Span<const uint32_t> draw_clockwise_interior_triangles_vert =
    rive::make_span(spirv_embedded::draw_clockwise_interior_triangles_vert);
rive::Span<const uint32_t> draw_clockwise_interior_triangles_frag =
    rive::make_span(spirv_embedded::draw_clockwise_interior_triangles_frag);
rive::Span<const uint32_t> draw_clockwise_atlas_blit_vert =
    rive::make_span(spirv_embedded::draw_clockwise_atlas_blit_vert);
rive::Span<const uint32_t> draw_clockwise_atlas_blit_frag =
    rive::make_span(spirv_embedded::draw_clockwise_atlas_blit_frag);
rive::Span<const uint32_t> draw_clockwise_image_mesh_vert =
    rive::make_span(spirv_embedded::draw_clockwise_image_mesh_vert);
rive::Span<const uint32_t> draw_clockwise_image_mesh_frag =
    rive::make_span(spirv_embedded::draw_clockwise_image_mesh_frag);

void hotload_shaders(rive::Span<const uint32_t> spirvData)
{
    size_t spirvIndex = 0;
    auto readNextBytecodeSpan = [spirvData,
                                 &spirvIndex]() -> rive::Span<const uint32_t> {
        size_t insnCount = spirvData[spirvIndex++];
        const uint32_t* insnData = spirvData.data() + spirvIndex;
        spirvIndex += insnCount;
        return rive::make_span(insnData, insnCount);
    };

    spirv::color_ramp_vert = readNextBytecodeSpan();
    spirv::color_ramp_frag = readNextBytecodeSpan();
    spirv::tessellate_vert = readNextBytecodeSpan();
    spirv::tessellate_frag = readNextBytecodeSpan();
    spirv::render_atlas_vert = readNextBytecodeSpan();
    spirv::render_atlas_fill_frag = readNextBytecodeSpan();
    spirv::render_atlas_stroke_frag = readNextBytecodeSpan();
    spirv::draw_path_vert = readNextBytecodeSpan();
    spirv::draw_path_frag = readNextBytecodeSpan();
    spirv::draw_interior_triangles_vert = readNextBytecodeSpan();
    spirv::draw_interior_triangles_frag = readNextBytecodeSpan();
    spirv::draw_atlas_blit_vert = readNextBytecodeSpan();
    spirv::draw_atlas_blit_frag = readNextBytecodeSpan();
    spirv::draw_image_mesh_vert = readNextBytecodeSpan();
    spirv::draw_image_mesh_frag = readNextBytecodeSpan();
    spirv::atomic_draw_path_vert = readNextBytecodeSpan();
    spirv::atomic_draw_path_frag = readNextBytecodeSpan();
    spirv::atomic_draw_path_fixedcolor_frag = readNextBytecodeSpan();
    spirv::atomic_draw_interior_triangles_vert = readNextBytecodeSpan();
    spirv::atomic_draw_interior_triangles_frag = readNextBytecodeSpan();
    spirv::atomic_draw_interior_triangles_fixedcolor_frag =
        readNextBytecodeSpan();
    spirv::atomic_draw_atlas_blit_vert = readNextBytecodeSpan();
    spirv::atomic_draw_atlas_blit_frag = readNextBytecodeSpan();
    spirv::atomic_draw_atlas_blit_fixedcolor_frag = readNextBytecodeSpan();
    spirv::atomic_draw_image_rect_vert = readNextBytecodeSpan();
    spirv::atomic_draw_image_rect_frag = readNextBytecodeSpan();
    spirv::atomic_draw_image_rect_fixedcolor_frag = readNextBytecodeSpan();
    spirv::atomic_draw_image_mesh_vert = readNextBytecodeSpan();
    spirv::atomic_draw_image_mesh_frag = readNextBytecodeSpan();
    spirv::atomic_draw_image_mesh_fixedcolor_frag = readNextBytecodeSpan();
    spirv::atomic_resolve_vert = readNextBytecodeSpan();
    spirv::atomic_resolve_frag = readNextBytecodeSpan();
    spirv::atomic_resolve_fixedcolor_frag = readNextBytecodeSpan();
    spirv::atomic_resolve_coalesced_vert = readNextBytecodeSpan();
    spirv::atomic_resolve_coalesced_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_path_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_path_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_interior_triangles_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_interior_triangles_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_atlas_blit_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_atlas_blit_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_image_mesh_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_image_mesh_frag = readNextBytecodeSpan();
}
}; // namespace spirv

DrawShaderVulkan::DrawShaderVulkan(VulkanContext* vk,
                                   gpu::DrawType drawType,
                                   gpu::InterlockMode interlockMode,
                                   gpu::ShaderFeatures shaderFeatures,
                                   gpu::ShaderMiscFlags shaderMiscFlags) :
    m_vk(ref_rcp(vk))
{
    VkShaderModuleCreateInfo vsInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkShaderModuleCreateInfo fsInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

    if (interlockMode == gpu::InterlockMode::rasterOrdering)
    {
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
                vkutil::set_shader_code(vsInfo, spirv::draw_path_vert);
                vkutil::set_shader_code(fsInfo, spirv::draw_path_frag);
                break;

            case DrawType::interiorTriangulation:
                vkutil::set_shader_code(vsInfo,
                                        spirv::draw_interior_triangles_vert);
                vkutil::set_shader_code(fsInfo,
                                        spirv::draw_interior_triangles_frag);
                break;

            case DrawType::atlasBlit:
                vkutil::set_shader_code(vsInfo, spirv::draw_atlas_blit_vert);
                vkutil::set_shader_code(fsInfo, spirv::draw_atlas_blit_frag);
                break;

            case DrawType::imageMesh:
                vkutil::set_shader_code(vsInfo, spirv::draw_image_mesh_vert);
                vkutil::set_shader_code(fsInfo, spirv::draw_image_mesh_frag);
                break;

            case DrawType::imageRect:
            case DrawType::atomicResolve:
            case DrawType::atomicInitialize:
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStencilClipReset:
                RIVE_UNREACHABLE();
        }
    }
    else if (interlockMode == gpu::InterlockMode::atomics)
    {
        assert(interlockMode == gpu::InterlockMode::atomics);
        bool fixedFunctionColorOutput =
            shaderMiscFlags & gpu::ShaderMiscFlags::fixedFunctionColorOutput;
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
                vkutil::set_shader_code(vsInfo, spirv::atomic_draw_path_vert);
                vkutil::set_shader_code_if_then_else(
                    fsInfo,
                    fixedFunctionColorOutput,
                    spirv::atomic_draw_path_fixedcolor_frag,
                    spirv::atomic_draw_path_frag);
                break;

            case DrawType::interiorTriangulation:
                vkutil::set_shader_code(
                    vsInfo,
                    spirv::atomic_draw_interior_triangles_vert);
                vkutil::set_shader_code_if_then_else(
                    fsInfo,
                    fixedFunctionColorOutput,
                    spirv::atomic_draw_interior_triangles_fixedcolor_frag,
                    spirv::atomic_draw_interior_triangles_frag);
                break;

            case DrawType::atlasBlit:
                vkutil::set_shader_code(vsInfo,
                                        spirv::atomic_draw_atlas_blit_vert);
                vkutil::set_shader_code_if_then_else(
                    fsInfo,
                    fixedFunctionColorOutput,
                    spirv::atomic_draw_atlas_blit_fixedcolor_frag,
                    spirv::atomic_draw_atlas_blit_frag);
                break;

            case DrawType::imageRect:
                vkutil::set_shader_code(vsInfo,
                                        spirv::atomic_draw_image_rect_vert);
                vkutil::set_shader_code_if_then_else(
                    fsInfo,
                    fixedFunctionColorOutput,
                    spirv::atomic_draw_image_rect_fixedcolor_frag,
                    spirv::atomic_draw_image_rect_frag);
                break;

            case DrawType::imageMesh:
                vkutil::set_shader_code(vsInfo,
                                        spirv::atomic_draw_image_mesh_vert);
                vkutil::set_shader_code_if_then_else(
                    fsInfo,
                    fixedFunctionColorOutput,
                    spirv::atomic_draw_image_mesh_fixedcolor_frag,
                    spirv::atomic_draw_image_mesh_frag);
                break;

            case DrawType::atomicResolve:
                if (shaderMiscFlags &
                    gpu::ShaderMiscFlags::coalescedResolveAndTransfer)
                {
                    vkutil::set_shader_code(
                        vsInfo,
                        spirv::atomic_resolve_coalesced_vert);
                    vkutil::set_shader_code(
                        fsInfo,
                        spirv::atomic_resolve_coalesced_frag);
                }
                else
                {
                    vkutil::set_shader_code(vsInfo, spirv::atomic_resolve_vert);
                    vkutil::set_shader_code_if_then_else(
                        fsInfo,
                        fixedFunctionColorOutput,
                        spirv::atomic_resolve_fixedcolor_frag,
                        spirv::atomic_resolve_frag);
                }
                break;

            case DrawType::atomicInitialize:
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStencilClipReset:
                RIVE_UNREACHABLE();
        }
    }
    else
    {
        assert(interlockMode == gpu::InterlockMode::clockwiseAtomic);
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
                vkutil::set_shader_code(vsInfo,
                                        spirv::draw_clockwise_path_vert);
                vkutil::set_shader_code(fsInfo,
                                        spirv::draw_clockwise_path_frag);
                break;

            case DrawType::interiorTriangulation:
                vkutil::set_shader_code(
                    vsInfo,
                    spirv::draw_clockwise_interior_triangles_vert);
                vkutil::set_shader_code(
                    fsInfo,
                    spirv::draw_clockwise_interior_triangles_frag);
                break;

            case DrawType::atlasBlit:
                vkutil::set_shader_code(vsInfo,
                                        spirv::draw_clockwise_atlas_blit_vert);
                vkutil::set_shader_code(fsInfo,
                                        spirv::draw_clockwise_atlas_blit_frag);
                break;

            case DrawType::imageMesh:
                vkutil::set_shader_code(vsInfo,
                                        spirv::draw_clockwise_image_mesh_vert);
                vkutil::set_shader_code(fsInfo,
                                        spirv::draw_clockwise_image_mesh_frag);
                break;

            case DrawType::imageRect:
            case DrawType::atomicResolve:
            case DrawType::atomicInitialize:
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStencilClipReset:
                RIVE_UNREACHABLE();
        }
    }

    VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                      &vsInfo,
                                      nullptr,
                                      &m_vertexModule));
    VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                      &fsInfo,
                                      nullptr,
                                      &m_fragmentModule));
}

DrawShaderVulkan::~DrawShaderVulkan()
{
    m_vk->DestroyShaderModule(m_vk->device, m_vertexModule, nullptr);
    m_vk->DestroyShaderModule(m_vk->device, m_fragmentModule, nullptr);
}
} // namespace rive::gpu

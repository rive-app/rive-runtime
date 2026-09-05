/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/vulkan_context.hpp"
#include "draw_shader_vulkan.hpp"
#include "vulkan_shaders.hpp"

namespace rive::gpu
{
DrawShaderVulkan::DrawShaderVulkan(Type type,
                                   VulkanContext* vk,
                                   DrawType drawType,
                                   ShaderFeatures shaderFeatures,
                                   InterlockMode interlockMode,
                                   ShaderMiscFlags shaderMiscFlags) :
    m_vk(ref_rcp(vk))
{
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

    const bool fixedFunctionColorOutput =
        enums::is_flag_set(shaderMiscFlags,
                           gpu::ShaderMiscFlags::fixedFunctionColorOutput);

    if (type == Type::fragment &&
        interlockMode == InterlockMode::depthStencil &&
        drawType != DrawType::renderPassInitialize &&
        drawType != DrawType::renderPassResolve)
    {
        // Fixed function color output and advanced blend are mutually exclusive
        // and one of them should always be set in depthStencil mode.
        assert(fixedFunctionColorOutput !=
               bool(shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND));
    }

    Span<const uint32_t> vertCode;
    Span<const uint32_t> fragCode;

    switch (interlockMode)
    {
        case gpu::InterlockMode::rasterOrdering:
        {
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::midpointFanCenterAAPatches:
                case DrawType::outerCurvePatches:
                    vertCode = spirv::draw_path_vert;
                    fragCode = spirv::draw_path_frag;
                    break;

                case DrawType::interiorTriangulation:
                    vertCode = spirv::draw_interior_triangles_vert;
                    fragCode = spirv::draw_interior_triangles_frag;
                    break;

                case DrawType::featherAtlasBlit:
                    vertCode = spirv::draw_atlas_blit_vert;
                    fragCode = spirv::draw_atlas_blit_frag;
                    break;

                case DrawType::imageMesh:
                    vertCode = spirv::draw_image_mesh_vert;
                    fragCode = spirv::draw_image_mesh_frag;
                    break;

                case DrawType::renderPassResolve:
                    vertCode = spirv::draw_fullscreen_quad_vert;
                    fragCode = spirv::draw_input_attachment_frag;
                    break;

                case DrawType::imageRect:
                case DrawType::depthStrokes:
                case DrawType::stencilMidpointFanBorrowedCoverage:
                case DrawType::stencilDynamicMidpointFans:
                case DrawType::stencilDynamicOuterCubics:
                case DrawType::stencilMidpointFans:
                case DrawType::stencilMidpointFanReset:
                case DrawType::stencilMidpointFanWinding:
                case DrawType::stencilMidpointFanCover:
                case DrawType::stencilOuterCubicBorrowedCoverage:
                case DrawType::stencilOuterCubicReset:
                case DrawType::stencilOuterCubicWinding:
                case DrawType::stencilOuterCubicCover:
                case DrawType::stencilOuterCubics:
                case DrawType::clipReset:
                case DrawType::renderPassInitialize:
                    RIVE_UNREACHABLE();
            }
            break;
        }

        case gpu::InterlockMode::atomics:
        {
#ifdef WITH_VULKAN_ATOMICS
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::midpointFanCenterAAPatches:
                case DrawType::outerCurvePatches:
                    vertCode = spirv::atomic_draw_path_vert;
                    fragCode = fixedFunctionColorOutput
                                   ? spirv::atomic_draw_path_fixedcolor_frag
                                   : spirv::atomic_draw_path_frag;
                    break;

                case DrawType::interiorTriangulation:
                    vertCode = spirv::atomic_draw_interior_triangles_vert;
                    fragCode =
                        fixedFunctionColorOutput
                            ? spirv::
                                  atomic_draw_interior_triangles_fixedcolor_frag
                            : spirv::atomic_draw_interior_triangles_frag;
                    break;

                case DrawType::featherAtlasBlit:
                    vertCode = spirv::atomic_draw_atlas_blit_vert;
                    fragCode =
                        fixedFunctionColorOutput
                            ? spirv::atomic_draw_atlas_blit_fixedcolor_frag
                            : spirv::atomic_draw_atlas_blit_frag;
                    break;

                case DrawType::imageRect:
                    vertCode = spirv::atomic_draw_image_rect_vert;
                    fragCode =
                        fixedFunctionColorOutput
                            ? spirv::atomic_draw_image_rect_fixedcolor_frag
                            : spirv::atomic_draw_image_rect_frag;
                    break;

                case DrawType::imageMesh:
                    vertCode = spirv::atomic_draw_image_mesh_vert;
                    fragCode =
                        fixedFunctionColorOutput
                            ? spirv::atomic_draw_image_mesh_fixedcolor_frag
                            : spirv::atomic_draw_image_mesh_frag;
                    break;

                case DrawType::renderPassResolve:
                    if (enums::is_flag_set(
                            shaderMiscFlags,
                            gpu::ShaderMiscFlags::coalescedResolveAndTransfer))
                    {
                        vertCode = spirv::atomic_resolve_coalesced_vert;
                        fragCode = spirv::atomic_resolve_coalesced_frag;
                    }
                    else
                    {
                        vertCode = spirv::atomic_resolve_vert;
                        fragCode = fixedFunctionColorOutput
                                       ? spirv::atomic_resolve_fixedcolor_frag
                                       : spirv::atomic_resolve_frag;
                    }
                    break;

                case DrawType::depthStrokes:
                case DrawType::stencilMidpointFanBorrowedCoverage:
                case DrawType::stencilDynamicMidpointFans:
                case DrawType::stencilDynamicOuterCubics:
                case DrawType::stencilMidpointFans:
                case DrawType::stencilMidpointFanReset:
                case DrawType::stencilMidpointFanWinding:
                case DrawType::stencilMidpointFanCover:
                case DrawType::stencilOuterCubicBorrowedCoverage:
                case DrawType::stencilOuterCubicReset:
                case DrawType::stencilOuterCubicWinding:
                case DrawType::stencilOuterCubicCover:
                case DrawType::stencilOuterCubics:
                case DrawType::clipReset:
                case DrawType::renderPassInitialize:
                    RIVE_UNREACHABLE();
            }
            break;
#else
            RIVE_UNREACHABLE();
#endif
        }

        case gpu::InterlockMode::clockwise:
        {
#ifndef RIVE_ANDROID
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::midpointFanCenterAAPatches:
                case DrawType::outerCurvePatches:
                    vertCode = spirv::draw_clockwise_path_vert;
                    fragCode =
                        enums::is_flag_set(shaderMiscFlags,
                                           gpu::ShaderMiscFlags::clipUpdateOnly)
                            ? fixedFunctionColorOutput
                                  ? spirv::draw_clockwise_clip_fixedcolor_frag
                                  : spirv::draw_clockwise_clip_frag
                        : fixedFunctionColorOutput
                            ? spirv::draw_clockwise_path_fixedcolor_frag
                            : spirv::draw_clockwise_path_frag;
                    break;

                case DrawType::interiorTriangulation:
                    vertCode = spirv::draw_clockwise_interior_triangles_vert;
                    fragCode =
                        enums::is_flag_set(shaderMiscFlags,
                                           gpu::ShaderMiscFlags::clipUpdateOnly)
                            ? fixedFunctionColorOutput
                                  ? spirv::
                                        draw_clockwise_clip_interior_triangles_fixedcolor_frag

                                  : spirv::
                                        draw_clockwise_clip_interior_triangles_frag
                        : fixedFunctionColorOutput
                            ? spirv::
                                  draw_clockwise_interior_triangles_fixedcolor_frag
                            : spirv::draw_clockwise_interior_triangles_frag;
                    break;

                case DrawType::featherAtlasBlit:
                    vertCode = spirv::draw_clockwise_atlas_blit_vert;
                    fragCode =
                        fixedFunctionColorOutput
                            ? spirv::draw_clockwise_atlas_blit_fixedcolor_frag
                            : spirv::draw_clockwise_atlas_blit_frag;
                    break;

                case DrawType::imageMesh:
                    vertCode = spirv::draw_clockwise_image_mesh_vert;
                    fragCode =
                        fixedFunctionColorOutput
                            ? spirv::draw_clockwise_image_mesh_fixedcolor_frag
                            : spirv::draw_clockwise_image_mesh_frag;
                    break;

                case DrawType::imageRect:
                case DrawType::depthStrokes:
                case DrawType::stencilMidpointFanBorrowedCoverage:
                case DrawType::stencilDynamicMidpointFans:
                case DrawType::stencilDynamicOuterCubics:
                case DrawType::stencilMidpointFans:
                case DrawType::stencilMidpointFanReset:
                case DrawType::stencilMidpointFanWinding:
                case DrawType::stencilMidpointFanCover:
                case DrawType::stencilOuterCubicBorrowedCoverage:
                case DrawType::stencilOuterCubicReset:
                case DrawType::stencilOuterCubicWinding:
                case DrawType::stencilOuterCubicCover:
                case DrawType::stencilOuterCubics:
                case DrawType::clipReset:
                case DrawType::renderPassResolve:
                case DrawType::renderPassInitialize:
                    RIVE_UNREACHABLE();
            }
            break;
#else
            RIVE_UNREACHABLE();
#endif
        }

        case gpu::InterlockMode::clockwiseAtomic:
        {
#ifdef WITH_VULKAN_ATOMICS
            // Since advanced blend is done via input attachments in
            // clockwiseAtomic mode, we can swap out the "_fixedcolor" shader
            // variants on a per-draw basis instead of per render pass.
            const bool drawUsesAdvancedBlend =
                enums::is_flag_set(shaderFeatures,
                                   gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND);
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::midpointFanCenterAAPatches:
                case DrawType::outerCurvePatches:
                    vertCode = spirv::draw_clockwise_atomic_path_vert;
                    if (enums::is_flag_set(
                            shaderMiscFlags,
                            gpu::ShaderMiscFlags::borrowedCoveragePass))
                    {
                        assert(fixedFunctionColorOutput);
                        assert(!enums::any_flag_set(
                            shaderMiscFlags,
                            gpu::ShaderMiscFlags::clipUpdateOnly |
                                gpu::ShaderMiscFlags::nestedClipUpdateOnly));
                        assert(!drawUsesAdvancedBlend);
                        fragCode =
                            spirv::draw_clockwise_atomic_borrowed_coverage_frag;
                    }
                    else if (enums::any_flag_set(
                                 shaderMiscFlags,
                                 gpu::ShaderMiscFlags::clipUpdateOnly |
                                     gpu::ShaderMiscFlags::
                                         nestedClipUpdateOnly))
                    {
                        fragCode =
                            !drawUsesAdvancedBlend
                                ? spirv::
                                      draw_clockwise_atomic_clip_fixedcolor_frag
                                : spirv::draw_clockwise_atomic_clip_frag;
                    }
                    else
                    {
                        fragCode =
                            !drawUsesAdvancedBlend
                                ? spirv::
                                      draw_clockwise_atomic_path_fixedcolor_frag
                                : spirv::draw_clockwise_atomic_path_frag;
                    }
                    break;

                case DrawType::interiorTriangulation:
                    vertCode =
                        spirv::draw_clockwise_atomic_interior_triangles_vert;
                    if (enums::is_flag_set(
                            shaderMiscFlags,
                            gpu::ShaderMiscFlags::borrowedCoveragePass))
                    {
                        assert(fixedFunctionColorOutput);
                        assert(!enums::any_flag_set(
                            shaderMiscFlags,
                            gpu::ShaderMiscFlags::clipUpdateOnly |
                                gpu::ShaderMiscFlags::nestedClipUpdateOnly));
                        assert(!drawUsesAdvancedBlend);
                        fragCode = spirv::
                            draw_clockwise_atomic_borrowed_coverage_interior_triangles_frag;
                    }
                    else if (enums::any_flag_set(
                                 shaderMiscFlags,
                                 gpu::ShaderMiscFlags::clipUpdateOnly |
                                     gpu::ShaderMiscFlags::
                                         nestedClipUpdateOnly))
                    {
                        fragCode =
                            !drawUsesAdvancedBlend
                                ? spirv::
                                      draw_clockwise_atomic_clip_interior_triangles_fixedcolor_frag
                                : spirv::
                                      draw_clockwise_atomic_clip_interior_triangles_frag;
                    }
                    else
                    {
                        fragCode =
                            !drawUsesAdvancedBlend
                                ? spirv::
                                      draw_clockwise_atomic_interior_triangles_fixedcolor_frag
                                : spirv::
                                      draw_clockwise_atomic_interior_triangles_frag;
                    }
                    break;

                case DrawType::featherAtlasBlit:
                    vertCode = spirv::draw_clockwise_atomic_atlas_blit_vert;
                    fragCode =
                        !drawUsesAdvancedBlend
                            ? spirv::
                                  draw_clockwise_atomic_atlas_blit_fixedcolor_frag
                            : spirv::draw_clockwise_atomic_atlas_blit_frag;
                    break;

                case DrawType::imageMesh:
                    vertCode = spirv::draw_clockwise_atomic_image_mesh_vert;
                    fragCode =
                        !drawUsesAdvancedBlend
                            ? spirv::
                                  draw_clockwise_atomic_image_mesh_fixedcolor_frag
                            : spirv::draw_clockwise_atomic_image_mesh_frag;
                    break;

                case DrawType::clipReset:
                    vertCode = spirv::clear_clockwise_atomic_clip_vert;
                    fragCode =
                        !drawUsesAdvancedBlend
                            ? spirv::clear_clockwise_atomic_clip_fixedcolor_frag
                            : spirv::clear_clockwise_atomic_clip_frag;
                    break;

                case DrawType::renderPassInitialize:
                    vertCode = spirv::draw_fullscreen_quad_vert;
                    fragCode =
                        fixedFunctionColorOutput
                            ? spirv::
                                  init_clockwise_atomic_workaround_fixedcolor_frag
                            : spirv::init_clockwise_atomic_workaround_frag;
                    break;

                case DrawType::imageRect:
                case DrawType::depthStrokes:
                case DrawType::stencilMidpointFanBorrowedCoverage:
                case DrawType::stencilDynamicMidpointFans:
                case DrawType::stencilDynamicOuterCubics:
                case DrawType::stencilMidpointFans:
                case DrawType::stencilMidpointFanReset:
                case DrawType::stencilMidpointFanWinding:
                case DrawType::stencilMidpointFanCover:
                case DrawType::stencilOuterCubicBorrowedCoverage:
                case DrawType::stencilOuterCubicReset:
                case DrawType::stencilOuterCubicWinding:
                case DrawType::stencilOuterCubicCover:
                case DrawType::stencilOuterCubics:
                case DrawType::renderPassResolve:
                    RIVE_UNREACHABLE();
            }
            break;
#else
            RIVE_UNREACHABLE();
#endif
        }

        case gpu::InterlockMode::depthStencil:
        {
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::midpointFanCenterAAPatches:
                case DrawType::outerCurvePatches:
                    RIVE_UNREACHABLE();

                case DrawType::stencilOuterCubicBorrowedCoverage:
                case DrawType::stencilOuterCubicReset:
                case DrawType::stencilOuterCubicWinding:
                case DrawType::stencilOuterCubicCover:
                case DrawType::stencilOuterCubics:
                case DrawType::depthStrokes:
                case DrawType::stencilMidpointFanBorrowedCoverage:
                case DrawType::stencilDynamicMidpointFans:
                case DrawType::stencilDynamicOuterCubics:
                case DrawType::stencilMidpointFans:
                case DrawType::stencilMidpointFanReset:
                case DrawType::stencilMidpointFanWinding:
                case DrawType::stencilMidpointFanCover:
                    vertCode =
                        enums::is_flag_set(shaderFeatures,
                                           ShaderFeatures::ENABLE_CLIP_RECT)
                            ? spirv::draw_depthstencil_path_vert
                            : spirv::draw_depthstencil_path_noclipdistance_vert;
                    fragCode =
                        fixedFunctionColorOutput
                            ? spirv::draw_depthstencil_path_fixedcolor_frag
                            : spirv::draw_depthstencil_path_frag;
                    break;

                case DrawType::clipReset:
                    vertCode = spirv::draw_depthstencil_triangles_nocolor_vert;
                    fragCode = spirv::draw_depthstencil_triangles_nocolor_frag;
                    break;

                case DrawType::interiorTriangulation:
                    // depthStencil interior triangles are smuggled in with
                    // outerCubic patches instead of using the
                    // interiorTriangulation draw type.
                    RIVE_UNREACHABLE();
                    break;

                case DrawType::featherAtlasBlit:
                    vertCode =
                        enums::is_flag_set(shaderFeatures,
                                           ShaderFeatures::ENABLE_CLIP_RECT)
                            ? spirv::draw_depthstencil_atlas_blit_vert
                            : spirv::
                                  draw_depthstencil_atlas_blit_noclipdistance_vert;
                    fragCode =
                        fixedFunctionColorOutput
                            ? spirv::
                                  draw_depthstencil_atlas_blit_fixedcolor_frag
                            : spirv::draw_depthstencil_atlas_blit_frag;
                    break;

                case DrawType::imageMesh:
                    vertCode =
                        enums::is_flag_set(shaderFeatures,
                                           ShaderFeatures::ENABLE_CLIP_RECT)
                            ? spirv::draw_depthstencil_image_mesh_vert
                            : spirv::
                                  draw_depthstencil_image_mesh_noclipdistance_vert;
                    fragCode =
                        fixedFunctionColorOutput
                            ? spirv::
                                  draw_depthstencil_image_mesh_fixedcolor_frag
                            : spirv::draw_depthstencil_image_mesh_frag;
                    break;

                case DrawType::renderPassInitialize:
                    // depthStencil render passes get initialized by drawing the
                    // previous contents into the framebuffer.
                    // (LoadAction::preserveRenderTarget only.)
                    vertCode = spirv::draw_fullscreen_quad_vert;
                    fragCode = spirv::draw_msaa_color_seed_attachment_frag;
                    break;

                case DrawType::renderPassResolve:
                    vertCode = spirv::draw_fullscreen_quad_vert;
                    fragCode = spirv::draw_msaa_resolve_frag;
                    break;

                case DrawType::imageRect:
                    RIVE_UNREACHABLE();
            }
            break;
        }
    }

    Span<const uint32_t> code;
    switch (type)
    {
        case Type::vertex:
            code = vertCode;
            break;
        case Type::fragment:
            code = fragCode;
            break;
    }

    assert(code.size_bytes() > 0);
    createInfo.pCode = code.data();
    createInfo.codeSize = code.size_bytes();

    if (m_vk->CreateShaderModule(m_vk->device,
                                 &createInfo,
                                 nullptr,
                                 &m_module) != VK_SUCCESS)
    {
        m_module = VK_NULL_HANDLE;
    }
}

DrawShaderVulkan::~DrawShaderVulkan()
{
    m_vk->DestroyShaderModule(m_vk->device, m_module, nullptr);
}
} // namespace rive::gpu

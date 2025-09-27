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
        shaderMiscFlags & gpu::ShaderMiscFlags::fixedFunctionColorOutput;

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

                case DrawType::atlasBlit:
                    vertCode = spirv::draw_atlas_blit_vert;
                    fragCode = spirv::draw_atlas_blit_frag;
                    break;

                case DrawType::imageMesh:
                    vertCode = spirv::draw_image_mesh_vert;
                    fragCode = spirv::draw_image_mesh_frag;
                    break;

                case DrawType::imageRect:
                case DrawType::msaaStrokes:
                case DrawType::msaaMidpointFanBorrowedCoverage:
                case DrawType::msaaMidpointFans:
                case DrawType::msaaMidpointFanStencilReset:
                case DrawType::msaaMidpointFanPathsStencil:
                case DrawType::msaaMidpointFanPathsCover:
                case DrawType::msaaOuterCubics:
                case DrawType::msaaStencilClipReset:
                case DrawType::renderPassResolve:
                case DrawType::renderPassInitialize:
                    RIVE_UNREACHABLE();
            }
            break;
        }

        case gpu::InterlockMode::atomics:
        {
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

                case DrawType::atlasBlit:
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
                    if (shaderMiscFlags &
                        gpu::ShaderMiscFlags::coalescedResolveAndTransfer)
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

                case DrawType::msaaStrokes:
                case DrawType::msaaMidpointFanBorrowedCoverage:
                case DrawType::msaaMidpointFans:
                case DrawType::msaaMidpointFanStencilReset:
                case DrawType::msaaMidpointFanPathsStencil:
                case DrawType::msaaMidpointFanPathsCover:
                case DrawType::msaaOuterCubics:
                case DrawType::msaaStencilClipReset:
                case DrawType::renderPassInitialize:
                    RIVE_UNREACHABLE();
            }
            break;
        }

        case gpu::InterlockMode::clockwiseAtomic:
        {
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::midpointFanCenterAAPatches:
                case DrawType::outerCurvePatches:
                    vertCode = spirv::draw_clockwise_path_vert;
                    fragCode = spirv::draw_clockwise_path_frag;
                    break;

                case DrawType::interiorTriangulation:
                    vertCode = spirv::draw_clockwise_interior_triangles_vert;
                    fragCode = spirv::draw_clockwise_interior_triangles_frag;
                    break;

                case DrawType::atlasBlit:
                    vertCode = spirv::draw_clockwise_atlas_blit_vert;
                    fragCode = spirv::draw_clockwise_atlas_blit_frag;
                    break;

                case DrawType::imageMesh:
                    vertCode = spirv::draw_clockwise_image_mesh_vert;
                    fragCode = spirv::draw_clockwise_image_mesh_frag;
                    break;

                case DrawType::imageRect:
                case DrawType::msaaStrokes:
                case DrawType::msaaMidpointFanBorrowedCoverage:
                case DrawType::msaaMidpointFans:
                case DrawType::msaaMidpointFanStencilReset:
                case DrawType::msaaMidpointFanPathsStencil:
                case DrawType::msaaMidpointFanPathsCover:
                case DrawType::msaaOuterCubics:
                case DrawType::msaaStencilClipReset:
                case DrawType::renderPassResolve:
                case DrawType::renderPassInitialize:
                    RIVE_UNREACHABLE();
            }
            break;
        }

        case gpu::InterlockMode::msaa:
        {
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::midpointFanCenterAAPatches:
                case DrawType::outerCurvePatches:
                    RIVE_UNREACHABLE();

                case DrawType::msaaOuterCubics:
                case DrawType::msaaStrokes:
                case DrawType::msaaMidpointFanBorrowedCoverage:
                case DrawType::msaaMidpointFans:
                case DrawType::msaaMidpointFanStencilReset:
                case DrawType::msaaMidpointFanPathsStencil:
                case DrawType::msaaMidpointFanPathsCover:
                    vertCode =
                        (shaderFeatures & ShaderFeatures::ENABLE_CLIP_RECT)
                            ? spirv::draw_msaa_path_vert
                            : spirv::draw_msaa_path_noclipdistance_vert;
                    fragCode = fixedFunctionColorOutput
                                   ? spirv::draw_msaa_path_fixedcolor_frag
                                   : spirv::draw_msaa_path_frag;
                    break;

                case DrawType::msaaStencilClipReset:
                    vertCode = spirv::draw_msaa_stencil_vert;
                    fragCode = spirv::draw_msaa_stencil_frag;
                    break;

                case DrawType::interiorTriangulation:
                    // Interior triangulation is not yet implemented for MSAA.
                    RIVE_UNREACHABLE();
                    break;

                case DrawType::atlasBlit:
                    vertCode =
                        (shaderFeatures & ShaderFeatures::ENABLE_CLIP_RECT)
                            ? spirv::draw_msaa_atlas_blit_vert
                            : spirv::draw_msaa_atlas_blit_noclipdistance_vert;
                    fragCode = fixedFunctionColorOutput
                                   ? spirv::draw_msaa_atlas_blit_fixedcolor_frag
                                   : spirv::draw_msaa_atlas_blit_frag;
                    break;

                case DrawType::imageMesh:
                    vertCode =
                        (shaderFeatures & ShaderFeatures::ENABLE_CLIP_RECT)
                            ? spirv::draw_msaa_image_mesh_vert
                            : spirv::draw_msaa_image_mesh_noclipdistance_vert;
                    fragCode = fixedFunctionColorOutput
                                   ? spirv::draw_msaa_image_mesh_fixedcolor_frag
                                   : spirv::draw_msaa_image_mesh_frag;
                    break;

                case DrawType::renderPassInitialize:
                    // MSAA render passes get initialized by drawing the
                    // previous contents into the framebuffer.
                    // (LoadAction::preserveRenderTarget only.)
                    vertCode = spirv::copy_attachment_to_attachment_vert;
                    fragCode = spirv::copy_attachment_to_attachment_frag;
                    break;

                case DrawType::imageRect:
                case DrawType::renderPassResolve:
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

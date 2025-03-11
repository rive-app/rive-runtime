/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"

#include "rive/renderer/stack_vector.hpp"
#include "rive/renderer/texture.hpp"
#include "rive/renderer/rive_render_buffer.hpp"
#include "shaders/constants.glsl"

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

namespace spirv_embedded
{
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
#include "generated/shaders/spirv/atomic_resolve_pls.vert.h"
#include "generated/shaders/spirv/atomic_resolve_pls.frag.h"
#include "generated/shaders/spirv/atomic_resolve_pls.fixedcolor_frag.h"

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
rive::Span<const uint32_t> atomic_resolve_pls_vert =
    rive::make_span(spirv_embedded::atomic_resolve_pls_vert);
rive::Span<const uint32_t> atomic_resolve_pls_frag =
    rive::make_span(spirv_embedded::atomic_resolve_pls_frag);
rive::Span<const uint32_t> atomic_resolve_pls_fixedcolor_frag = rive::make_span(
    spirv_embedded::atomic_resolve_pls_fixedcolor_frag,
    std::size(spirv_embedded::atomic_resolve_pls_fixedcolor_frag));
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
}; // namespace spirv

// Common layout descriptors shared by various pipelines.
namespace layout
{
constexpr static VkVertexInputBindingDescription PATH_INPUT_BINDINGS[] = {{
    .binding = 0,
    .stride = sizeof(rive::gpu::PatchVertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
}};
constexpr static VkVertexInputAttributeDescription PATH_VERTEX_ATTRIBS[] = {
    {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = 0,
    },
    {
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = 4 * sizeof(float),
    },
};
constexpr static VkPipelineVertexInputStateCreateInfo PATH_VERTEX_INPUT_STATE =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::size(PATH_INPUT_BINDINGS),
        .pVertexBindingDescriptions = PATH_INPUT_BINDINGS,
        .vertexAttributeDescriptionCount = std::size(PATH_VERTEX_ATTRIBS),
        .pVertexAttributeDescriptions = PATH_VERTEX_ATTRIBS,
};

constexpr static VkVertexInputBindingDescription INTERIOR_TRI_INPUT_BINDINGS[] =
    {{
        .binding = 0,
        .stride = sizeof(rive::gpu::TriangleVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }};
constexpr static VkVertexInputAttributeDescription
    INTERIOR_TRI_VERTEX_ATTRIBS[] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0,
        },
};
constexpr static VkPipelineVertexInputStateCreateInfo
    INTERIOR_TRI_VERTEX_INPUT_STATE = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::size(INTERIOR_TRI_INPUT_BINDINGS),
        .pVertexBindingDescriptions = INTERIOR_TRI_INPUT_BINDINGS,
        .vertexAttributeDescriptionCount =
            std::size(INTERIOR_TRI_VERTEX_ATTRIBS),
        .pVertexAttributeDescriptions = INTERIOR_TRI_VERTEX_ATTRIBS,
};

constexpr static VkVertexInputBindingDescription IMAGE_RECT_INPUT_BINDINGS[] = {
    {
        .binding = 0,
        .stride = sizeof(rive::gpu::ImageRectVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }};
constexpr static VkVertexInputAttributeDescription IMAGE_RECT_VERTEX_ATTRIBS[] =
    {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 0,
        },
};
constexpr static VkPipelineVertexInputStateCreateInfo
    IMAGE_RECT_VERTEX_INPUT_STATE = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::size(IMAGE_RECT_INPUT_BINDINGS),
        .pVertexBindingDescriptions = IMAGE_RECT_INPUT_BINDINGS,
        .vertexAttributeDescriptionCount = std::size(IMAGE_RECT_VERTEX_ATTRIBS),
        .pVertexAttributeDescriptions = IMAGE_RECT_VERTEX_ATTRIBS,
};

constexpr static VkVertexInputBindingDescription IMAGE_MESH_INPUT_BINDINGS[] = {
    {
        .binding = 0,
        .stride = sizeof(float) * 2,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
    {
        .binding = 1,
        .stride = sizeof(float) * 2,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
};
constexpr static VkVertexInputAttributeDescription IMAGE_MESH_VERTEX_ATTRIBS[] =
    {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 0,
        },
        {
            .location = 1,
            .binding = 1,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 0,
        },
};
constexpr static VkPipelineVertexInputStateCreateInfo
    IMAGE_MESH_VERTEX_INPUT_STATE = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::size(IMAGE_MESH_INPUT_BINDINGS),
        .pVertexBindingDescriptions = IMAGE_MESH_INPUT_BINDINGS,
        .vertexAttributeDescriptionCount = std::size(IMAGE_MESH_VERTEX_ATTRIBS),
        .pVertexAttributeDescriptions = IMAGE_MESH_VERTEX_ATTRIBS,
};

constexpr static VkPipelineVertexInputStateCreateInfo EMPTY_VERTEX_INPUT_STATE =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
};

constexpr static VkPipelineInputAssemblyStateCreateInfo
    INPUT_ASSEMBLY_TRIANGLE_STRIP = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
};

constexpr static VkPipelineInputAssemblyStateCreateInfo
    INPUT_ASSEMBLY_TRIANGLE_LIST = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
};

constexpr static VkPipelineViewportStateCreateInfo SINGLE_VIEWPORT = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
};

constexpr static VkPipelineRasterizationStateCreateInfo
    RASTER_STATE_CULL_BACK_CCW = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.f,
};

constexpr static VkPipelineRasterizationStateCreateInfo
    RASTER_STATE_CULL_BACK_CW = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.f,
};

constexpr static VkPipelineMultisampleStateCreateInfo MSAA_DISABLED = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
};

constexpr static VkPipelineColorBlendAttachmentState BLEND_DISABLED_VALUES = {
    .colorWriteMask = rive::gpu::vkutil::kColorWriteMaskRGBA};
constexpr static VkPipelineColorBlendStateCreateInfo
    SINGLE_ATTACHMENT_BLEND_DISABLED = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &BLEND_DISABLED_VALUES,
};

constexpr static VkDynamicState DYNAMIC_VIEWPORT_SCISSOR_VALUES[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
};
constexpr static VkPipelineDynamicStateCreateInfo DYNAMIC_VIEWPORT_SCISSOR = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = std::size(DYNAMIC_VIEWPORT_SCISSOR_VALUES),
    .pDynamicStates = DYNAMIC_VIEWPORT_SCISSOR_VALUES,
};

constexpr static VkAttachmentReference SINGLE_ATTACHMENT_SUBPASS_REFERENCE = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
};
constexpr static VkSubpassDescription SINGLE_ATTACHMENT_SUBPASS = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &SINGLE_ATTACHMENT_SUBPASS_REFERENCE,
};
} // namespace layout

namespace rive::gpu
{
void RenderContextVulkanImpl::hotloadShaders(
    rive::Span<const uint32_t> spirvData)
{
    m_vk->DeviceWaitIdle(m_vk->device);

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
    spirv::atomic_resolve_pls_vert = readNextBytecodeSpan();
    spirv::atomic_resolve_pls_frag = readNextBytecodeSpan();
    spirv::atomic_resolve_pls_fixedcolor_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_path_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_path_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_interior_triangles_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_interior_triangles_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_atlas_blit_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_atlas_blit_frag = readNextBytecodeSpan();
    spirv::draw_clockwise_image_mesh_vert = readNextBytecodeSpan();
    spirv::draw_clockwise_image_mesh_frag = readNextBytecodeSpan();

    // Delete and replace old shaders
    m_colorRampPipeline = std::make_unique<ColorRampPipeline>(this);
    m_tessellatePipeline = std::make_unique<TessellatePipeline>(this);
    m_atlasPipeline = std::make_unique<AtlasPipeline>(this);
    m_drawShaders.clear();
    m_drawPipelines.clear();
}

static VkFormat get_preferred_depth_stencil_format(bool isD24S8Supported)
{
    return isD24S8Supported ? VK_FORMAT_D24_UNORM_S8_UINT
                            : VK_FORMAT_D32_SFLOAT_S8_UINT;
}

static VkBufferUsageFlagBits render_buffer_usage_flags(
    RenderBufferType renderBufferType)
{
    switch (renderBufferType)
    {
        case RenderBufferType::index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case RenderBufferType::vertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    RIVE_UNREACHABLE();
}

class RenderBufferVulkanImpl
    : public LITE_RTTI_OVERRIDE(RiveRenderBuffer, RenderBufferVulkanImpl)
{
public:
    RenderBufferVulkanImpl(rcp<VulkanContext> vk,
                           RenderBufferType renderBufferType,
                           RenderBufferFlags renderBufferFlags,
                           size_t sizeInBytes) :
        lite_rtti_override(renderBufferType, renderBufferFlags, sizeInBytes),
        m_bufferRing(std::move(vk),
                     render_buffer_usage_flags(renderBufferType),
                     vkutil::Mappability::writeOnly,
                     sizeInBytes)
    {}

    VkBuffer frontVkBuffer()
    {
        return m_bufferRing.vkBufferAt(frontBufferIdx());
    }

    const VkBuffer* frontVkBufferAddressOf()
    {
        return m_bufferRing.vkBufferAtAddressOf(frontBufferIdx());
    }

protected:
    void* onMap() override
    {
        m_bufferRing.synchronizeSizeAt(backBufferIdx());
        return m_bufferRing.contentsAt(backBufferIdx());
    }

    void onUnmap() override { m_bufferRing.flushContentsAt(backBufferIdx()); }

private:
    vkutil::BufferRing m_bufferRing;
};

rcp<RenderBuffer> RenderContextVulkanImpl::makeRenderBuffer(
    RenderBufferType type,
    RenderBufferFlags flags,
    size_t sizeInBytes)
{
    return make_rcp<RenderBufferVulkanImpl>(m_vk, type, flags, sizeInBytes);
}

enum class TextureFormat
{
    rgba8,
    r16f,
};

constexpr static VkFormat vulkan_texture_format(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::rgba8:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::r16f:
            return VK_FORMAT_R16_SFLOAT;
    }
    RIVE_UNREACHABLE();
}

constexpr static size_t vulkan_texture_bytes_per_pixel(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::rgba8:
            return 4;
        case TextureFormat::r16f:
            return 2;
    }
    RIVE_UNREACHABLE();
}

class TextureVulkanImpl : public Texture
{
public:
    TextureVulkanImpl(rcp<VulkanContext> vk,
                      uint32_t width,
                      uint32_t height,
                      uint32_t mipLevelCount,
                      TextureFormat format,
                      const void* imageData) :
        Texture(width, height),
        m_vk(std::move(vk)),
        m_texture(m_vk->makeTexture({
            .format = vulkan_texture_format(format),
            .extent = {width, height, 1},
            .mipLevels = mipLevelCount,
            .usage =
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        })),
        m_textureView(m_vk->makeTextureView(m_texture)),
        m_imageUploadBuffer(m_vk->makeBuffer(
            {
                .size = height * width * vulkan_texture_bytes_per_pixel(format),
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            },
            vkutil::Mappability::writeOnly))
    {
        memcpy(m_imageUploadBuffer->contents(),
               imageData,
               m_imageUploadBuffer->info().size);
        m_imageUploadBuffer->flushContents();
    }

    bool hasUpdates() const { return m_imageUploadBuffer != nullptr; }

    void synchronize(VkCommandBuffer commandBuffer) const
    {
        assert(hasUpdates());

        // Upload the new image.
        VkBufferImageCopy bufferImageCopy = {
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                },
            .imageExtent = {width(), height(), 1},
        };

        m_vk->imageMemoryBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            {
                .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = *m_texture,
                .subresourceRange =
                    {
                        .baseMipLevel = 0,
                        .levelCount = m_texture->info().mipLevels,
                    },
            });

        m_vk->CmdCopyBufferToImage(commandBuffer,
                                   *m_imageUploadBuffer,
                                   *m_texture,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1,
                                   &bufferImageCopy);

        uint32_t mipLevels = m_texture->info().mipLevels;
        if (mipLevels > 1)
        {
            // Generate mipmaps.
            int2 dstSize, srcSize = {static_cast<int32_t>(width()),
                                     static_cast<int32_t>(height())};
            for (uint32_t level = 1; level < mipLevels;
                 ++level, srcSize = dstSize)
            {
                dstSize = simd::max(srcSize >> 1, int2(1));

                VkImageBlit imageBlit = {
                    .srcSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = level - 1,
                            .layerCount = 1,
                        },
                    .dstSubresource =
                        {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel = level,
                            .layerCount = 1,
                        },
                };

                imageBlit.srcOffsets[0] = {0, 0, 0};
                imageBlit.srcOffsets[1] = {srcSize.x, srcSize.y, 1};

                imageBlit.dstOffsets[0] = {0, 0, 0};
                imageBlit.dstOffsets[1] = {dstSize.x, dstSize.y, 1};

                m_vk->imageMemoryBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    {
                        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        .image = *m_texture,
                        .subresourceRange =
                            {
                                .baseMipLevel = level - 1,
                                .levelCount = 1,
                            },
                    });

                m_vk->CmdBlitImage(commandBuffer,
                                   *m_texture,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   *m_texture,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1,
                                   &imageBlit,
                                   VK_FILTER_LINEAR);
            }

            m_vk->imageMemoryBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                {
                    .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .image = *m_texture,
                    .subresourceRange =
                        {
                            .baseMipLevel = 0,
                            .levelCount = mipLevels - 1,
                        },
                });
        }

        m_vk->imageMemoryBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            {
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .image = *m_texture,
                .subresourceRange =
                    {
                        .baseMipLevel = mipLevels - 1,
                        .levelCount = 1,
                    },
            });

        m_imageUploadBuffer = nullptr;
    }

private:
    friend class RenderContextVulkanImpl;

    rcp<VulkanContext> m_vk;
    rcp<vkutil::Texture> m_texture;
    rcp<vkutil::TextureView> m_textureView;

    mutable rcp<vkutil::Buffer> m_imageUploadBuffer;

    // Location for RenderContextVulkanImpl to store a descriptor set for the
    // current flush that binds this image texture.
    mutable VkDescriptorSet m_imageTextureDescriptorSet = VK_NULL_HANDLE;
    mutable uint64_t m_descriptorSetFrameIdx =
        std::numeric_limits<size_t>::max();
};

rcp<Texture> RenderContextVulkanImpl::decodeImageTexture(
    Span<const uint8_t> encodedBytes)
{
#ifdef RIVE_DECODERS
    auto bitmap = Bitmap::decode(encodedBytes.data(), encodedBytes.size());
    if (bitmap)
    {
        // For now, RenderContextImpl::makeImageTexture() only accepts RGBA.
        if (bitmap->pixelFormat() != Bitmap::PixelFormat::RGBA)
        {
            bitmap->pixelFormat(Bitmap::PixelFormat::RGBA);
        }
        uint32_t width = bitmap->width();
        uint32_t height = bitmap->height();
        uint32_t mipLevelCount = math::msb(height | width);
        return make_rcp<TextureVulkanImpl>(m_vk,
                                           width,
                                           height,
                                           mipLevelCount,
                                           TextureFormat::rgba8,
                                           bitmap->bytes());
    }
#endif
    return nullptr;
}

// Renders color ramps to the gradient texture.
class RenderContextVulkanImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(RenderContextVulkanImpl* impl) :
        m_vk(ref_rcp(impl->vulkanContext()))
    {
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &impl->m_perFlushDescriptorSetLayout,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::color_ramp_vert.size_bytes(),
            .pCode = spirv::color_ramp_vert.data(),
        };

        VkShaderModule vertexShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &vertexShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::color_ramp_frag.size_bytes(),
            .pCode = spirv::color_ramp_frag.data(),
        };

        VkShaderModule fragmentShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &fragmentShader));

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertexShader,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragmentShader,
                .pName = "main",
            },
        };

        VkVertexInputBindingDescription vertexInputBindingDescription = {
            .binding = 0,
            .stride = sizeof(gpu::GradientSpan),
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
        };

        VkVertexInputAttributeDescription vertexAttributeDescription = {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_UINT,
        };

        VkPipelineVertexInputStateCreateInfo
            pipelineVertexInputStateCreateInfo = {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertexInputBindingDescription,
                .vertexAttributeDescriptionCount = 1,
                .pVertexAttributeDescriptions = &vertexAttributeDescription,
            };

        VkAttachmentDescription attachment = {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &layout::SINGLE_ATTACHMENT_SUBPASS,
        };

        VK_CHECK(m_vk->CreateRenderPass(m_vk->device,
                                        &renderPassCreateInfo,
                                        nullptr,
                                        &m_renderPass));

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &pipelineVertexInputStateCreateInfo,
            .pInputAssemblyState = &layout::INPUT_ASSEMBLY_TRIANGLE_STRIP,
            .pViewportState = &layout::SINGLE_VIEWPORT,
            .pRasterizationState = &layout::RASTER_STATE_CULL_BACK_CCW,
            .pMultisampleState = &layout::MSAA_DISABLED,
            .pColorBlendState = &layout::SINGLE_ATTACHMENT_BLEND_DISABLED,
            .pDynamicState = &layout::DYNAMIC_VIEWPORT_SCISSOR,
            .layout = m_pipelineLayout,
            .renderPass = m_renderPass,
        };

        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &pipelineCreateInfo,
                                               nullptr,
                                               &m_renderPipeline));

        m_vk->DestroyShaderModule(m_vk->device, vertexShader, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, fragmentShader, nullptr);
    }

    ~ColorRampPipeline()
    {
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
        m_vk->DestroyRenderPass(m_vk->device, m_renderPass, nullptr);
        m_vk->DestroyPipeline(m_vk->device, m_renderPipeline, nullptr);
    }

    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkPipeline renderPipeline() const { return m_renderPipeline; }

private:
    rcp<VulkanContext> m_vk;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_renderPipeline;
};

// Renders tessellated vertices to the tessellation texture.
class RenderContextVulkanImpl::TessellatePipeline
{
public:
    TessellatePipeline(RenderContextVulkanImpl* impl) :
        m_vk(ref_rcp(impl->vulkanContext()))
    {
        VkDescriptorSetLayout pipelineDescriptorSetLayouts[] = {
            impl->m_perFlushDescriptorSetLayout,
            impl->m_emptyDescriptorSetLayout,
            impl->m_immutableSamplerDescriptorSetLayout,
        };
        static_assert(PER_FLUSH_BINDINGS_SET == 0);
        static_assert(SAMPLER_BINDINGS_SET == 2);

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = std::size(pipelineDescriptorSetLayouts),
            .pSetLayouts = pipelineDescriptorSetLayouts,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::tessellate_vert.size_bytes(),
            .pCode = spirv::tessellate_vert.data(),
        };

        VkShaderModule vertexShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &vertexShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::tessellate_frag.size_bytes(),
            .pCode = spirv::tessellate_frag.data(),
        };

        VkShaderModule fragmentShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &fragmentShader));

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertexShader,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragmentShader,
                .pName = "main",
            },
        };

        VkVertexInputBindingDescription vertexInputBindingDescription = {
            .binding = 0,
            .stride = sizeof(gpu::TessVertexSpan),
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
        };

        VkVertexInputAttributeDescription vertexAttributeDescriptions[] = {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 0,
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 4 * sizeof(float),
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = 8 * sizeof(float),
            },
            {
                .location = 3,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32A32_UINT,
                .offset = 12 * sizeof(float),
            },
        };

        VkPipelineVertexInputStateCreateInfo
            pipelineVertexInputStateCreateInfo = {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertexInputBindingDescription,
                .vertexAttributeDescriptionCount = 4,
                .pVertexAttributeDescriptions = vertexAttributeDescriptions,
            };

        VkAttachmentDescription attachment = {
            .format = VK_FORMAT_R32G32B32A32_UINT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &layout::SINGLE_ATTACHMENT_SUBPASS,
        };

        VK_CHECK(m_vk->CreateRenderPass(m_vk->device,
                                        &renderPassCreateInfo,
                                        nullptr,
                                        &m_renderPass));

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &pipelineVertexInputStateCreateInfo,
            .pInputAssemblyState = &layout::INPUT_ASSEMBLY_TRIANGLE_LIST,
            .pViewportState = &layout::SINGLE_VIEWPORT,
            .pRasterizationState = &layout::RASTER_STATE_CULL_BACK_CCW,
            .pMultisampleState = &layout::MSAA_DISABLED,
            .pColorBlendState = &layout::SINGLE_ATTACHMENT_BLEND_DISABLED,
            .pDynamicState = &layout::DYNAMIC_VIEWPORT_SCISSOR,
            .layout = m_pipelineLayout,
            .renderPass = m_renderPass,
        };

        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &pipelineCreateInfo,
                                               nullptr,
                                               &m_renderPipeline));

        m_vk->DestroyShaderModule(m_vk->device, vertexShader, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, fragmentShader, nullptr);
    }

    ~TessellatePipeline()
    {
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
        m_vk->DestroyRenderPass(m_vk->device, m_renderPass, nullptr);
        m_vk->DestroyPipeline(m_vk->device, m_renderPipeline, nullptr);
    }

    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkPipeline renderPipeline() const { return m_renderPipeline; }

private:
    rcp<VulkanContext> m_vk;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_renderPipeline;
};

// Renders feathers to the atlas.
class RenderContextVulkanImpl::AtlasPipeline
{
public:
    AtlasPipeline(RenderContextVulkanImpl* impl) :
        m_vk(ref_rcp(impl->vulkanContext()))
    {
        VkDescriptorSetLayout pipelineDescriptorSetLayouts[] = {
            impl->m_perFlushDescriptorSetLayout,
            impl->m_emptyDescriptorSetLayout,
            impl->m_immutableSamplerDescriptorSetLayout,
        };
        static_assert(PER_FLUSH_BINDINGS_SET == 0);
        static_assert(SAMPLER_BINDINGS_SET == 2);

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = std::size(pipelineDescriptorSetLayouts),
            .pSetLayouts = pipelineDescriptorSetLayouts,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::render_atlas_vert.size_bytes(),
            .pCode = spirv::render_atlas_vert.data(),
        };

        VkShaderModule vertexShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &vertexShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::render_atlas_fill_frag.size_bytes(),
            .pCode = spirv::render_atlas_fill_frag.data(),
        };

        VkShaderModule fragmentFillShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &fragmentFillShader));

        shaderModuleCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv::render_atlas_stroke_frag.size_bytes(),
            .pCode = spirv::render_atlas_stroke_frag.data(),
        };

        VkShaderModule fragmentStrokeShader;
        VK_CHECK(m_vk->CreateShaderModule(m_vk->device,
                                          &shaderModuleCreateInfo,
                                          nullptr,
                                          &fragmentStrokeShader));

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertexShader,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = nullptr, // Set for individual fill/stroke pipelines.
                .pName = "main",
            },
        };

        VkAttachmentDescription attachment = {
            .format = VK_FORMAT_R32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkRenderPassCreateInfo renderPassCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &layout::SINGLE_ATTACHMENT_SUBPASS,
        };
        VK_CHECK(m_vk->CreateRenderPass(m_vk->device,
                                        &renderPassCreateInfo,
                                        nullptr,
                                        &m_renderPass));

        VkPipelineColorBlendAttachmentState blendState =
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT,
            };
        VkPipelineColorBlendStateCreateInfo blendStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1u,
            .pAttachments = &blendState,
        };

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = std::size(stages),
            .pStages = stages,
            .pVertexInputState = &layout::PATH_VERTEX_INPUT_STATE,
            .pInputAssemblyState = &layout::INPUT_ASSEMBLY_TRIANGLE_LIST,
            .pViewportState = &layout::SINGLE_VIEWPORT,
            .pRasterizationState = &layout::RASTER_STATE_CULL_BACK_CW,
            .pMultisampleState = &layout::MSAA_DISABLED,
            .pColorBlendState = &blendStateCreateInfo,
            .pDynamicState = &layout::DYNAMIC_VIEWPORT_SCISSOR,
            .layout = m_pipelineLayout,
            .renderPass = m_renderPass,
        };

        stages[1].module = fragmentFillShader;
        blendState.colorBlendOp = VK_BLEND_OP_ADD;
        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &pipelineCreateInfo,
                                               nullptr,
                                               &m_fillPipeline));

        stages[1].module = fragmentStrokeShader;
        blendState.colorBlendOp = VK_BLEND_OP_MAX;
        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &pipelineCreateInfo,
                                               nullptr,
                                               &m_strokePipeline));

        m_vk->DestroyShaderModule(m_vk->device, vertexShader, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, fragmentFillShader, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, fragmentStrokeShader, nullptr);
    }

    ~AtlasPipeline()
    {
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
        m_vk->DestroyRenderPass(m_vk->device, m_renderPass, nullptr);
        m_vk->DestroyPipeline(m_vk->device, m_fillPipeline, nullptr);
        m_vk->DestroyPipeline(m_vk->device, m_strokePipeline, nullptr);
    }

    VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkPipeline fillPipeline() const { return m_fillPipeline; }
    VkPipeline strokePipeline() const { return m_strokePipeline; }

private:
    rcp<VulkanContext> m_vk;
    VkPipelineLayout m_pipelineLayout;
    VkRenderPass m_renderPass;
    VkPipeline m_fillPipeline;
    VkPipeline m_strokePipeline;
};

enum class DrawPipelineLayoutOptions
{
    none = 0,
    fixedFunctionColorOutput = 1 << 0, // COLOR is not an input attachment.
};
RIVE_MAKE_ENUM_BITSET(DrawPipelineLayoutOptions);

class RenderContextVulkanImpl::DrawPipelineLayout
{
public:
    static_assert(kDrawPipelineLayoutOptionCount == 1);

    // Number of render pass variants that can be used with a single
    // DrawPipeline (framebufferFormat x loadOp).
    constexpr static int kRenderPassVariantCount = 6;

    static int RenderPassVariantIdx(VkFormat framebufferFormat,
                                    gpu::LoadAction loadAction)
    {
        int loadActionIdx = static_cast<int>(loadAction);
        assert(0 <= loadActionIdx && loadActionIdx < 3);
        assert(framebufferFormat == VK_FORMAT_B8G8R8A8_UNORM ||
               framebufferFormat == VK_FORMAT_R8G8B8A8_UNORM);
        int idx = (loadActionIdx << 1) |
                  (framebufferFormat == VK_FORMAT_B8G8R8A8_UNORM ? 1 : 0);
        assert(0 <= idx && idx < kRenderPassVariantCount);
        return idx;
    }

    constexpr static VkFormat FormatFromRenderPassVariant(int idx)
    {
        return (idx & 1) ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
    }

    constexpr static VkAttachmentLoadOp LoadOpFromRenderPassVariant(int idx)
    {
        auto loadAction = static_cast<gpu::LoadAction>(idx >> 1);
        switch (loadAction)
        {
            case gpu::LoadAction::preserveRenderTarget:
                return VK_ATTACHMENT_LOAD_OP_LOAD;
            case gpu::LoadAction::clear:
                return VK_ATTACHMENT_LOAD_OP_CLEAR;
            case gpu::LoadAction::dontCare:
                return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
        RIVE_UNREACHABLE();
    }

    DrawPipelineLayout(RenderContextVulkanImpl* impl,
                       gpu::InterlockMode interlockMode,
                       DrawPipelineLayoutOptions options) :
        m_vk(ref_rcp(impl->vulkanContext())),
        m_interlockMode(interlockMode),
        m_options(options)
    {
        assert(interlockMode != gpu::InterlockMode::msaa); // TODO: msaa.

        if (interlockMode == gpu::InterlockMode::rasterOrdering ||
            interlockMode == gpu::InterlockMode::atomics)
        {
            // PLS planes get bound per flush as input attachments or storage
            // textures.
            VkDescriptorSetLayoutBinding plsLayoutBindings[] = {
                {
                    .binding = COLOR_PLANE_IDX,
                    .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    .binding = CLIP_PLANE_IDX,
                    .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    .binding = SCRATCH_COLOR_PLANE_IDX,
                    .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                    .binding = COVERAGE_PLANE_IDX,
                    .descriptorType =
                        m_interlockMode == gpu::InterlockMode::atomics
                            ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                            : VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
            };
            static_assert(COLOR_PLANE_IDX == 0);
            static_assert(CLIP_PLANE_IDX == 1);
            static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
            static_assert(COVERAGE_PLANE_IDX == 3);

            VkDescriptorSetLayoutCreateInfo plsLayoutInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            };
            if (m_options & DrawPipelineLayoutOptions::fixedFunctionColorOutput)
            {
                // Drop the COLOR input attachment when using
                // fixedFunctionColorOutput.
                assert(plsLayoutBindings[0].binding == COLOR_PLANE_IDX);
                plsLayoutInfo.bindingCount = std::size(plsLayoutBindings) - 1;
                plsLayoutInfo.pBindings = plsLayoutBindings + 1;
            }
            else
            {
                plsLayoutInfo.bindingCount = std::size(plsLayoutBindings);
                plsLayoutInfo.pBindings = plsLayoutBindings;
            }

            VK_CHECK(m_vk->CreateDescriptorSetLayout(
                m_vk->device,
                &plsLayoutInfo,
                nullptr,
                &m_plsTextureDescriptorSetLayout));
        }
        else
        {
            // clockwiseAtomic and msaa modes don't use pixel local storage.
            m_plsTextureDescriptorSetLayout = VK_NULL_HANDLE;
        }

        VkDescriptorSetLayout pipelineDescriptorSetLayouts[] = {
            impl->m_perFlushDescriptorSetLayout,
            impl->m_perDrawDescriptorSetLayout,
            impl->m_immutableSamplerDescriptorSetLayout,
            m_plsTextureDescriptorSetLayout,
        };
        static_assert(COLOR_PLANE_IDX == 0);
        static_assert(CLIP_PLANE_IDX == 1);
        static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
        static_assert(COVERAGE_PLANE_IDX == 3);
        static_assert(BINDINGS_SET_COUNT == 4);

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = m_plsTextureDescriptorSetLayout != nullptr
                                  ? BINDINGS_SET_COUNT
                                  : BINDINGS_SET_COUNT - 1u,
            .pSetLayouts = pipelineDescriptorSetLayouts,
        };

        VK_CHECK(m_vk->CreatePipelineLayout(m_vk->device,
                                            &pipelineLayoutCreateInfo,
                                            nullptr,
                                            &m_pipelineLayout));
    }

    VkRenderPass renderPassAt(int renderPassVariantIdx)
    {
        if (m_renderPasses[renderPassVariantIdx] == VK_NULL_HANDLE)
        {
            // Create the render pass.
            VkAttachmentLoadOp colorLoadOp =
                LoadOpFromRenderPassVariant(renderPassVariantIdx);
            VkFormat renderTargetFormat =
                FormatFromRenderPassVariant(renderPassVariantIdx);

            StackVector<VkAttachmentDescription,
                        PIXEL_LOCAL_STORAGE_PLANE_COUNT>
                attachmentDescs;
            StackVector<VkAttachmentReference, PIXEL_LOCAL_STORAGE_PLANE_COUNT>
                attachmentRefs;

            attachmentDescs.push_back({
                .format = renderTargetFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = colorLoadOp,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = colorLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD
                                     ? VK_IMAGE_LAYOUT_GENERAL
                                     : VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
            });
            attachmentRefs.push_back({
                .attachment = COLOR_PLANE_IDX,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            });

            if (m_interlockMode == gpu::InterlockMode::rasterOrdering ||
                m_interlockMode == gpu::InterlockMode::atomics)
            {
                attachmentDescs.push_back({
                    // The clip buffer is RGBA8 in atomic mode.
                    .format = m_interlockMode == gpu::InterlockMode::atomics
                                  ? renderTargetFormat
                                  : VK_FORMAT_R32_UINT,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
                });
                attachmentRefs.push_back({
                    .attachment = CLIP_PLANE_IDX,
                    .layout = VK_IMAGE_LAYOUT_GENERAL,
                });
            }

            if (m_interlockMode == gpu::InterlockMode::rasterOrdering)
            {
                attachmentDescs.push_back({
                    .format = renderTargetFormat,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
                });
                attachmentRefs.push_back({
                    .attachment = SCRATCH_COLOR_PLANE_IDX,
                    .layout = VK_IMAGE_LAYOUT_GENERAL,
                });
                attachmentDescs.push_back({
                    .format = VK_FORMAT_R32_UINT,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
                });
                attachmentRefs.push_back({
                    .attachment = COVERAGE_PLANE_IDX,
                    .layout = VK_IMAGE_LAYOUT_GENERAL,
                });
            }

            const bool usesDepth = m_interlockMode == gpu::InterlockMode::msaa;
            if (usesDepth)
            {
                attachmentDescs.push_back({
                    .format = get_preferred_depth_stencil_format(
                        m_vk->supportsD24S8()),
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    // Clear on render pass start, and don't need to store on
                    // render pass end
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout =
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                });
            }

            VkAttachmentReference depthAttachmentRef = {};
            depthAttachmentRef.attachment = attachmentDescs.size() - 1;
            depthAttachmentRef.layout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            // Input attachments can differ from the proper color attachments
            StackVector<VkAttachmentReference, PIXEL_LOCAL_STORAGE_PLANE_COUNT>
                inputAttachmentRefs;
            inputAttachmentRefs.push_back_n(attachmentRefs.size(),
                                            attachmentRefs.data());

            // COLOR is not an input attachment if we're using fixed
            // function blending.
            if (m_options & DrawPipelineLayoutOptions::fixedFunctionColorOutput)
            {
                inputAttachmentRefs[0] =
                    VkAttachmentReference{.attachment = VK_ATTACHMENT_UNUSED};
            }

            uint32_t numInputAttachments = inputAttachmentRefs.size();
            if (m_interlockMode == gpu::InterlockMode::clockwiseAtomic)
            {
                numInputAttachments = 0;
            }
            const uint32_t numColorAttachments = attachmentRefs.size();

            // Subpasses

            constexpr uint32_t MAX_SUBPASSES_POSSIBLE = 2;
            StackVector<VkSubpassDescription, MAX_SUBPASSES_POSSIBLE>
                subpassDescriptions;

            // Without EXT_rasterization_order_attachment_access we need to
            // declare explicit subpass dependencies.
            StackVector<VkSubpassDependency, MAX_SUBPASSES_POSSIBLE>
                subpassDeps;
            const bool implicitSubpassDependencies =
                m_interlockMode == gpu::InterlockMode::rasterOrdering &&
                m_vk->features.rasterizationOrderColorAttachmentAccess;

            // Draw subpass
            subpassDescriptions.push_back({
                .flags =
                    implicitSubpassDependencies
                        ? VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT
                        : 0u,
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = numInputAttachments,
                .pInputAttachments = inputAttachmentRefs.data(),
                .colorAttachmentCount = numColorAttachments,
                .pColorAttachments = attachmentRefs.data(),
                .pDepthStencilAttachment =
                    usesDepth ? &depthAttachmentRef : nullptr,
            });
            // Draw subpass self-dependencies.
            subpassDeps.push_back(
                    m_interlockMode != gpu::InterlockMode::clockwiseAtomic
                    // Normally our dependencies are input attachments.
                    // TODO: Do we need shader SHADER_READ/SHADER_WRITE as well,
                    // for the coverage texture?
                    ? VkSubpassDependency {
                        .srcSubpass = 0,
                        .dstSubpass = 0,
                        .srcStageMask =
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                    }
                // clockwiseAtomic mode only has dependencies in the
                // coverage buffer (for now).
                : VkSubpassDependency {
                    .srcSubpass = 0,
                    .dstSubpass = 0,
                    .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                });

            if (m_interlockMode == gpu::InterlockMode::atomics)
            {
                // Atomic resolve subpass.
                subpassDescriptions.push_back({
                    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                    .inputAttachmentCount = numColorAttachments,
                    .pInputAttachments = inputAttachmentRefs.data(),
                    .colorAttachmentCount =
                        1u, // The resolve only writes color.
                    .pColorAttachments = attachmentRefs.data(),
                });
                subpassDeps.push_back({
                    // Atomic resolve dependencies (InterlockMode::atomics
                    // only).
                    .srcSubpass = 0,
                    .dstSubpass = 1,
                    .srcStageMask =
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
                });
            }

            VkRenderPassCreateInfo renderPassCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = attachmentDescs.size(),
                .pAttachments = attachmentDescs.data(),
                .subpassCount = subpassDescriptions.size(),
                .pSubpasses = subpassDescriptions.data(),
                .dependencyCount = implicitSubpassDependencies
                                       ? 0
                                       : renderPassCreateInfo.subpassCount,
                .pDependencies =
                    implicitSubpassDependencies ? nullptr : subpassDeps.data(),
            };

            VK_CHECK(
                m_vk->CreateRenderPass(m_vk->device,
                                       &renderPassCreateInfo,
                                       nullptr,
                                       &m_renderPasses[renderPassVariantIdx]));
        }
        return m_renderPasses[renderPassVariantIdx];
    }

    ~DrawPipelineLayout()
    {
        m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                         m_plsTextureDescriptorSetLayout,
                                         nullptr);
        m_vk->DestroyPipelineLayout(m_vk->device, m_pipelineLayout, nullptr);
        for (VkRenderPass renderPass : m_renderPasses)
        {
            m_vk->DestroyRenderPass(m_vk->device, renderPass, nullptr);
        }
    }

    gpu::InterlockMode interlockMode() const { return m_interlockMode; }
    DrawPipelineLayoutOptions options() const { return m_options; }

    uint32_t attachmentCount() const
    {
        switch (m_interlockMode)
        {
            case gpu::InterlockMode::rasterOrdering:
                return 4;
            case gpu::InterlockMode::atomics:
                return 2;
            case gpu::InterlockMode::clockwiseAtomic:
                return 1;
            case gpu::InterlockMode::msaa:
                return 2;
        }
    }

    VkDescriptorSetLayout plsLayout() const
    {
        return m_plsTextureDescriptorSetLayout;
    }

    VkPipelineLayout operator*() const { return m_pipelineLayout; }
    VkPipelineLayout vkPipelineLayout() const { return m_pipelineLayout; }

private:
    const rcp<VulkanContext> m_vk;
    const gpu::InterlockMode m_interlockMode;
    const DrawPipelineLayoutOptions m_options;

    VkDescriptorSetLayout m_plsTextureDescriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    std::array<VkRenderPass, kRenderPassVariantCount> m_renderPasses = {};
};

// Wraps vertex and fragment shader modules for a specific combination of
// DrawType, InterlockMode, and ShaderFeatures.
class RenderContextVulkanImpl::DrawShader
{
public:
    DrawShader(VulkanContext* vk,
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
                    vkutil::set_shader_code(
                        vsInfo,
                        spirv::draw_interior_triangles_vert);
                    vkutil::set_shader_code(
                        fsInfo,
                        spirv::draw_interior_triangles_frag);
                    break;

                case DrawType::atlasBlit:
                    vkutil::set_shader_code(vsInfo,
                                            spirv::draw_atlas_blit_vert);
                    vkutil::set_shader_code(fsInfo,
                                            spirv::draw_atlas_blit_frag);
                    break;

                case DrawType::imageMesh:
                    vkutil::set_shader_code(vsInfo,
                                            spirv::draw_image_mesh_vert);
                    vkutil::set_shader_code(fsInfo,
                                            spirv::draw_image_mesh_frag);
                    break;

                case DrawType::imageRect:
                case DrawType::atomicResolve:
                case DrawType::atomicInitialize:
                case DrawType::stencilClipReset:
                    RIVE_UNREACHABLE();
            }
        }
        else if (interlockMode == gpu::InterlockMode::atomics)
        {
            assert(interlockMode == gpu::InterlockMode::atomics);
            bool fixedFunctionColorOutput =
                shaderMiscFlags &
                gpu::ShaderMiscFlags::fixedFunctionColorOutput;
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::midpointFanCenterAAPatches:
                case DrawType::outerCurvePatches:
                    vkutil::set_shader_code(vsInfo,
                                            spirv::atomic_draw_path_vert);
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
                    vkutil::set_shader_code(vsInfo,
                                            spirv::atomic_resolve_pls_vert);
                    vkutil::set_shader_code_if_then_else(
                        fsInfo,
                        fixedFunctionColorOutput,
                        spirv::atomic_resolve_pls_fixedcolor_frag,
                        spirv::atomic_resolve_pls_frag);
                    break;

                case DrawType::atomicInitialize:
                case DrawType::stencilClipReset:
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
                    vkutil::set_shader_code(
                        vsInfo,
                        spirv::draw_clockwise_atlas_blit_vert);
                    vkutil::set_shader_code(
                        fsInfo,
                        spirv::draw_clockwise_atlas_blit_frag);
                    break;

                case DrawType::imageMesh:
                    vkutil::set_shader_code(
                        vsInfo,
                        spirv::draw_clockwise_image_mesh_vert);
                    vkutil::set_shader_code(
                        fsInfo,
                        spirv::draw_clockwise_image_mesh_frag);
                    break;

                case DrawType::imageRect:
                case DrawType::atomicResolve:
                case DrawType::atomicInitialize:
                case DrawType::stencilClipReset:
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

    ~DrawShader()
    {
        m_vk->DestroyShaderModule(m_vk->device, m_vertexModule, nullptr);
        m_vk->DestroyShaderModule(m_vk->device, m_fragmentModule, nullptr);
    }

    VkShaderModule vertexModule() const { return m_vertexModule; }
    VkShaderModule fragmentModule() const { return m_fragmentModule; }

private:
    const rcp<VulkanContext> m_vk;
    VkShaderModule m_vertexModule = nullptr;
    VkShaderModule m_fragmentModule = nullptr;
};

// Pipeline options that don't affect the shader.
enum class DrawPipelineOptions
{
    none = 0,
    wireframe = 1 << 0,
};
constexpr static int kDrawPipelineOptionCount = 1;
RIVE_MAKE_ENUM_BITSET(DrawPipelineOptions);

class RenderContextVulkanImpl::DrawPipeline
{
public:
    DrawPipeline(RenderContextVulkanImpl* impl,
                 gpu::DrawType drawType,
                 const DrawPipelineLayout& pipelineLayout,
                 gpu::ShaderFeatures shaderFeatures,
                 gpu::ShaderMiscFlags shaderMiscFlags,
                 DrawPipelineOptions drawPipelineOptions,
                 VkRenderPass vkRenderPass) :
        m_vk(ref_rcp(impl->vulkanContext()))
    {
        gpu::InterlockMode interlockMode = pipelineLayout.interlockMode();
        uint32_t shaderKey = gpu::ShaderUniqueKey(drawType,
                                                  shaderFeatures,
                                                  interlockMode,
                                                  shaderMiscFlags);
        const DrawShader& drawShader = impl->m_drawShaders
                                           .try_emplace(shaderKey,
                                                        m_vk.get(),
                                                        drawType,
                                                        interlockMode,
                                                        shaderFeatures,
                                                        shaderMiscFlags)
                                           .first->second;

        VkBool32 shaderPermutationFlags[SPECIALIZATION_COUNT] = {
            shaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_CLIP_RECT,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_FEATHER,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_EVEN_ODD,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_NESTED_CLIPPING,
            shaderFeatures & gpu::ShaderFeatures::ENABLE_HSL_BLEND_MODES,
            shaderMiscFlags & gpu::ShaderMiscFlags::clockwiseFill,
            shaderMiscFlags & gpu::ShaderMiscFlags::borrowedCoveragePrepass,
        };
        static_assert(CLIPPING_SPECIALIZATION_IDX == 0);
        static_assert(CLIP_RECT_SPECIALIZATION_IDX == 1);
        static_assert(ADVANCED_BLEND_SPECIALIZATION_IDX == 2);
        static_assert(FEATHER_SPECIALIZATION_IDX == 3);
        static_assert(EVEN_ODD_SPECIALIZATION_IDX == 4);
        static_assert(NESTED_CLIPPING_SPECIALIZATION_IDX == 5);
        static_assert(HSL_BLEND_MODES_SPECIALIZATION_IDX == 6);
        static_assert(CLOCKWISE_FILL_SPECIALIZATION_IDX == 7);
        static_assert(BORROWED_COVERAGE_PREPASS_SPECIALIZATION_IDX == 8);
        static_assert(SPECIALIZATION_COUNT == 9);

        VkSpecializationMapEntry permutationMapEntries[SPECIALIZATION_COUNT];
        for (uint32_t i = 0; i < SPECIALIZATION_COUNT; ++i)
        {
            permutationMapEntries[i] = {
                .constantID = i,
                .offset = i * static_cast<uint32_t>(sizeof(VkBool32)),
                .size = sizeof(VkBool32),
            };
        }

        VkSpecializationInfo specializationInfo = {
            .mapEntryCount = SPECIALIZATION_COUNT,
            .pMapEntries = permutationMapEntries,
            .dataSize = sizeof(shaderPermutationFlags),
            .pData = &shaderPermutationFlags,
        };

        VkPipelineShaderStageCreateInfo stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = drawShader.vertexModule(),
                .pName = "main",
                .pSpecializationInfo = &specializationInfo,
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = drawShader.fragmentModule(),
                .pName = "main",
                .pSpecializationInfo = &specializationInfo,
            },
        };

        VkPipelineRasterizationStateCreateInfo
            pipelineRasterizationStateCreateInfo = {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode =
                    (drawPipelineOptions & DrawPipelineOptions::wireframe)
                        ? VK_POLYGON_MODE_LINE
                        : VK_POLYGON_MODE_FILL,
                .cullMode = static_cast<VkCullModeFlags>(
                    DrawTypeIsImageDraw(drawType) ? VK_CULL_MODE_NONE
                                                  : VK_CULL_MODE_BACK_BIT),
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .lineWidth = 1.0,
            };

        VkPipelineColorBlendAttachmentState colorBlendState =
            interlockMode == gpu::InterlockMode::rasterOrdering
                // rasterOrdering mode does all the blending in shaders.
                ? VkPipelineColorBlendAttachmentState{
                    .blendEnable = VK_FALSE,
                    .colorWriteMask = vkutil::kColorWriteMaskRGBA,
                }
            : shaderMiscFlags & gpu::ShaderMiscFlags::borrowedCoveragePrepass
                // Borrowed coverage draws only update the coverage buffer.
                ? VkPipelineColorBlendAttachmentState{
                    .blendEnable = VK_FALSE,
                    .colorWriteMask = vkutil::kColorWriteMaskNone,
                }
                // Atomic mode blends the color and clip both as independent RGBA8
                // values.
                //
                // Advanced blend modes are handled by rearranging the math
                // such that the correct color isn't reached until *AFTER* this
                // blend state is applied.
                //
                // This allows us to preserve clip and color contents by just
                // emitting a=0 instead of loading the current value. It also saves
                // flops by offloading the blending work onto the ROP blending
                // unit, and serves as a hint to the hardware that it doesn't need
                // to read or write anything when a == 0.
                : VkPipelineColorBlendAttachmentState{
                    .blendEnable = VK_TRUE,
                    // Image draws use premultiplied src-over so they can blend the
                    // image with the previous paint together, out of order.
                    // (Premultiplied src-over is associative.)
                    //
                    // Otherwise we save 3 flops and let the ROP multiply alpha
                    // into the color when it does the blending.
                    .srcColorBlendFactor =
                        interlockMode == gpu::InterlockMode::atomics &&
                        gpu::DrawTypeIsImageDraw(drawType)
                            ? VK_BLEND_FACTOR_ONE
                            : VK_BLEND_FACTOR_SRC_ALPHA,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                    .colorWriteMask = vkutil::kColorWriteMaskRGBA,
                };

        VkPipelineColorBlendAttachmentState blendColorAttachments[] = {
            {colorBlendState},
            {colorBlendState},
            {colorBlendState},
            {colorBlendState},
        };

        VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo =
            {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = drawType == gpu::DrawType::atomicResolve
                                       ? 1u
                                       : pipelineLayout.attachmentCount(),
                .pAttachments = blendColorAttachments,
            };

        if (interlockMode == gpu::InterlockMode::rasterOrdering &&
            m_vk->features.rasterizationOrderColorAttachmentAccess)
        {
            pipelineColorBlendStateCreateInfo.flags |=
                VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT;
        }

        VkPipelineDepthStencilStateCreateInfo depthStencilState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .minDepthBounds = DEPTH_MIN,
            .maxDepthBounds = DEPTH_MAX,
        };

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pViewportState = &layout::SINGLE_VIEWPORT,
            .pRasterizationState = &pipelineRasterizationStateCreateInfo,
            .pMultisampleState = &layout::MSAA_DISABLED,
            .pDepthStencilState = interlockMode == gpu::InterlockMode::msaa
                                      ? &depthStencilState
                                      : nullptr,
            .pColorBlendState = &pipelineColorBlendStateCreateInfo,
            .pDynamicState = &layout::DYNAMIC_VIEWPORT_SCISSOR,
            .layout = *pipelineLayout,
            .renderPass = vkRenderPass,
            .subpass = drawType == gpu::DrawType::atomicResolve ? 1u : 0u,
        };

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
                pipelineCreateInfo.pVertexInputState =
                    &layout::PATH_VERTEX_INPUT_STATE;
                pipelineCreateInfo.pInputAssemblyState =
                    &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
                break;

            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
                pipelineCreateInfo.pVertexInputState =
                    &layout::INTERIOR_TRI_VERTEX_INPUT_STATE;
                pipelineCreateInfo.pInputAssemblyState =
                    &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
                break;

            case DrawType::imageRect:
                pipelineCreateInfo.pVertexInputState =
                    &layout::IMAGE_RECT_VERTEX_INPUT_STATE;
                pipelineCreateInfo.pInputAssemblyState =
                    &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
                break;

            case DrawType::imageMesh:
                pipelineCreateInfo.pVertexInputState =
                    &layout::IMAGE_MESH_VERTEX_INPUT_STATE;
                pipelineCreateInfo.pInputAssemblyState =
                    &layout::INPUT_ASSEMBLY_TRIANGLE_LIST;
                break;

            case DrawType::atomicResolve:
                pipelineCreateInfo.pVertexInputState =
                    &layout::EMPTY_VERTEX_INPUT_STATE;
                pipelineCreateInfo.pInputAssemblyState =
                    &layout::INPUT_ASSEMBLY_TRIANGLE_STRIP;
                break;

            case DrawType::atomicInitialize:
            case DrawType::stencilClipReset:
                RIVE_UNREACHABLE();
        }

        VK_CHECK(m_vk->CreateGraphicsPipelines(m_vk->device,
                                               VK_NULL_HANDLE,
                                               1,
                                               &pipelineCreateInfo,
                                               nullptr,
                                               &m_vkPipeline));
    }

    ~DrawPipeline()
    {
        m_vk->DestroyPipeline(m_vk->device, m_vkPipeline, nullptr);
    }

    const VkPipeline vkPipeline() const { return m_vkPipeline; }

private:
    const rcp<VulkanContext> m_vk;
    VkPipeline m_vkPipeline;
};

RenderContextVulkanImpl::RenderContextVulkanImpl(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const VulkanFeatures& features,
    PFN_vkGetInstanceProcAddr fp_vkGetInstanceProcAddr,
    PFN_vkGetDeviceProcAddr fp_vkGetDeviceProcAddr) :
    m_vk(make_rcp<VulkanContext>(instance,
                                 physicalDevice,
                                 device,
                                 features,
                                 fp_vkGetInstanceProcAddr,
                                 fp_vkGetDeviceProcAddr)),
    m_flushUniformBufferRing(m_vk,
                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             vkutil::Mappability::writeOnly),
    m_imageDrawUniformBufferRing(m_vk,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 vkutil::Mappability::writeOnly),
    m_pathBufferRing(m_vk,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     vkutil::Mappability::writeOnly),
    m_paintBufferRing(m_vk,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      vkutil::Mappability::writeOnly),
    m_paintAuxBufferRing(m_vk,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         vkutil::Mappability::writeOnly),
    m_contourBufferRing(m_vk,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        vkutil::Mappability::writeOnly),
    m_gradSpanBufferRing(m_vk,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vkutil::Mappability::writeOnly),
    m_tessSpanBufferRing(m_vk,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vkutil::Mappability::writeOnly),
    m_triangleBufferRing(m_vk,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vkutil::Mappability::writeOnly),
    m_descriptorSetPoolPool(
        make_rcp<vkutil::ResourcePool<DescriptorSetPool>>(m_vk))
{
    m_platformFeatures.supportsRasterOrdering =
        features.rasterizationOrderColorAttachmentAccess;
    m_platformFeatures.supportsFragmentShaderAtomics =
        features.fragmentStoresAndAtomics;
    m_platformFeatures.supportsClockwiseAtomicRendering =
        features.fragmentStoresAndAtomics;
    m_platformFeatures.clipSpaceBottomUp = false;
    m_platformFeatures.framebufferBottomUp = false;
    m_platformFeatures.maxCoverageBufferLength =
        std::min(features.maxStorageBufferRange, 1u << 28) / sizeof(uint32_t);

    if (features.vendorID == vkutil::kVendorQualcomm)
    {
        // Qualcomm advertises EXT_rasterization_order_attachment_access, but
        // it's slow. Use atomics instead on this platform.
        m_platformFeatures.supportsRasterOrdering = false;
        // Pixel4 struggles with fine-grained fp16 path IDs.
        m_platformFeatures.pathIDGranularity = 2;
    }
    else if (features.vendorID == vkutil::kVendorARM)
    {
        // This is undocumented, but raster ordering always works on ARM Mali
        // GPUs if you define a subpass dependency, even without
        // EXT_rasterization_order_attachment_access.
        m_platformFeatures.supportsRasterOrdering = true;
    }
}

void RenderContextVulkanImpl::initGPUObjects()
{
    // Create the immutable samplers.
    VkSamplerCreateInfo linearSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minLod = 0,
        .maxLod = 0,
    };

    VK_CHECK(m_vk->CreateSampler(m_vk->device,
                                 &linearSamplerCreateInfo,
                                 nullptr,
                                 &m_linearSampler));

    VkSamplerCreateInfo mipmapSamplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .minLod = 0,
        .maxLod = VK_LOD_CLAMP_NONE,
    };

    VK_CHECK(m_vk->CreateSampler(m_vk->device,
                                 &mipmapSamplerCreateInfo,
                                 nullptr,
                                 &m_mipmapSampler));

    // Bound when there is not an image paint.
    constexpr static uint8_t black[] = {0, 0, 0, 1};
    m_nullImageTexture =
        make_rcp<TextureVulkanImpl>(m_vk, 1, 1, 1, TextureFormat::rgba8, black);

    // All pipelines share the same perFlush bindings.
    VkDescriptorSetLayoutBinding perFlushLayoutBindings[] = {
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = PATH_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = PAINT_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = PAINT_AUX_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = CONTOUR_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = COVERAGE_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = TESS_VERTEX_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = GRAD_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = FEATHER_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = ATLAS_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo perFlushLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(perFlushLayoutBindings),
        .pBindings = perFlushLayoutBindings,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                             &perFlushLayoutInfo,
                                             nullptr,
                                             &m_perFlushDescriptorSetLayout));

    // The imageTexture gets updated with every draw that uses it.
    VkDescriptorSetLayoutBinding perDrawLayoutBindings[] = {
        {
            .binding = IMAGE_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo perDrawLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(perDrawLayoutBindings),
        .pBindings = perDrawLayoutBindings,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                             &perDrawLayoutInfo,
                                             nullptr,
                                             &m_perDrawDescriptorSetLayout));

    // Every shader uses the same immutable sampler layout.
    VkDescriptorSetLayoutBinding immutableSamplerLayoutBindings[] = {
        {
            .binding = GRAD_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &m_linearSampler,
        },
        {
            .binding = FEATHER_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags =
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &m_linearSampler,
        },
        {
            .binding = ATLAS_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &m_linearSampler,
        },
        {
            .binding = IMAGE_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &m_mipmapSampler,
        },
    };

    VkDescriptorSetLayoutCreateInfo immutableSamplerLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = std::size(immutableSamplerLayoutBindings),
        .pBindings = immutableSamplerLayoutBindings,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(
        m_vk->device,
        &immutableSamplerLayoutInfo,
        nullptr,
        &m_immutableSamplerDescriptorSetLayout));

    // For when a set isn't used at all by a shader.
    VkDescriptorSetLayoutCreateInfo emptyLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 0,
    };

    VK_CHECK(m_vk->CreateDescriptorSetLayout(m_vk->device,
                                             &emptyLayoutInfo,
                                             nullptr,
                                             &m_emptyDescriptorSetLayout));

    // Create static descriptor sets.
    VkDescriptorPoolSize staticDescriptorPoolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1, // m_nullImageTexture
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 4, // grad, feather, atlas, image samplers
        },
    };

    VkDescriptorPoolCreateInfo staticDescriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 2,
        .poolSizeCount = std::size(staticDescriptorPoolSizes),
        .pPoolSizes = staticDescriptorPoolSizes,
    };

    VK_CHECK(m_vk->CreateDescriptorPool(m_vk->device,
                                        &staticDescriptorPoolCreateInfo,
                                        nullptr,
                                        &m_staticDescriptorPool));

    // Create a descriptor set to bind m_nullImageTexture when there is no image
    // paint.
    VkDescriptorSetAllocateInfo nullImageDescriptorSetInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_staticDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_perDrawDescriptorSetLayout,
    };

    VK_CHECK(m_vk->AllocateDescriptorSets(m_vk->device,
                                          &nullImageDescriptorSetInfo,
                                          &m_nullImageDescriptorSet));

    m_vk->updateImageDescriptorSets(
        m_nullImageDescriptorSet,
        {
            .dstBinding = IMAGE_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = *m_nullImageTexture->m_textureView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    // Create an empty descriptor set for SAMPLER_BINDINGS_SET. Vulkan requires
    // this even though the samplers are all immutable.
    VkDescriptorSetAllocateInfo immutableSamplerDescriptorSetInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_staticDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_immutableSamplerDescriptorSetLayout,
    };

    VK_CHECK(m_vk->AllocateDescriptorSets(m_vk->device,
                                          &immutableSamplerDescriptorSetInfo,
                                          &m_immutableSamplerDescriptorSet));

    // The pipelines reference our vulkan objects. Delete them first.
    m_colorRampPipeline = std::make_unique<ColorRampPipeline>(this);
    m_tessellatePipeline = std::make_unique<TessellatePipeline>(this);
    m_atlasPipeline = std::make_unique<AtlasPipeline>(this);

    // Emulate the feather texture1d array as a 2d texture until we add
    // texture1d support in Vulkan.
    uint16_t featherTextureData[gpu::GAUSSIAN_TABLE_SIZE *
                                FEATHER_TEXTURE_1D_ARRAY_LENGTH];
    memcpy(featherTextureData,
           gpu::g_gaussianIntegralTableF16,
           sizeof(gpu::g_gaussianIntegralTableF16));
    memcpy(featherTextureData + gpu::GAUSSIAN_TABLE_SIZE,
           gpu::g_inverseGaussianIntegralTableF16,
           sizeof(gpu::g_inverseGaussianIntegralTableF16));
    static_assert(FEATHER_FUNCTION_ARRAY_INDEX == 0);
    static_assert(FEATHER_INVERSE_FUNCTION_ARRAY_INDEX == 1);
    m_featherTexture =
        make_rcp<TextureVulkanImpl>(m_vk,
                                    gpu::GAUSSIAN_TABLE_SIZE,
                                    FEATHER_TEXTURE_1D_ARRAY_LENGTH,
                                    1,
                                    TextureFormat::r16f,
                                    featherTextureData);

    m_tessSpanIndexBuffer = m_vk->makeBuffer(
        {
            .size = sizeof(gpu::kTessSpanIndices),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(m_tessSpanIndexBuffer->contents(),
           gpu::kTessSpanIndices,
           sizeof(gpu::kTessSpanIndices));
    m_tessSpanIndexBuffer->flushContents();

    m_pathPatchVertexBuffer = m_vk->makeBuffer(
        {
            .size = kPatchVertexBufferCount * sizeof(gpu::PatchVertex),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    m_pathPatchIndexBuffer = m_vk->makeBuffer(
        {
            .size = kPatchIndexBufferCount * sizeof(uint16_t),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    gpu::GeneratePatchBufferData(
        reinterpret_cast<PatchVertex*>(m_pathPatchVertexBuffer->contents()),
        reinterpret_cast<uint16_t*>(m_pathPatchIndexBuffer->contents()));
    m_pathPatchVertexBuffer->flushContents();
    m_pathPatchIndexBuffer->flushContents();

    m_imageRectVertexBuffer = m_vk->makeBuffer(
        {
            .size = sizeof(gpu::kImageRectVertices),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(m_imageRectVertexBuffer->contents(),
           gpu::kImageRectVertices,
           sizeof(gpu::kImageRectVertices));
    m_imageRectVertexBuffer->flushContents();
    m_imageRectIndexBuffer = m_vk->makeBuffer(
        {
            .size = sizeof(gpu::kImageRectIndices),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);
    memcpy(m_imageRectIndexBuffer->contents(),
           gpu::kImageRectIndices,
           sizeof(gpu::kImageRectIndices));
    m_imageRectIndexBuffer->flushContents();
}

RenderContextVulkanImpl::~RenderContextVulkanImpl()
{
    // Wait for all fences before cleaning up.
    for (const rcp<gpu::CommandBufferCompletionFence>& fence :
         m_frameCompletionFences)
    {
        if (fence != nullptr)
        {
            fence->wait();
        }
    }

    // Tell the context we are entering our shutdown cycle. After this point,
    // all resources will be deleted immediately upon their refCount reaching
    // zero, as opposed to being kept alive for in-flight command buffers.
    m_vk->shutdown();

    m_vk->DestroyDescriptorPool(m_vk->device, m_staticDescriptorPool, nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_perFlushDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_perDrawDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_immutableSamplerDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroyDescriptorSetLayout(m_vk->device,
                                     m_emptyDescriptorSetLayout,
                                     nullptr);
    m_vk->DestroySampler(m_vk->device, m_mipmapSampler, nullptr);
    m_vk->DestroySampler(m_vk->device, m_linearSampler, nullptr);
}

void RenderContextVulkanImpl::resizeGradientTexture(uint32_t width,
                                                    uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (m_gradientTexture == nullptr ||
        m_gradientTexture->info().extent.width != width ||
        m_gradientTexture->info().extent.height != height)
    {
        m_gradientTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {width, height, 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT,
        });

        m_gradTextureView = m_vk->makeTextureView(m_gradientTexture);

        m_gradTextureFramebuffer = m_vk->makeFramebuffer({
            .renderPass = m_colorRampPipeline->renderPass(),
            .attachmentCount = 1,
            .pAttachments = m_gradTextureView->vkImageViewAddressOf(),
            .width = width,
            .height = height,
            .layers = 1,
        });
    }
}

void RenderContextVulkanImpl::resizeTessellationTexture(uint32_t width,
                                                        uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (m_tessVertexTexture == nullptr ||
        m_tessVertexTexture->info().extent.width != width ||
        m_tessVertexTexture->info().extent.height != height)
    {
        m_tessVertexTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32G32B32A32_UINT,
            .extent = {width, height, 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT,
        });

        m_tessVertexTextureView = m_vk->makeTextureView(m_tessVertexTexture);

        m_tessTextureFramebuffer = m_vk->makeFramebuffer({
            .renderPass = m_tessellatePipeline->renderPass(),
            .attachmentCount = 1,
            .pAttachments = m_tessVertexTextureView->vkImageViewAddressOf(),
            .width = width,
            .height = height,
            .layers = 1,
        });
    }
}

void RenderContextVulkanImpl::resizeAtlasTexture(uint32_t width,
                                                 uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);
    if (m_atlasTexture == nullptr ||
        m_atlasTexture->info().extent.width != width ||
        m_atlasTexture->info().extent.height != height)
    {
        m_atlasTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_SFLOAT,
            .extent = {width, height, 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT,
        });

        m_atlasTextureView = m_vk->makeTextureView(m_atlasTexture);

        m_atlasFramebuffer = m_vk->makeFramebuffer({
            .renderPass = m_atlasPipeline->renderPass(),
            .attachmentCount = 1,
            .pAttachments = m_atlasTextureView->vkImageViewAddressOf(),
            .width = width,
            .height = height,
            .layers = 1,
        });
    }
}

void RenderContextVulkanImpl::resizeCoverageBuffer(size_t sizeInBytes)
{
    if (sizeInBytes == 0)
    {
        m_coverageBuffer = nullptr;
    }
    else
    {
        m_coverageBuffer = m_vk->makeBuffer(
            {
                .size = sizeInBytes,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            },
            vkutil::Mappability::none);
    }
}

void RenderContextVulkanImpl::prepareToMapBuffers()
{
    m_bufferRingIdx = (m_bufferRingIdx + 1) % gpu::kBufferRingSize;

    // Wait for the existing resources to finish before we release/recycle them.
    if (rcp<gpu::CommandBufferCompletionFence> fence =
            std::move(m_frameCompletionFences[m_bufferRingIdx]))
    {
        fence->wait();
    }

    // Delete resources that are no longer referenced by in-flight command
    // buffers.
    m_vk->onNewFrameBegun();

    // Clean expired resources in our pool of descriptor set pools.
    m_descriptorSetPoolPool->cleanExcessExpiredResources();

    // Synchronize buffer sizes in the buffer rings.
    m_flushUniformBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_imageDrawUniformBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_pathBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_paintBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_paintAuxBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_contourBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_gradSpanBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_tessSpanBufferRing.synchronizeSizeAt(m_bufferRingIdx);
    m_triangleBufferRing.synchronizeSizeAt(m_bufferRingIdx);
}

namespace descriptor_pool_limits
{
constexpr static uint32_t kMaxUniformUpdates = 3;
constexpr static uint32_t kMaxDynamicUniformUpdates = 1;
constexpr static uint32_t kMaxImageTextureUpdates = 256;
constexpr static uint32_t kMaxSampledImageUpdates =
    4 + kMaxImageTextureUpdates; // tess + grad + feather + atlas + images
constexpr static uint32_t kMaxStorageImageUpdates =
    1; // coverageAtomicTexture in atomic mode.
constexpr static uint32_t kMaxStorageBufferUpdates =
    7 + // path/paint/uniform buffers
    1;  // coverage buffer in clockwiseAtomic mode
constexpr static uint32_t kMaxDescriptorSets = 3 + kMaxImageTextureUpdates;
} // namespace descriptor_pool_limits

RenderContextVulkanImpl::DescriptorSetPool::DescriptorSetPool(
    rcp<VulkanContext> vk) :
    m_vk(std::move(vk))
{
    VkDescriptorPoolSize descriptorPoolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = descriptor_pool_limits::kMaxUniformUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount =
                descriptor_pool_limits::kMaxDynamicUniformUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = descriptor_pool_limits::kMaxSampledImageUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = descriptor_pool_limits::kMaxStorageImageUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = descriptor_pool_limits::kMaxStorageBufferUpdates,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 4,
        },
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = descriptor_pool_limits::kMaxDescriptorSets,
        .poolSizeCount = std::size(descriptorPoolSizes),
        .pPoolSizes = descriptorPoolSizes,
    };

    VK_CHECK(m_vk->CreateDescriptorPool(m_vk->device,
                                        &descriptorPoolCreateInfo,
                                        nullptr,
                                        &m_vkDescriptorPool));
}

RenderContextVulkanImpl::DescriptorSetPool::~DescriptorSetPool()
{
    m_vk->DestroyDescriptorPool(m_vk->device, m_vkDescriptorPool, nullptr);
}

VkDescriptorSet RenderContextVulkanImpl::DescriptorSetPool::
    allocateDescriptorSet(VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_vkDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet descriptorSet;
    VK_CHECK(m_vk->AllocateDescriptorSets(m_vk->device,
                                          &descriptorSetAllocateInfo,
                                          &descriptorSet));

    return descriptorSet;
}

void RenderContextVulkanImpl::DescriptorSetPool::reset()
{
    m_vk->ResetDescriptorPool(m_vk->device, m_vkDescriptorPool, 0);
}

vkutil::TextureView* RenderTargetVulkan::ensureOffscreenColorTextureView()
{
    if (m_offscreenColorTextureView == nullptr)
    {
        m_offscreenColorTexture = m_vk->makeTexture({
            .format = m_framebufferFormat,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        });

        m_offscreenColorTextureView =
            m_vk->makeTextureView(m_offscreenColorTexture);
    }

    return m_offscreenColorTextureView.get();
}

vkutil::TextureView* RenderTargetVulkan::ensureClipTextureView()
{
    if (m_clipTextureView == nullptr)
    {
        m_clipTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        m_clipTextureView = m_vk->makeTextureView(m_clipTexture);
    }

    return m_clipTextureView.get();
}

vkutil::TextureView* RenderTargetVulkan::ensureScratchColorTextureView()
{
    if (m_scratchColorTextureView == nullptr)
    {
        m_scratchColorTexture = m_vk->makeTexture({
            .format = m_framebufferFormat,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        m_scratchColorTextureView =
            m_vk->makeTextureView(m_scratchColorTexture);
    }

    return m_scratchColorTextureView.get();
}

vkutil::TextureView* RenderTargetVulkan::ensureCoverageTextureView()
{
    if (m_coverageTextureView == nullptr)
    {
        m_coverageTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        m_coverageTextureView = m_vk->makeTextureView(m_coverageTexture);
    }

    return m_coverageTextureView.get();
}

vkutil::TextureView* RenderTargetVulkan::ensureCoverageAtomicTextureView()
{
    if (m_coverageAtomicTextureView == nullptr)
    {
        m_coverageAtomicTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage =
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT, // For vkCmdClearColorImage
        });

        m_coverageAtomicTextureView =
            m_vk->makeTextureView(m_coverageAtomicTexture);
    }

    return m_coverageAtomicTextureView.get();
}

vkutil::TextureView* RenderTargetVulkan::ensureDepthStencilTextureView()
{
    if (m_depthStencilTextureView == nullptr)
    {
        m_depthStencilTexture = m_vk->makeTexture({
            .format = get_preferred_depth_stencil_format(m_vk->supportsD24S8()),
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        });

        m_depthStencilTextureView =
            m_vk->makeTextureView(m_depthStencilTexture);
    }

    return m_depthStencilTextureView.get();
}

void RenderContextVulkanImpl::flush(const FlushDescriptor& desc)
{
    constexpr static VkDeviceSize zeroOffset[1] = {0};
    constexpr static uint32_t zeroOffset32[1] = {0};

    if (desc.interlockMode == gpu::InterlockMode::msaa)
    {
        return;
    }

    auto commandBuffer =
        reinterpret_cast<VkCommandBuffer>(desc.externalCommandBuffer);
    rcp<DescriptorSetPool> descriptorSetPool = m_descriptorSetPoolPool->make();

    // Apply pending texture updates.
    if (m_featherTexture->hasUpdates())
    {
        m_featherTexture->synchronize(commandBuffer);
    }
    if (m_nullImageTexture->hasUpdates())
    {
        m_nullImageTexture->synchronize(commandBuffer);
    }
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (auto imageTextureVulkan =
                static_cast<const TextureVulkanImpl*>(batch.imageTexture))
        {
            if (imageTextureVulkan->hasUpdates())
            {
                imageTextureVulkan->synchronize(commandBuffer);
            }
        }
    }

    // Create a per-flush descriptor set.
    VkDescriptorSet perFlushDescriptorSet =
        descriptorSetPool->allocateDescriptorSet(m_perFlushDescriptorSetLayout);

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = FLUSH_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        },
        {{
            .buffer = m_flushUniformBufferRing.vkBufferAt(m_bufferRingIdx),
            .offset = desc.flushUniformDataOffsetInBytes,
            .range = sizeof(gpu::FlushUniforms),
        }});

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        },
        {{
            .buffer = m_imageDrawUniformBufferRing.vkBufferAt(m_bufferRingIdx),
            .offset = 0,
            .range = sizeof(gpu::ImageDrawUniforms),
        }});

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = PATH_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        },
        {{
            .buffer = m_pathBufferRing.vkBufferAt(m_bufferRingIdx),
            .offset = desc.firstPath * sizeof(gpu::PathData),
            .range = VK_WHOLE_SIZE,
        }});

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = PAINT_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        },
        {
            {
                .buffer = m_paintBufferRing.vkBufferAt(m_bufferRingIdx),
                .offset = desc.firstPaint * sizeof(gpu::PaintData),
                .range = VK_WHOLE_SIZE,
            },
            {
                .buffer = m_paintAuxBufferRing.vkBufferAt(m_bufferRingIdx),
                .offset = desc.firstPaintAux * sizeof(gpu::PaintAuxData),
                .range = VK_WHOLE_SIZE,
            },
        });
    static_assert(PAINT_AUX_BUFFER_IDX == PAINT_BUFFER_IDX + 1);

    m_vk->updateBufferDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = CONTOUR_BUFFER_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        },
        {{
            .buffer = m_contourBufferRing.vkBufferAt(m_bufferRingIdx),
            .offset = desc.firstContour * sizeof(gpu::ContourData),
            .range = VK_WHOLE_SIZE,
        }});

    if (desc.interlockMode == gpu::InterlockMode::clockwiseAtomic &&
        m_coverageBuffer != nullptr)
    {
        m_vk->updateBufferDescriptorSets(
            perFlushDescriptorSet,
            {
                .dstBinding = COVERAGE_BUFFER_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            },
            {{
                .buffer = *m_coverageBuffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            }});
    }

    m_vk->updateImageDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = TESS_VERTEX_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = *m_tessVertexTextureView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    m_vk->updateImageDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = GRAD_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = *m_gradTextureView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    m_vk->updateImageDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = FEATHER_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = *m_featherTexture->m_textureView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    m_vk->updateImageDescriptorSets(
        perFlushDescriptorSet,
        {
            .dstBinding = ATLAS_TEXTURE_IDX,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        },
        {{
            .imageView = *m_atlasTextureView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }});

    VulkanContext::TextureAccess lastGradTextureAccess, lastTessTextureAccess,
        lastAtlasTextureAccess;
    lastTessTextureAccess = lastGradTextureAccess = lastAtlasTextureAccess = {
        // The last thing to access the gradient and tessellation textures was
        // the previous flush.
        // Make sure our barriers account for this so we don't overwrite these
        // textures before previous draws are done reading them.
        .pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .accessMask = VK_ACCESS_SHADER_READ_BIT,
        // Transition from an "UNDEFINED" layout because we don't care about
        // preserving color ramp content from the previous frame.
        .layout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    // Render the complex color ramps to the gradient texture.
    if (desc.gradSpanCount > 0)
    {
        // Wait for previous accesses to finish before rendering to the gradient
        // texture.
        lastGradTextureAccess = m_vk->simpleImageMemoryBarrier(
            commandBuffer,
            lastGradTextureAccess,
            {
                .pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            *m_gradientTexture);

        VkRect2D renderArea = {
            .extent = {gpu::kGradTextureWidth, desc.gradDataHeight},
        };

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_colorRampPipeline->renderPass(),
            .framebuffer = *m_gradTextureFramebuffer,
            .renderArea = renderArea,
        };

        m_vk->CmdBeginRenderPass(commandBuffer,
                                 &renderPassBeginInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);

        m_vk->CmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_colorRampPipeline->renderPipeline());

        m_vk->CmdSetViewport(commandBuffer,
                             0,
                             1,
                             vkutil::ViewportFromRect2D(renderArea));

        m_vk->CmdSetScissor(commandBuffer, 0, 1, &renderArea);

        VkBuffer gradSpanBuffer =
            m_gradSpanBufferRing.vkBufferAt(m_bufferRingIdx);
        VkDeviceSize gradSpanOffset =
            desc.firstGradSpan * sizeof(gpu::GradientSpan);
        m_vk->CmdBindVertexBuffers(commandBuffer,
                                   0,
                                   1,
                                   &gradSpanBuffer,
                                   &gradSpanOffset);

        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_colorRampPipeline->pipelineLayout(),
                                    PER_FLUSH_BINDINGS_SET,
                                    1,
                                    &perFlushDescriptorSet,
                                    1,
                                    zeroOffset32);

        m_vk->CmdDraw(commandBuffer,
                      gpu::GRAD_SPAN_TRI_STRIP_VERTEX_COUNT,
                      desc.gradSpanCount,
                      0,
                      0);

        m_vk->CmdEndRenderPass(commandBuffer);

        // The render pass transitioned the gradient texture to
        // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
        lastGradTextureAccess.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Ensure the gradient texture has finished updating before the path
    // fragment shaders read it.
    m_vk->simpleImageMemoryBarrier(
        commandBuffer,
        lastGradTextureAccess,
        {
            .pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .accessMask = VK_ACCESS_SHADER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        *m_gradientTexture);

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        // Don't render new vertices until the previous flush has finished using
        // the tessellation texture.
        lastTessTextureAccess = m_vk->simpleImageMemoryBarrier(
            commandBuffer,
            lastTessTextureAccess,
            {
                .pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            *m_tessVertexTexture);

        VkRect2D renderArea = {
            .extent = {gpu::kTessTextureWidth, desc.tessDataHeight},
        };

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_tessellatePipeline->renderPass(),
            .framebuffer = *m_tessTextureFramebuffer,
            .renderArea = renderArea,
        };

        m_vk->CmdBeginRenderPass(commandBuffer,
                                 &renderPassBeginInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);

        m_vk->CmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_tessellatePipeline->renderPipeline());

        m_vk->CmdSetViewport(commandBuffer,
                             0,
                             1,
                             vkutil::ViewportFromRect2D(renderArea));

        m_vk->CmdSetScissor(commandBuffer, 0, 1, &renderArea);

        VkBuffer tessBuffer = m_tessSpanBufferRing.vkBufferAt(m_bufferRingIdx);
        VkDeviceSize tessOffset =
            desc.firstTessVertexSpan * sizeof(gpu::TessVertexSpan);
        m_vk->CmdBindVertexBuffers(commandBuffer,
                                   0,
                                   1,
                                   &tessBuffer,
                                   &tessOffset);

        m_vk->CmdBindIndexBuffer(commandBuffer,
                                 *m_tessSpanIndexBuffer,
                                 0,
                                 VK_INDEX_TYPE_UINT16);

        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_tessellatePipeline->pipelineLayout(),
                                    PER_FLUSH_BINDINGS_SET,
                                    1,
                                    &perFlushDescriptorSet,
                                    1,
                                    zeroOffset32);
        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_tessellatePipeline->pipelineLayout(),
                                    SAMPLER_BINDINGS_SET,
                                    1,
                                    &m_immutableSamplerDescriptorSet,
                                    0,
                                    nullptr);

        m_vk->CmdDrawIndexed(commandBuffer,
                             std::size(gpu::kTessSpanIndices),
                             desc.tessVertexSpanCount,
                             0,
                             0,
                             0);

        m_vk->CmdEndRenderPass(commandBuffer);

        // The render pass transitioned the tessellation texture to
        // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
        lastTessTextureAccess.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Ensure the tessellation texture has finished rendering before the path
    // vertex shaders read it.
    m_vk->simpleImageMemoryBarrier(
        commandBuffer,
        lastTessTextureAccess,
        {
            .pipelineStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            .accessMask = VK_ACCESS_SHADER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        *m_tessVertexTexture);

    // Render the atlas if we have any offscreen feathers.
    if ((desc.atlasFillBatchCount | desc.atlasStrokeBatchCount) != 0)
    {
        // Don't render new vertices until the previous flush has finished using
        // the atlas texture.
        lastAtlasTextureAccess = m_vk->simpleImageMemoryBarrier(
            commandBuffer,
            lastAtlasTextureAccess,
            {
                .pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            *m_atlasTexture);

        VkRect2D renderArea = {
            .extent = {desc.atlasContentWidth, desc.atlasContentHeight},
        };

        VkClearValue atlasClearValue = {};

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_atlasPipeline->renderPass(),
            .framebuffer = *m_atlasFramebuffer,
            .renderArea = renderArea,
            .clearValueCount = 1,
            .pClearValues = &atlasClearValue,
        };

        m_vk->CmdBeginRenderPass(commandBuffer,
                                 &renderPassBeginInfo,
                                 VK_SUBPASS_CONTENTS_INLINE);
        m_vk->CmdSetViewport(commandBuffer,
                             0,
                             1,
                             vkutil::ViewportFromRect2D(renderArea));
        m_vk->CmdBindVertexBuffers(commandBuffer,
                                   0,
                                   1,
                                   m_pathPatchVertexBuffer->vkBufferAddressOf(),
                                   zeroOffset);
        m_vk->CmdBindIndexBuffer(commandBuffer,
                                 *m_pathPatchIndexBuffer,
                                 0,
                                 VK_INDEX_TYPE_UINT16);
        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_atlasPipeline->pipelineLayout(),
                                    PER_FLUSH_BINDINGS_SET,
                                    1,
                                    &perFlushDescriptorSet,
                                    1,
                                    zeroOffset32);
        m_vk->CmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_atlasPipeline->pipelineLayout(),
                                    SAMPLER_BINDINGS_SET,
                                    1,
                                    &m_immutableSamplerDescriptorSet,
                                    0,
                                    nullptr);
        if (desc.atlasFillBatchCount != 0)
        {
            m_vk->CmdBindPipeline(commandBuffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  m_atlasPipeline->fillPipeline());
            for (size_t i = 0; i < desc.atlasFillBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& fillBatch = desc.atlasFillBatches[i];
                VkRect2D scissor = {
                    .offset = {fillBatch.scissor.left, fillBatch.scissor.top},
                    .extent = {fillBatch.scissor.width(),
                               fillBatch.scissor.height()},
                };
                m_vk->CmdSetScissor(commandBuffer, 0, 1, &scissor);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     gpu::kMidpointFanCenterAAPatchIndexCount,
                                     fillBatch.patchCount,
                                     gpu::kMidpointFanCenterAAPatchBaseIndex,
                                     0,
                                     fillBatch.basePatch);
            }
        }

        if (desc.atlasStrokeBatchCount != 0)
        {
            m_vk->CmdBindPipeline(commandBuffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  m_atlasPipeline->strokePipeline());
            for (size_t i = 0; i < desc.atlasStrokeBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& strokeBatch =
                    desc.atlasStrokeBatches[i];
                VkRect2D scissor = {
                    .offset = {strokeBatch.scissor.left,
                               strokeBatch.scissor.top},
                    .extent = {strokeBatch.scissor.width(),
                               strokeBatch.scissor.height()},
                };
                m_vk->CmdSetScissor(commandBuffer, 0, 1, &scissor);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     gpu::kMidpointFanPatchBorderIndexCount,
                                     strokeBatch.patchCount,
                                     gpu::kMidpointFanPatchBaseIndex,
                                     0,
                                     strokeBatch.basePatch);
            }
        }

        m_vk->CmdEndRenderPass(commandBuffer);

        // The render pass transitioned the atlas texture to
        // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
        lastAtlasTextureAccess.layout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Ensure the atlas texture has finished rendering before the fragment
    // shaders read it.
    m_vk->simpleImageMemoryBarrier(
        commandBuffer,
        lastAtlasTextureAccess,
        {
            .pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .accessMask = VK_ACCESS_SHADER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        *m_atlasTexture);

    auto* renderTarget = static_cast<RenderTargetVulkan*>(desc.renderTarget);

    auto pipelineLayoutOptions = DrawPipelineLayoutOptions::none;
    if (desc.interlockMode != gpu::InterlockMode::rasterOrdering &&
        !(desc.combinedShaderFeatures &
          gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND))
    {
        pipelineLayoutOptions |=
            DrawPipelineLayoutOptions::fixedFunctionColorOutput;
    }

    int pipelineLayoutIdx = (static_cast<int>(desc.interlockMode)
                             << kDrawPipelineLayoutOptionCount) |
                            static_cast<int>(pipelineLayoutOptions);
    assert(pipelineLayoutIdx < m_drawPipelineLayouts.size());
    if (m_drawPipelineLayouts[pipelineLayoutIdx] == nullptr)
    {
        m_drawPipelineLayouts[pipelineLayoutIdx] =
            std::make_unique<DrawPipelineLayout>(this,
                                                 desc.interlockMode,
                                                 pipelineLayoutOptions);
    }
    DrawPipelineLayout& pipelineLayout =
        *m_drawPipelineLayouts[pipelineLayoutIdx];
    bool fixedFunctionColorOutput =
        pipelineLayout.options() &
        DrawPipelineLayoutOptions::fixedFunctionColorOutput;

    vkutil::TextureView* colorView =
        renderTarget->targetViewContainsUsageFlag(
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) ||
                fixedFunctionColorOutput
            ? renderTarget->targetTextureView()
            : renderTarget->ensureOffscreenColorTextureView();
    vkutil::TextureView *clipView = nullptr, *scratchColorTextureView = nullptr,
                        *coverageTextureView = nullptr,
                        *depthStencilTextureView = nullptr;
    if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
    {
        clipView = renderTarget->ensureClipTextureView();
        scratchColorTextureView = renderTarget->ensureScratchColorTextureView();
        coverageTextureView = renderTarget->ensureCoverageTextureView();
    }
    else if (desc.interlockMode == gpu::InterlockMode::atomics)
    {
        // In atomic mode, the clip plane is RGBA8. Just use the scratch color
        // texture since it isn't otherwise used in atomic mode.
        clipView = renderTarget->ensureScratchColorTextureView();
        scratchColorTextureView = nullptr;
        coverageTextureView = renderTarget->ensureCoverageAtomicTextureView();
    }
    else if (desc.interlockMode == gpu::InterlockMode::msaa)
    {
        depthStencilTextureView = renderTarget->ensureDepthStencilTextureView();
    }

    VulkanContext::TextureAccess initialColorAccess =
        renderTarget->targetLastAccess();

    if (desc.colorLoadAction != gpu::LoadAction::preserveRenderTarget)
    {
        // No need to preserve what was in the render target before.
        initialColorAccess.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    else if (colorView == renderTarget->offscreenColorTextureView())
    {
        // The access we just declared was actually for the *renderTarget*
        // texture, not the actual color attachment we will render to, which
        // will be the the offscreen texture.
        VulkanContext::TextureAccess initialRenderTargetAccess =
            initialColorAccess;

        // The initial *color attachment* (aka offscreenColorTexture) access is
        // the previous frame's copy into the renderTarget texture.
        initialColorAccess = {
            .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        };

        // Copy the target into our offscreen color texture before rendering.
        VkImageMemoryBarrier imageMemoryBarriers[] = {
            {
                .srcAccessMask = initialRenderTargetAccess.accessMask,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = initialRenderTargetAccess.layout,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .image = renderTarget->targetTexture(),
            },
            {
                .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .dstAccessMask = initialColorAccess.accessMask,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = initialColorAccess.layout,
                .image = renderTarget->offscreenColorTexture(),
            },
        };

        m_vk->imageMemoryBarriers(commandBuffer,
                                  initialRenderTargetAccess.pipelineStages |
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  initialColorAccess.pipelineStages |
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  0,
                                  std::size(imageMemoryBarriers),
                                  imageMemoryBarriers);

        m_vk->blitSubRect(commandBuffer,
                          renderTarget->targetTexture(),
                          renderTarget->offscreenColorTexture(),
                          desc.renderTargetUpdateBounds);
    }

    int renderPassVariantIdx = DrawPipelineLayout::RenderPassVariantIdx(
        renderTarget->framebufferFormat(),
        desc.colorLoadAction);
    VkRenderPass vkRenderPass =
        pipelineLayout.renderPassAt(renderPassVariantIdx);

    VkImageView imageViews[] = {
        *colorView,
        clipView != nullptr ? *clipView : VK_NULL_HANDLE,
        scratchColorTextureView != nullptr ? *scratchColorTextureView
                                           : VK_NULL_HANDLE,
        coverageTextureView != nullptr ? *coverageTextureView : VK_NULL_HANDLE,
        depthStencilTextureView != nullptr ? *depthStencilTextureView
                                           : VK_NULL_HANDLE,
    };
    static_assert(COLOR_PLANE_IDX == 0);
    static_assert(CLIP_PLANE_IDX == 1);
    static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
    static_assert(COVERAGE_PLANE_IDX == 3);
    static_assert(DEPTH_STENCIL_IDX == 4);

    rcp<vkutil::Framebuffer> framebuffer = m_vk->makeFramebuffer({
        .renderPass = vkRenderPass,
        .attachmentCount = pipelineLayout.attachmentCount(),
        .pAttachments = imageViews,
        .width = static_cast<uint32_t>(renderTarget->width()),
        .height = static_cast<uint32_t>(renderTarget->height()),
        .layers = 1,
    });

    VkRect2D renderArea = {
        .extent = {static_cast<uint32_t>(renderTarget->width()),
                   static_cast<uint32_t>(renderTarget->height())},
    };

    VkClearValue clearValues[] = {
        {.color = vkutil::color_clear_rgba32f(desc.colorClearValue)},
        {},
        {},
        {.color = vkutil::color_clear_r32ui(desc.coverageClearValue)},
        {.depthStencil = {desc.depthClearValue, desc.stencilClearValue}},
    };
    static_assert(COLOR_PLANE_IDX == 0);
    static_assert(CLIP_PLANE_IDX == 1);
    static_assert(SCRATCH_COLOR_PLANE_IDX == 2);
    static_assert(COVERAGE_PLANE_IDX == 3);
    static_assert(DEPTH_STENCIL_IDX == 4);

    // Ensure any previous accesses to the color texture complete before we
    // begin rendering.
    m_vk->simpleImageMemoryBarrier(
        commandBuffer,
        initialColorAccess,
        {
            // "Load" operations always occur in
            // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT.
            .pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .layout = VK_IMAGE_LAYOUT_GENERAL,
        },
        colorView->info().image);

    bool needsBarrierBeforeNextDraw = false;
    if (desc.interlockMode == gpu::InterlockMode::atomics)
    {
        // Clear the coverage texture, which is not an attachment.
        VkImageSubresourceRange clearRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        };

        // Don't clear the coverage texture until shaders in the previous flush
        // have finished using it.
        m_vk->imageMemoryBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            {
                .srcAccessMask =
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = renderTarget->coverageAtomicTexture(),
            });

        const VkClearColorValue coverageClearValue =
            vkutil::color_clear_r32ui(desc.coverageClearValue);
        m_vk->CmdClearColorImage(commandBuffer,
                                 renderTarget->coverageAtomicTexture(),
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 &coverageClearValue,
                                 1,
                                 &clearRange);

        // Don't use the coverage texture in shaders until the clear finishes.
        m_vk->imageMemoryBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            {
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask =
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .image = renderTarget->coverageAtomicTexture(),
            });

        // Make sure we get a barrier between load operations and the first
        // color attachment accesses.
        needsBarrierBeforeNextDraw = true;
    }
    else if (desc.interlockMode == gpu::InterlockMode::clockwiseAtomic)
    {
        VkPipelineStageFlags lastCoverageBufferStage =
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        VkAccessFlags lastCoverageBufferAccess = VK_ACCESS_SHADER_WRITE_BIT;

        if (desc.needsCoverageBufferClear)
        {
            assert(m_coverageBuffer != nullptr);

            // Don't clear the coverage buffer until shaders in the previous
            // flush have finished accessing it.
            m_vk->bufferMemoryBarrier(
                commandBuffer,
                lastCoverageBufferStage,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                {
                    .srcAccessMask = lastCoverageBufferAccess,
                    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .buffer = *m_coverageBuffer,
                });

            m_vk->CmdFillBuffer(commandBuffer,
                                *m_coverageBuffer,
                                0,
                                m_coverageBuffer->info().size,
                                0);

            lastCoverageBufferStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            lastCoverageBufferAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        }

        if (m_coverageBuffer != nullptr)
        {
            // Don't use the coverage buffer until prior clears/accesses
            // have completed.
            m_vk->bufferMemoryBarrier(
                commandBuffer,
                lastCoverageBufferStage,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                {
                    .srcAccessMask = lastCoverageBufferAccess,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .buffer = *m_coverageBuffer,
                });
        }
    }

    // Ensure all reads to any internal attachments complete before we execute
    // the load operations.
    m_vk->memoryBarrier(
        commandBuffer,
        // "Load" operations always occur in
        // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT.
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        {
            .srcAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        });

    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = vkRenderPass,
        .framebuffer = *framebuffer,
        .renderArea = renderArea,
        .clearValueCount = std::size(clearValues),
        .pClearValues = clearValues,
    };

    m_vk->CmdBeginRenderPass(commandBuffer,
                             &renderPassBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);

    m_vk->CmdSetViewport(commandBuffer,
                         0,
                         1,
                         vkutil::ViewportFromRect2D(renderArea));

    m_vk->CmdSetScissor(commandBuffer, 0, 1, &renderArea);

    // Update the PLS input attachment descriptor sets.
    VkDescriptorSet inputAttachmentDescriptorSet = VK_NULL_HANDLE;
    if (desc.interlockMode == gpu::InterlockMode::rasterOrdering ||
        desc.interlockMode == gpu::InterlockMode::atomics)
    {
        inputAttachmentDescriptorSet = descriptorSetPool->allocateDescriptorSet(
            pipelineLayout.plsLayout());

        if (!fixedFunctionColorOutput)
        {
            m_vk->updateImageDescriptorSets(
                inputAttachmentDescriptorSet,
                {
                    .dstBinding = COLOR_PLANE_IDX,
                    .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                },
                {{
                    .imageView = *colorView,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }});
        }

        m_vk->updateImageDescriptorSets(
            inputAttachmentDescriptorSet,
            {
                .dstBinding = CLIP_PLANE_IDX,
                .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            },
            {{
                .imageView = *clipView,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            }});

        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
        {
            m_vk->updateImageDescriptorSets(
                inputAttachmentDescriptorSet,
                {
                    .dstBinding = SCRATCH_COLOR_PLANE_IDX,
                    .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                },
                {{
                    .imageView = *scratchColorTextureView,
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }});
        }

        assert(coverageTextureView != nullptr);
        m_vk->updateImageDescriptorSets(
            inputAttachmentDescriptorSet,
            {
                .dstBinding = COVERAGE_PLANE_IDX,
                .descriptorType =
                    desc.interlockMode == gpu::InterlockMode::atomics
                        ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                        : VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            },
            {{
                .imageView = *coverageTextureView,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            }});
    }

    // Bind the descriptor sets for this draw pass.
    // (The imageTexture and imageDraw dynamic uniform offsets might have to
    // update between draws, but this is otherwise all we need to bind!)
    VkDescriptorSet drawDescriptorSets[] = {
        perFlushDescriptorSet,
        m_nullImageDescriptorSet,
        m_immutableSamplerDescriptorSet,
        inputAttachmentDescriptorSet,
    };
    static_assert(PER_FLUSH_BINDINGS_SET == 0);
    static_assert(PER_DRAW_BINDINGS_SET == 1);
    static_assert(SAMPLER_BINDINGS_SET == 2);
    static_assert(PLS_TEXTURE_BINDINGS_SET == 3);
    static_assert(BINDINGS_SET_COUNT == 4);

    m_vk->CmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                *pipelineLayout,
                                PER_FLUSH_BINDINGS_SET,
                                drawDescriptorSets[PLS_TEXTURE_BINDINGS_SET] !=
                                        VK_NULL_HANDLE
                                    ? BINDINGS_SET_COUNT
                                    : BINDINGS_SET_COUNT - 1,
                                drawDescriptorSets,
                                1,
                                zeroOffset32);

    // Execute the DrawList.
    uint32_t imageTextureUpdateCount = 0;
    for (const DrawBatch& batch : *desc.drawList)
    {
        if (batch.elementCount == 0)
        {
            needsBarrierBeforeNextDraw =
                needsBarrierBeforeNextDraw || batch.needsBarrier;
            continue;
        }

        DrawType drawType = batch.drawType;

        if (batch.imageTexture != nullptr)
        {
            // Update the imageTexture binding and the dynamic offset into the
            // imageDraw uniform buffer.
            auto imageTexture =
                static_cast<const TextureVulkanImpl*>(batch.imageTexture);
            if (imageTexture->m_descriptorSetFrameIdx !=
                m_vk->currentFrameIdx())
            {
                // Update the image's "texture binding" descriptor set. (These
                // expire every frame, so we need to make a new one each frame.)
                if (imageTextureUpdateCount >=
                    descriptor_pool_limits::kMaxImageTextureUpdates)
                {
                    // We ran out of room for image texture updates. Allocate a
                    // new pool.
                    descriptorSetPool = m_descriptorSetPoolPool->make();
                    imageTextureUpdateCount = 0;
                }

                imageTexture->m_imageTextureDescriptorSet =
                    descriptorSetPool->allocateDescriptorSet(
                        m_perDrawDescriptorSetLayout);

                m_vk->updateImageDescriptorSets(
                    imageTexture->m_imageTextureDescriptorSet,
                    {
                        .dstBinding = IMAGE_TEXTURE_IDX,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                    },
                    {{
                        .imageView = *imageTexture->m_textureView,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    }});

                ++imageTextureUpdateCount;
                imageTexture->m_descriptorSetFrameIdx = m_vk->currentFrameIdx();
            }

            VkDescriptorSet imageDescriptorSets[] = {
                perFlushDescriptorSet, // Dynamic offset to imageDraw uniforms.
                imageTexture->m_imageTextureDescriptorSet, // imageTexture.
            };
            static_assert(PER_DRAW_BINDINGS_SET == PER_FLUSH_BINDINGS_SET + 1);

            m_vk->CmdBindDescriptorSets(commandBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        *pipelineLayout,
                                        PER_FLUSH_BINDINGS_SET,
                                        std::size(imageDescriptorSets),
                                        imageDescriptorSets,
                                        1,
                                        &batch.imageDrawDataOffset);
        }

        // Setup the pipeline for this specific drawType and shaderFeatures.
        gpu::ShaderFeatures shaderFeatures =
            desc.interlockMode == gpu::InterlockMode::atomics
                ? desc.combinedShaderFeatures
                : batch.shaderFeatures;
        auto shaderMiscFlags = batch.shaderMiscFlags;
        if (fixedFunctionColorOutput)
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::fixedFunctionColorOutput;
        }
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
            (batch.drawContents & gpu::DrawContents::clockwiseFill))
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::clockwiseFill;
        }
        uint32_t pipelineKey = gpu::ShaderUniqueKey(drawType,
                                                    shaderFeatures,
                                                    desc.interlockMode,
                                                    shaderMiscFlags);
        auto drawPipelineOptions = DrawPipelineOptions::none;
        if (desc.wireframe && m_vk->features.fillModeNonSolid)
        {
            drawPipelineOptions |= DrawPipelineOptions::wireframe;
        }
        assert(pipelineKey << kDrawPipelineOptionCount >>
                   kDrawPipelineOptionCount ==
               pipelineKey);
        pipelineKey = (pipelineKey << kDrawPipelineOptionCount) |
                      static_cast<uint32_t>(drawPipelineOptions);
        assert(pipelineKey * DrawPipelineLayout::kRenderPassVariantCount /
                   DrawPipelineLayout::kRenderPassVariantCount ==
               pipelineKey);
        pipelineKey =
            (pipelineKey * DrawPipelineLayout::kRenderPassVariantCount) +
            renderPassVariantIdx;
        const DrawPipeline& drawPipeline = m_drawPipelines
                                               .try_emplace(pipelineKey,
                                                            this,
                                                            drawType,
                                                            pipelineLayout,
                                                            shaderFeatures,
                                                            shaderMiscFlags,
                                                            drawPipelineOptions,
                                                            vkRenderPass)
                                               .first->second;
        m_vk->CmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              drawPipeline.vkPipeline());

        if (needsBarrierBeforeNextDraw &&
            drawType != gpu::DrawType::atomicResolve) // The atomic resolve gets
                                                      // its barrier via
                                                      // vkCmdNextSubpass().
        {
            if (desc.interlockMode == gpu::InterlockMode::atomics)
            {
                // Wait for color attachment writes to complete before we read
                // the input attachments again.
                m_vk->memoryBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_DEPENDENCY_BY_REGION_BIT,
                    {
                        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                    });
            }
            else
            {
                assert(desc.interlockMode ==
                       gpu::InterlockMode::clockwiseAtomic);
                // Wait for prior fragment shaders to finish updating the
                // coverage buffer before we read it again.
                m_vk->memoryBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_DEPENDENCY_BY_REGION_BIT,
                    {
                        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    });
            }
        }

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw patches that connect the tessellation vertices.
                m_vk->CmdBindVertexBuffers(
                    commandBuffer,
                    0,
                    1,
                    m_pathPatchVertexBuffer->vkBufferAddressOf(),
                    zeroOffset);
                m_vk->CmdBindIndexBuffer(commandBuffer,
                                         *m_pathPatchIndexBuffer,
                                         0,
                                         VK_INDEX_TYPE_UINT16);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     gpu::PatchIndexCount(drawType),
                                     batch.elementCount,
                                     gpu::PatchBaseIndex(drawType),
                                     0,
                                     batch.baseElement);
                break;
            }

            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
            {
                VkBuffer buffer =
                    m_triangleBufferRing.vkBufferAt(m_bufferRingIdx);
                m_vk->CmdBindVertexBuffers(commandBuffer,
                                           0,
                                           1,
                                           &buffer,
                                           zeroOffset);
                m_vk->CmdDraw(commandBuffer,
                              batch.elementCount,
                              1,
                              batch.baseElement,
                              0);
                break;
            }

            case DrawType::imageRect:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                m_vk->CmdBindVertexBuffers(
                    commandBuffer,
                    0,
                    1,
                    m_imageRectVertexBuffer->vkBufferAddressOf(),
                    zeroOffset);
                m_vk->CmdBindIndexBuffer(commandBuffer,
                                         *m_imageRectIndexBuffer,
                                         0,
                                         VK_INDEX_TYPE_UINT16);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     std::size(gpu::kImageRectIndices),
                                     1,
                                     batch.baseElement,
                                     0,
                                     0);
                break;
            }

            case DrawType::imageMesh:
            {
                LITE_RTTI_CAST_OR_BREAK(vertexBuffer,
                                        RenderBufferVulkanImpl*,
                                        batch.vertexBuffer);
                LITE_RTTI_CAST_OR_BREAK(uvBuffer,
                                        RenderBufferVulkanImpl*,
                                        batch.uvBuffer);
                LITE_RTTI_CAST_OR_BREAK(indexBuffer,
                                        RenderBufferVulkanImpl*,
                                        batch.indexBuffer);
                m_vk->CmdBindVertexBuffers(
                    commandBuffer,
                    0,
                    1,
                    vertexBuffer->frontVkBufferAddressOf(),
                    zeroOffset);
                m_vk->CmdBindVertexBuffers(commandBuffer,
                                           1,
                                           1,
                                           uvBuffer->frontVkBufferAddressOf(),
                                           zeroOffset);
                m_vk->CmdBindIndexBuffer(commandBuffer,
                                         indexBuffer->frontVkBuffer(),
                                         0,
                                         VK_INDEX_TYPE_UINT16);
                m_vk->CmdDrawIndexed(commandBuffer,
                                     batch.elementCount,
                                     1,
                                     batch.baseElement,
                                     0,
                                     0);
                break;
            }

            case DrawType::atomicResolve:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                m_vk->CmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
                m_vk->CmdDraw(commandBuffer, 4, 1, 0, 0);
                break;
            }

            case DrawType::atomicInitialize:
            case DrawType::stencilClipReset:
                RIVE_UNREACHABLE();
        }

        needsBarrierBeforeNextDraw = batch.needsBarrier;
    }

    m_vk->CmdEndRenderPass(commandBuffer);

    VulkanContext::TextureAccess finalRenderTargetAccess = {
        // "Store" operations always occur in
        // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT.
        .pipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .accessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .layout = VK_IMAGE_LAYOUT_GENERAL,
    };

    if (colorView == renderTarget->offscreenColorTextureView())
    {
        // The access we just declared was actually for the offscreen texture,
        // not the final target.
        VulkanContext::TextureAccess finalOffscreenAccess =
            finalRenderTargetAccess;

        // The final *renderTarget* access will be a copy from the offscreen
        // texture.
        finalRenderTargetAccess = {
            .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        };

        // Copy our offscreen color texture back to the render target now that
        // we've finished rendering.
        VkImageMemoryBarrier imageMemoryBarriers[] = {
            {
                .srcAccessMask = finalOffscreenAccess.accessMask,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = finalOffscreenAccess.layout,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .image = renderTarget->offscreenColorTexture(),
            },
            {
                .srcAccessMask = VK_ACCESS_NONE,
                .dstAccessMask = finalRenderTargetAccess.accessMask,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = finalRenderTargetAccess.layout,
                .image = renderTarget->targetTexture(),
            },
        };

        m_vk->imageMemoryBarriers(commandBuffer,
                                  finalOffscreenAccess.pipelineStages |
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  finalRenderTargetAccess.pipelineStages |
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  0,
                                  std::size(imageMemoryBarriers),
                                  imageMemoryBarriers);

        m_vk->blitSubRect(commandBuffer,
                          renderTarget->offscreenColorTexture(),
                          renderTarget->targetTexture(),
                          desc.renderTargetUpdateBounds);
    }

    renderTarget->setTargetLastAccess(finalRenderTargetAccess);

    if (desc.isFinalFlushOfFrame)
    {
        m_frameCompletionFences[m_bufferRingIdx] =
            ref_rcp(desc.frameCompletionFence);
    }
}

std::unique_ptr<RenderContext> RenderContextVulkanImpl::MakeContext(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const VulkanFeatures& features,
    PFN_vkGetInstanceProcAddr fp_vkGetInstanceProcAddr,
    PFN_vkGetDeviceProcAddr fp_vkGetDeviceProcAddr)
{
    std::unique_ptr<RenderContextVulkanImpl> impl(
        new RenderContextVulkanImpl(instance,
                                    physicalDevice,
                                    device,
                                    features,
                                    fp_vkGetInstanceProcAddr,
                                    fp_vkGetDeviceProcAddr));
    if (!impl->platformFeatures().supportsRasterOrdering &&
        !impl->platformFeatures().supportsFragmentShaderAtomics)
    {
        return nullptr; // TODO: implement MSAA.
    }
    impl->initGPUObjects();
    return std::make_unique<RenderContext>(std::move(impl));
}
} // namespace rive::gpu

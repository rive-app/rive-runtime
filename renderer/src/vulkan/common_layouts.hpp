#include "rive/renderer/gpu.hpp"
#include "rive/renderer/vulkan/vkutil.hpp"
#include "shaders/constants.glsl"
#include "image_draw_attributes.hpp"

#include <array>
#include <vulkan/vulkan.h>

// Common layout descriptors shared by various pipelines.
namespace rive::gpu::layout
{
// rasterOrdering mode with a non-input-attachment renderTarget currently
// requires the most attachments in a single pass: all 4 PLS planes plus one
// more resolve target.
constexpr static uint32_t MAX_RENDER_PASS_ATTACHMENTS = PLS_PLANE_COUNT + 1;

constexpr VkVertexInputBindingDescription PATH_INPUT_BINDINGS[] = {{
    .binding = 0,
    .stride = sizeof(rive::gpu::PatchVertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
}};
constexpr VkVertexInputAttributeDescription PATH_VERTEX_ATTRIBS[] = {
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
constexpr VkPipelineVertexInputStateCreateInfo PATH_VERTEX_INPUT_STATE = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = std::size(PATH_INPUT_BINDINGS),
    .pVertexBindingDescriptions = PATH_INPUT_BINDINGS,
    .vertexAttributeDescriptionCount = std::size(PATH_VERTEX_ATTRIBS),
    .pVertexAttributeDescriptions = PATH_VERTEX_ATTRIBS,
};

constexpr VkVertexInputBindingDescription INTERIOR_TRI_INPUT_BINDINGS[] = {{
    .binding = 0,
    .stride = sizeof(rive::gpu::TriangleVertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
}};
constexpr VkVertexInputAttributeDescription INTERIOR_TRI_VERTEX_ATTRIBS[] = {
    {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0,
    },
};
constexpr VkPipelineVertexInputStateCreateInfo INTERIOR_TRI_VERTEX_INPUT_STATE =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = std::size(INTERIOR_TRI_INPUT_BINDINGS),
        .pVertexBindingDescriptions = INTERIOR_TRI_INPUT_BINDINGS,
        .vertexAttributeDescriptionCount =
            std::size(INTERIOR_TRI_VERTEX_ATTRIBS),
        .pVertexAttributeDescriptions = INTERIOR_TRI_VERTEX_ATTRIBS,
};

constexpr VkFormat getVkFormat(VertexElementFormat format)
{
    switch (format)
    {
        case VertexElementFormat::float1:
            return VK_FORMAT_R32_SFLOAT;
        case VertexElementFormat::float2:
            return VK_FORMAT_R32G32_SFLOAT;
        case VertexElementFormat::float3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexElementFormat::float4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexElementFormat::uint8x4:
            return VK_FORMAT_R8G8B8A8_UINT;
        case VertexElementFormat::sint8x4:
            return VK_FORMAT_R8G8B8A8_SINT;
        case VertexElementFormat::unorm8x4:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case VertexElementFormat::snorm8x4:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case VertexElementFormat::uint16x2:
            return VK_FORMAT_R16G16_UINT;
        case VertexElementFormat::sint16x2:
            return VK_FORMAT_R16G16_SINT;
        case VertexElementFormat::unorm16x2:
            return VK_FORMAT_R16G16_UNORM;
        case VertexElementFormat::snorm16x2:
            return VK_FORMAT_R16G16_SNORM;
        case VertexElementFormat::uint16x4:
            return VK_FORMAT_R16G16B16A16_UINT;
        case VertexElementFormat::sint16x4:
            return VK_FORMAT_R16G16B16A16_SINT;
        case VertexElementFormat::float16x2:
            return VK_FORMAT_R16G16_SFLOAT;
        case VertexElementFormat::float16x4:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case VertexElementFormat::uint32:
            return VK_FORMAT_R32_UINT;
    }
}

// Grab these from image_draw_attributes.hpp so that it's constexpr
template <typename ImageDrawInstance> constexpr auto getImageDrawAttributes()
{
    if constexpr (std::is_same_v<ImageDrawInstance, ImageRectInstance>)
    {
        return ImageRectInstanceAttributes;
    }
    else
    {
        static_assert(std::is_same_v<ImageDrawInstance, ImageMeshInstance>);
        return ImageMeshInstanceAttributes;
    }
}

// Concatenates the given geometryAttribs with Rive's ImageRect/MeshInstance
// attribs (bound at 'binding').
template <typename ImageDrawInstance, typename... GeometryAttribs>
constexpr auto appendImageDrawInstanceAttribs(
    uint32_t binding,
    GeometryAttribs... geometryAttribs)
{
    constexpr auto Attributes = getImageDrawAttributes<ImageDrawInstance>();

    constexpr auto ArrayLength =
        sizeof...(GeometryAttribs) + std::size(Attributes);
    std::array<VkVertexInputAttributeDescription, ArrayLength> attrs = {
        geometryAttribs...};

    for (auto i = 0u; i < std::size(Attributes); i++)
    {
        const auto& src = Attributes[i];
        auto& dest = attrs[i + sizeof...(GeometryAttribs)];

        dest = {
            .location = src.attributeIndex,
            .binding = binding,
            .format = getVkFormat(src.format),
            .offset = src.byteOffset,
        };
    }

    return attrs;
}

constexpr uint32_t ImageRectGeometryBufferBinding = 0;
constexpr uint32_t ImageRectImageAttribBufferBinding = 1;
constexpr VkVertexInputBindingDescription ImageRectInputBindings[] = {
    {
        .binding = ImageRectGeometryBufferBinding,
        .stride = sizeof(rive::gpu::ImageRectVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
    {
        .binding = ImageRectImageAttribBufferBinding,
        .stride = sizeof(rive::gpu::ImageRectInstance),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
    },
};
constexpr auto ImageRectVertexAttribs =
    appendImageDrawInstanceAttribs<ImageRectInstance>(
        ImageRectImageAttribBufferBinding,
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding = ImageRectGeometryBufferBinding,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 0,
        });
constexpr VkPipelineVertexInputStateCreateInfo IMAGE_RECT_VERTEX_INPUT_STATE = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = std::size(ImageRectInputBindings),
    .pVertexBindingDescriptions = ImageRectInputBindings,
    .vertexAttributeDescriptionCount = std::size(ImageRectVertexAttribs),
    .pVertexAttributeDescriptions = ImageRectVertexAttribs.data(),
};

constexpr uint32_t ImageMeshVertexBufferBinding = 0;
constexpr uint32_t ImageMeshUVBufferBinding = 1;
constexpr uint32_t ImageMeshImageAttribBufferBinding = 2;
constexpr VkVertexInputBindingDescription ImageMeshInputBindings[] = {
    {
        .binding = ImageMeshVertexBufferBinding,
        .stride = sizeof(float) * 2,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
    {
        .binding = ImageMeshUVBufferBinding,
        .stride = sizeof(float) * 2,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
    {
        .binding = ImageMeshImageAttribBufferBinding,
        .stride = sizeof(rive::gpu::ImageMeshInstance),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
    },
};
constexpr auto ImageMeshVertexAttribs =
    appendImageDrawInstanceAttribs<ImageMeshInstance>(
        ImageMeshImageAttribBufferBinding,
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding = ImageMeshVertexBufferBinding,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 0,
        },
        VkVertexInputAttributeDescription{
            .location = 1,
            .binding = ImageMeshUVBufferBinding,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 0,
        });
constexpr VkPipelineVertexInputStateCreateInfo IMAGE_MESH_VERTEX_INPUT_STATE = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = std::size(ImageMeshInputBindings),
    .pVertexBindingDescriptions = ImageMeshInputBindings,
    .vertexAttributeDescriptionCount = std::size(ImageMeshVertexAttribs),
    .pVertexAttributeDescriptions = ImageMeshVertexAttribs.data(),
};

constexpr VkPipelineVertexInputStateCreateInfo EMPTY_VERTEX_INPUT_STATE = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 0,
    .vertexAttributeDescriptionCount = 0,
};

constexpr VkPipelineInputAssemblyStateCreateInfo INPUT_ASSEMBLY_TRIANGLE_STRIP =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
};

constexpr VkPipelineInputAssemblyStateCreateInfo INPUT_ASSEMBLY_TRIANGLE_LIST =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
};

constexpr VkPipelineViewportStateCreateInfo SINGLE_VIEWPORT = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
};

constexpr VkPipelineRasterizationStateCreateInfo RASTER_STATE_CULL_BACK_CCW = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .lineWidth = 1.f,
};

constexpr VkPipelineRasterizationStateCreateInfo RASTER_STATE_CULL_BACK_CW = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .lineWidth = 1.f,
};

constexpr VkPipelineRasterizationStateCreateInfo RASTER_STATE_CULL_NONE_CW = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_NONE,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .lineWidth = 1.f,
};

constexpr VkPipelineMultisampleStateCreateInfo MSAA_DISABLED = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
};

constexpr VkPipelineColorBlendAttachmentState BLEND_DISABLED_VALUES = {
    .colorWriteMask = rive::gpu::vkutil::kColorWriteMaskRGBA};
constexpr VkPipelineColorBlendStateCreateInfo SINGLE_ATTACHMENT_BLEND_DISABLED =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &BLEND_DISABLED_VALUES,
};

constexpr VkDynamicState DYNAMIC_VIEWPORT_SCISSOR_VALUES[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
};
constexpr VkPipelineDynamicStateCreateInfo DYNAMIC_VIEWPORT_SCISSOR = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = std::size(DYNAMIC_VIEWPORT_SCISSOR_VALUES),
    .pDynamicStates = DYNAMIC_VIEWPORT_SCISSOR_VALUES,
};

constexpr VkAttachmentReference SINGLE_ATTACHMENT_SUBPASS_REFERENCE = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
};
constexpr VkSubpassDescription SINGLE_ATTACHMENT_SUBPASS = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &SINGLE_ATTACHMENT_SUBPASS_REFERENCE,
};
} // namespace rive::gpu::layout

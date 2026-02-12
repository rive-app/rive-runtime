#include "rive/renderer/gpu.hpp"
#include "rive/renderer/vulkan/vkutil.hpp"
#include "shaders/constants.glsl"
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

constexpr VkVertexInputBindingDescription IMAGE_RECT_INPUT_BINDINGS[] = {{
    .binding = 0,
    .stride = sizeof(rive::gpu::ImageRectVertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
}};
constexpr VkVertexInputAttributeDescription IMAGE_RECT_VERTEX_ATTRIBS[] = {
    {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = 0,
    },
};
constexpr VkPipelineVertexInputStateCreateInfo IMAGE_RECT_VERTEX_INPUT_STATE = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = std::size(IMAGE_RECT_INPUT_BINDINGS),
    .pVertexBindingDescriptions = IMAGE_RECT_INPUT_BINDINGS,
    .vertexAttributeDescriptionCount = std::size(IMAGE_RECT_VERTEX_ATTRIBS),
    .pVertexAttributeDescriptions = IMAGE_RECT_VERTEX_ATTRIBS,
};

constexpr VkVertexInputBindingDescription IMAGE_MESH_INPUT_BINDINGS[] = {
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
constexpr VkVertexInputAttributeDescription IMAGE_MESH_VERTEX_ATTRIBS[] = {
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
constexpr VkPipelineVertexInputStateCreateInfo IMAGE_MESH_VERTEX_INPUT_STATE = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = std::size(IMAGE_MESH_INPUT_BINDINGS),
    .pVertexBindingDescriptions = IMAGE_MESH_INPUT_BINDINGS,
    .vertexAttributeDescriptionCount = std::size(IMAGE_MESH_VERTEX_ATTRIBS),
    .pVertexAttributeDescriptions = IMAGE_MESH_VERTEX_ATTRIBS,
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

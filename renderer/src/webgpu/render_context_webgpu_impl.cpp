/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"

#include "rive/decoders/astc_footprints.hpp"

#include "rive/renderer/draw.hpp"
#ifdef RIVE_CANVAS
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_context_wgpu.hpp"
#endif
#include "rive/renderer/stack_vector.hpp"
#include "rive/gpu_texture_format.hpp"
#include "shaders/constants.glsl"

#include <sstream>
#include <string>

#include "generated/shaders/wgsl/blit_texture_as_draw_filtered.webgpu_vert.hpp"
#include "generated/shaders/wgsl/blit_texture_as_draw_filtered.webgpu_frag.hpp"
#include "generated/shaders/wgsl/color_ramp.vert.hpp"
#include "generated/shaders/wgsl/color_ramp.frag.hpp"
#include "generated/shaders/wgsl/tessellate.webgpu_vert.hpp"
#include "generated/shaders/wgsl/tessellate.webgpu_nossbo_vert.hpp"
#include "generated/shaders/wgsl/tessellate.webgpu_frag.hpp"
#include "generated/shaders/wgsl/render_atlas.webgpu_vert.hpp"
#include "generated/shaders/wgsl/render_atlas.webgpu_nossbo_vert.hpp"
#include "generated/shaders/wgsl/render_atlas_fill.webgpu_frag.hpp"
#include "generated/shaders/wgsl/render_atlas_stroke.webgpu_frag.hpp"

// InterlockMode::atomics shaders.
#include "generated/shaders/wgsl/atomic_draw_path.webgpu_vert.hpp"
#include "generated/shaders/wgsl/atomic_draw_path.webgpu_frag.hpp"
#include "generated/shaders/wgsl/atomic_draw_path.webgpu_fixedcolor_frag.hpp"
#include "generated/shaders/wgsl/atomic_draw_interior_triangles.webgpu_vert.hpp"
#include "generated/shaders/wgsl/atomic_draw_interior_triangles.webgpu_frag.hpp"
#include "generated/shaders/wgsl/atomic_draw_interior_triangles.webgpu_fixedcolor_frag.hpp"
#include "generated/shaders/wgsl/atomic_draw_atlas_blit.webgpu_vert.hpp"
#include "generated/shaders/wgsl/atomic_draw_atlas_blit.webgpu_frag.hpp"
#include "generated/shaders/wgsl/atomic_draw_atlas_blit.webgpu_fixedcolor_frag.hpp"
#include "generated/shaders/wgsl/atomic_draw_image_rect.webgpu_vert.hpp"
#include "generated/shaders/wgsl/atomic_draw_image_rect.webgpu_frag.hpp"
#include "generated/shaders/wgsl/atomic_draw_image_rect.webgpu_fixedcolor_frag.hpp"
#include "generated/shaders/wgsl/atomic_draw_image_mesh.webgpu_vert.hpp"
#include "generated/shaders/wgsl/atomic_draw_image_mesh.webgpu_frag.hpp"
#include "generated/shaders/wgsl/atomic_draw_image_mesh.webgpu_fixedcolor_frag.hpp"
#include "generated/shaders/wgsl/atomic_resolve.webgpu_vert.hpp"
#include "generated/shaders/wgsl/atomic_resolve.webgpu_fixedcolor_frag.hpp"
#include "generated/shaders/wgsl/atomic_resolve_coalesced.webgpu_vert.hpp"
#include "generated/shaders/wgsl/atomic_resolve_coalesced.webgpu_frag.hpp"
#include "generated/shaders/wgsl/atomic_init.webgpu_vert.hpp"
#include "generated/shaders/wgsl/atomic_init.webgpu_frag.hpp"
#include "generated/shaders/wgsl/atomic_init.webgpu_fixedcolor_frag.hpp"

// InterlockMode::msaa shaders.
#include "generated/shaders/wgsl/draw_msaa_path.webgpu_vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_path.webgpu_noclipdistance_vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_path.webgpu_nossbo_vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_path.webgpu_nossbo_noclipdistance_vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_path.webgpu_frag.hpp"
#include "generated/shaders/wgsl/draw_msaa_path.webgpu_fixedcolor_frag.hpp"
#include "generated/shaders/wgsl/draw_msaa_atlas_blit.webgpu_vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_atlas_blit.webgpu_noclipdistance_vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_atlas_blit.webgpu_nossbo_vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_atlas_blit.webgpu_nossbo_noclipdistance_vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_atlas_blit.webgpu_frag.hpp"
#include "generated/shaders/wgsl/draw_msaa_atlas_blit.webgpu_fixedcolor_frag.hpp"
#include "generated/shaders/wgsl/draw_msaa_image_mesh.webgpu_vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_image_mesh.webgpu_noclipdistance_vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_image_mesh.webgpu_frag.hpp"
#include "generated/shaders/wgsl/draw_msaa_image_mesh.webgpu_fixedcolor_frag.hpp"
#include "generated/shaders/wgsl/draw_msaa_stencil.vert.hpp"
#include "generated/shaders/wgsl/draw_msaa_stencil.frag.hpp"

#ifdef RIVE_DAWN
#include <dawn/webgpu_cpp.h>
#endif

#ifdef RIVE_WEBGPU
#include <webgpu/webgpu_cpp.h>
#if RIVE_WEBGPU == 1
#include "webgpu_compat.h"
#endif
#endif

#if RIVE_WEBGPU == 1
namespace wgpu
{
using TexelCopyBufferInfo = ImageCopyBuffer;
using TexelCopyTextureInfo = ImageCopyTexture;
using TexelCopyBufferLayout = TextureDataLayout;
using ShaderSourceWGSL = ShaderModuleWGSLDescriptor;
}; // namespace wgpu

static WGPUBool wgpu_bool(bool value) { return static_cast<WGPUBool>(value); }
#else
#define WGPU_STRING_VIEW(s)                                                    \
    {                                                                          \
        .data = s,                                                             \
        .length = strlen(s),                                                   \
    }

static WGPUOptionalBool wgpu_bool(bool value)
{
    return value ? WGPUOptionalBool_True : WGPUOptionalBool_False;
}
#endif

constexpr static auto RIVE_FRONT_FACE = wgpu::FrontFace::CW;

constexpr static uint32_t MSAA_SAMPLE_COUNT = 4u;

constexpr static WGPUBlendComponent BLEND_COMPONENT_SRC_OVER = {
    .operation = WGPUBlendOperation_Add,
    .srcFactor = WGPUBlendFactor_One,
    .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
};

constexpr static WGPUBlendState BLEND_STATE_SRC_OVER = {
    .color = BLEND_COMPONENT_SRC_OVER,
    .alpha = BLEND_COMPONENT_SRC_OVER,
};

constexpr static WGPUStencilFaceState STENCIL_FACE_STATE_DISABLED = {
    .compare = WGPUCompareFunction_Always,
    .failOp = WGPUStencilOperation_Keep,
    .depthFailOp = WGPUStencilOperation_Keep,
    .passOp = WGPUStencilOperation_Keep,
};

static WGPUCullMode wgpu_cull_mode(rive::gpu::CullFace face)
{
    switch (face)
    {
        case rive::gpu::CullFace::none:
            return WGPUCullMode_None;
        case rive::gpu::CullFace::clockwise:
            return WGPUCullMode_Front;
        case rive::gpu::CullFace::counterclockwise:
            return WGPUCullMode_Back;
    }
    RIVE_UNREACHABLE();
}

static WGPUCompareFunction wgpu_compare_function(rive::gpu::StencilCompareOp op)
{
    switch (op)
    {
        case rive::gpu::StencilCompareOp::less:
            return WGPUCompareFunction_Less;
        case rive::gpu::StencilCompareOp::equal:
            return WGPUCompareFunction_Equal;
        case rive::gpu::StencilCompareOp::lessOrEqual:
            return WGPUCompareFunction_LessEqual;
        case rive::gpu::StencilCompareOp::notEqual:
            return WGPUCompareFunction_NotEqual;
        case rive::gpu::StencilCompareOp::always:
            return WGPUCompareFunction_Always;
    }
    RIVE_UNREACHABLE();
};

static WGPUStencilOperation wgpu_stencil_operation(rive::gpu::StencilOp op)
{
    switch (op)
    {
        case rive::gpu::StencilOp::keep:
            return WGPUStencilOperation_Keep;
        case rive::gpu::StencilOp::replace:
            return WGPUStencilOperation_Replace;
        case rive::gpu::StencilOp::zero:
            return WGPUStencilOperation_Zero;
        case rive::gpu::StencilOp::decrClamp:
            return WGPUStencilOperation_DecrementClamp;
        case rive::gpu::StencilOp::incrWrap:
            return WGPUStencilOperation_IncrementWrap;
        case rive::gpu::StencilOp::decrWrap:
            return WGPUStencilOperation_DecrementWrap;
    };
    RIVE_UNREACHABLE();
}

static WGPUStencilFaceState wgpu_stencil_face_state(
    rive::gpu::StencilFaceOps face)
{
    return {
        .compare = wgpu_compare_function(face.compareOp),
        .failOp = wgpu_stencil_operation(face.failOp),
        .depthFailOp = wgpu_stencil_operation(face.depthFailOp),
        .passOp = wgpu_stencil_operation(face.passOp),
    };
}

static wgpu::Color wgpu_color_premul(rive::ColorInt riveColor)
{
    float premul[4];
    rive::UnpackColorToRGBA32FPremul(riveColor, premul);
    return {premul[0], premul[1], premul[2], premul[3]};
}

static wgpu::AddressMode webgpu_address_mode(rive::ImageWrap wrap)
{
    switch (wrap)
    {
        case rive::ImageWrap::clamp:
            return wgpu::AddressMode::ClampToEdge;
        case rive::ImageWrap::repeat:
            return wgpu::AddressMode::Repeat;
        case rive::ImageWrap::mirror:
            return wgpu::AddressMode::MirrorRepeat;
    }
    RIVE_UNREACHABLE();
}

static wgpu::FilterMode webgpu_filter_mode(rive::ImageFilter filter)
{
    switch (filter)
    {
        case rive::ImageFilter::bilinear:
            return wgpu::FilterMode::Linear;
        case rive::ImageFilter::nearest:
            return wgpu::FilterMode::Nearest;
    }
    RIVE_UNREACHABLE();
}

static wgpu::MipmapFilterMode webgpu_mipmap_filter_mode(
    rive::ImageFilter filter)
{
    switch (filter)
    {
        case rive::ImageFilter::nearest:
        case rive::ImageFilter::bilinear:
            return wgpu::MipmapFilterMode::Nearest;
    }
    RIVE_UNREACHABLE();
}

static wgpu::ShaderModule compile_shader_module_wgsl(
    wgpu::Device device,
    const rive::gpu::wgsl::Shader& shader)
{
    wgpu::ShaderSourceWGSL source;
    source.code = shader.source;
    wgpu::ShaderModuleDescriptor descriptor = {
        .nextInChain = &source,
    };
    descriptor.label = shader.label;

    return device.CreateShaderModule(&descriptor);
}

#ifdef RIVE_WAGYU
#include "rive/renderer/gl/load_store_actions_ext.hpp"

#include <webgpu/webgpu_wagyu.h>

#include "generated/shaders/glsl.glsl.hpp"
#include "generated/shaders/constants.glsl.hpp"
#include "generated/shaders/flush_uniforms.glsl.hpp"
#include "generated/shaders/common.glsl.hpp"
#include "generated/shaders/color_ramp.glsl.hpp"
#include "generated/shaders/bezier_utils.glsl.hpp"
#include "generated/shaders/tessellate.glsl.hpp"
#include "generated/shaders/render_atlas.glsl.hpp"
#include "generated/shaders/advanced_blend.glsl.hpp"
#include "generated/shaders/draw_path_common.glsl.hpp"
#include "generated/shaders/draw_path.vert.hpp"
#include "generated/shaders/draw_raster_order_path.frag.hpp"
#include "generated/shaders/draw_clockwise_path.frag.hpp"
#include "generated/shaders/draw_clockwise_clip.frag.hpp"
#include "generated/shaders/draw_image_mesh.vert.hpp"
#include "generated/shaders/draw_mesh.frag.hpp"

// When compiling "glslRaw" shaders, the WebGPU driver will automatically search
// for a uniform with this name and update its value when draw commands have a
// base instance.
constexpr static char BASE_INSTANCE_UNIFORM_NAME[] = "nrdp_BaseInstance";

static wgpu::ShaderModule compile_shader_module_wagyu(
    wgpu::Device device,
    const char* source,
    WGPUWagyuShaderLanguage language)
{
    WGPUChainedStruct* chainedShaderModuleDescriptor = nullptr;
    WGPUWagyuShaderModuleDescriptor wagyuDesc =
        WGPU_WAGYU_SHADER_MODULE_DESCRIPTOR_INIT;
    wagyuDesc.codeSize = strlen(source);
    wagyuDesc.code = source;
    wagyuDesc.language = language;
    chainedShaderModuleDescriptor = &wagyuDesc.chain;

    WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    descriptor.nextInChain = chainedShaderModuleDescriptor;
    auto ret = wgpuDeviceCreateShaderModule(device.Get(), &descriptor);
    return wgpu::ShaderModule::Acquire(ret);
}

static bool using_pls(rive::gpu::InterlockMode interlockMode)
{
    switch (interlockMode)
    {
        case rive::gpu::InterlockMode::rasterOrdering:
        case rive::gpu::InterlockMode::clockwise:
            return true;
        case rive::gpu::InterlockMode::atomics:
        case rive::gpu::InterlockMode::clockwiseAtomic:
        case rive::gpu::InterlockMode::msaa:
            return false;
    }
    RIVE_UNREACHABLE();
}
#endif // RIVE_WAGYU

namespace rive::gpu
{
#ifdef RIVE_WAGYU
// Draws emulated render-pass load/store actions for
// EXT_shader_pixel_local_storage.
class RenderContextWebGPUImpl::LoadStoreEXTPipeline
{
public:
    constexpr static uint32_t Key(LoadStoreActionsEXT actions,
                                  wgpu::TextureFormat framebufferFormat)
    {
        uint32_t key = static_cast<uint32_t>(framebufferFormat);

        assert(key << LOAD_STORE_ACTIONS_EXT_COUNT >>
                   LOAD_STORE_ACTIONS_EXT_COUNT ==
               key);
        assert(static_cast<uint32_t>(actions) <
               1 << LOAD_STORE_ACTIONS_EXT_COUNT);
        key = (key << LOAD_STORE_ACTIONS_EXT_COUNT) |
              static_cast<uint32_t>(actions);

        return key;
    }

    LoadStoreEXTPipeline(RenderContextWebGPUImpl* context,
                         LoadStoreActionsEXT actions,
                         wgpu::TextureFormat framebufferFormat) :
        m_framebufferFormat(framebufferFormat)
    {
        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc;
        if (enums::is_flag_set(actions, LoadStoreActionsEXT::clearColor))
        {
            // Create a uniform buffer binding for the clear color.
            wgpu::BindGroupLayoutEntry bindingLayouts[] = {
                {
                    .binding = 0,
                    .visibility = wgpu::ShaderStage::Fragment,
                    .buffer =
                        {
                            .type = wgpu::BufferBindingType::Uniform,
                        },
                },
            };

            wgpu::BindGroupLayoutDescriptor bindingsDesc = {
                .entryCount = std::size(bindingLayouts),
                .entries = bindingLayouts,
            };

            m_bindGroupLayout =
                context->m_device.CreateBindGroupLayout(&bindingsDesc);

            pipelineLayoutDesc = {
                .bindGroupLayoutCount = 1,
                .bindGroupLayouts = &m_bindGroupLayout,
            };
        }
        else
        {
            pipelineLayoutDesc = {
                .bindGroupLayoutCount = 0,
                .bindGroupLayouts = nullptr,
            };
        }

        wgpu::PipelineLayout pipelineLayout =
            context->m_device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule fragmentShaderModule;
        std::ostringstream glsl;
        glsl << "#version 310 es\n";
        glsl << "#define " GLSL_FRAGMENT " true\n";
        glsl << "#define " GLSL_ENABLE_CLIPPING " true\n";
        BuildLoadStoreEXTGLSL(glsl, actions);
        fragmentShaderModule =
            compile_shader_module_wagyu(context->m_device,
                                        glsl.str().c_str(),
                                        WGPUWagyuShaderLanguage_GLSLRAW);

        wgpu::ColorTargetState colorTargetState = {
            .format = framebufferFormat,
        };

        wgpu::FragmentState fragmentState = {
            .module = fragmentShaderModule,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .label = "RIVE_LoadStoreEXTPipeline",
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = context->m_loadStoreEXTVertexShader,
                    .entryPoint = "main",
                    .bufferCount = 0,
                    .buffers = nullptr,
                },
            .primitive =
                {
                    .topology = wgpu::PrimitiveTopology::TriangleStrip,
                    .frontFace = RIVE_FRONT_FACE,
                    .cullMode = wgpu::CullMode::None,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = context->m_device.CreateRenderPipeline(&desc);
    }

    const wgpu::BindGroupLayout& bindGroupLayout() const
    {
        assert(m_bindGroupLayout); // We only have a bind group if there is a
                                   // clear color.
        return m_bindGroupLayout;
    }

    wgpu::RenderPipeline renderPipeline(
        wgpu::TextureFormat framebufferFormat) const
    {
        assert(framebufferFormat == m_framebufferFormat);
        return m_renderPipeline;
    }

private:
    const wgpu::TextureFormat m_framebufferFormat RIVE_MAYBE_UNUSED;
    wgpu::BindGroupLayout m_bindGroupLayout;
    wgpu::RenderPipeline m_renderPipeline;
};
#endif

// Lazily-built wgpu::PipelineLayouts for Rive draw pipelines. The PLS bindings
// in the layout differ based on the interlockMode, so these are keyed off
// interlockMode.
class RenderContextWebGPUImpl::DrawPipelineLayout
{
public:
    DrawPipelineLayout(RenderContextWebGPUImpl* impl,
                       gpu::InterlockMode interlockMode)
    {
        wgpu::ShaderStage pathStorageVisibility, paintStorageVisibility;
        if (interlockMode == gpu::InterlockMode::atomics)
        {
            pathStorageVisibility =
                wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
            paintStorageVisibility = wgpu::ShaderStage::Fragment;
        }
        else
        {
            pathStorageVisibility = paintStorageVisibility =
                wgpu::ShaderStage::Vertex;
        }
        m_perFlushBindingLayoutEntries = {{
            {
                .binding = FLUSH_UNIFORM_BUFFER_IDX,
                .visibility =
                    wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
                .buffer =
                    {
                        .type = wgpu::BufferBindingType::Uniform,
                    },
            },
            impl->m_capabilities.polyfillVertexStorageBuffers ?
                wgpu::BindGroupLayoutEntry{
                    .binding = PATH_BUFFER_IDX,
                    .visibility = pathStorageVisibility,
                    .texture =
                        {
                            .sampleType = wgpu::TextureSampleType::Uint,
                            .viewDimension = wgpu::TextureViewDimension::e2D,
                        },
                } :
                wgpu::BindGroupLayoutEntry{
                    .binding = PATH_BUFFER_IDX,
                    .visibility = pathStorageVisibility,
                    .buffer =
                        {
                            .type = wgpu::BufferBindingType::ReadOnlyStorage,
                        },
                },
            impl->m_capabilities.polyfillVertexStorageBuffers ?
                wgpu::BindGroupLayoutEntry{
                    .binding = PAINT_BUFFER_IDX,
                    .visibility = paintStorageVisibility,
                    .texture =
                        {
                            .sampleType = wgpu::TextureSampleType::Uint,
                            .viewDimension = wgpu::TextureViewDimension::e2D,
                        },
                } :
                wgpu::BindGroupLayoutEntry{
                    .binding = PAINT_BUFFER_IDX,
                    .visibility = paintStorageVisibility,
                    .buffer =
                        {
                            .type = wgpu::BufferBindingType::ReadOnlyStorage,
                        },
                },
            impl->m_capabilities.polyfillVertexStorageBuffers ?
                wgpu::BindGroupLayoutEntry{
                    .binding = PAINT_AUX_BUFFER_IDX,
                    .visibility = paintStorageVisibility,
                    .texture =
                        {
                            .sampleType =
                                wgpu::TextureSampleType::UnfilterableFloat,
                            .viewDimension = wgpu::TextureViewDimension::e2D,
                        },
                } :
                wgpu::BindGroupLayoutEntry{
                    .binding = PAINT_AUX_BUFFER_IDX,
                    .visibility = paintStorageVisibility,
                    .buffer =
                        {
                            .type = wgpu::BufferBindingType::ReadOnlyStorage,
                        },
                },
            impl->m_capabilities.polyfillVertexStorageBuffers ?
                wgpu::BindGroupLayoutEntry{
                    .binding = CONTOUR_BUFFER_IDX,
                    .visibility = wgpu::ShaderStage::Vertex,
                    .texture =
                        {
                            .sampleType = wgpu::TextureSampleType::Uint,
                            .viewDimension = wgpu::TextureViewDimension::e2D,
                        },
                } :
                wgpu::BindGroupLayoutEntry{
                    .binding = CONTOUR_BUFFER_IDX,
                    .visibility = wgpu::ShaderStage::Vertex,
                    .buffer =
                        {
                            .type = wgpu::BufferBindingType::ReadOnlyStorage,
                        },
                },
            {
                .binding = FEATHER_TEXTURE_IDX,
                .visibility =
                    wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Float,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            },
            {
                .binding = TESS_VERTEX_TEXTURE_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Uint,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            },
            {
                .binding = ATLAS_TEXTURE_IDX,
                .visibility = wgpu::ShaderStage::Fragment,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Float,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            },
            {
                .binding = GRAD_TEXTURE_IDX,
                .visibility = wgpu::ShaderStage::Fragment,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Float,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            },
            {
                .binding = DST_COLOR_TEXTURE_IDX,
                .visibility = wgpu::ShaderStage::Fragment,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Float,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            },
        }};
        static_assert(DRAW_BINDINGS_COUNT == 10);

        wgpu::BindGroupLayoutDescriptor perFlushBindingsDesc = {
            .entryCount = DRAW_BINDINGS_COUNT,
            .entries = m_perFlushBindingLayoutEntries.data(),
        };
        m_bindGroupLayouts[PER_FLUSH_BINDINGS_SET] =
            impl->m_device.CreateBindGroupLayout(&perFlushBindingsDesc);

        // Per-draw bindings: image texture + image sampler. Mode-independent.
        wgpu::BindGroupLayoutEntry perDrawBindingLayouts[] = {
            {
                .binding = IMAGE_TEXTURE_IDX,
                .visibility = wgpu::ShaderStage::Fragment,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Float,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            },
            {
                .binding = WEBGPU_IMAGE_SAMPLER_IDX,
                .visibility = wgpu::ShaderStage::Fragment,
                .sampler = {.type = wgpu::SamplerBindingType::Filtering},
            },
        };
        wgpu::BindGroupLayoutDescriptor perDrawBindingsDesc = {
            .entryCount = std::size(perDrawBindingLayouts),
            .entries = perDrawBindingLayouts,
        };
        m_bindGroupLayouts[PER_DRAW_BINDINGS_SET] =
            impl->m_device.CreateBindGroupLayout(&perDrawBindingsDesc);

        if (interlockMode == gpu::InterlockMode::atomics)
        {
            // Atomic mode binds three storage buffers (color + clip +
            // coverage). Color is only actually used by shaders compiled
            // without FIXED_FUNCTION_COLOR_OUTPUT.
            wgpu::BindGroupLayoutEntry plsAtomicBufferBindingLayouts[] = {
                {
                    .binding = COLOR_PLANE_IDX,
                    .visibility = wgpu::ShaderStage::Fragment,
                    .buffer = {.type = wgpu::BufferBindingType::Storage},
                },
                {
                    .binding = CLIP_PLANE_IDX,
                    .visibility = wgpu::ShaderStage::Fragment,
                    .buffer = {.type = wgpu::BufferBindingType::Storage},
                },
                {
                    .binding = COVERAGE_PLANE_IDX,
                    .visibility = wgpu::ShaderStage::Fragment,
                    .buffer = {.type = wgpu::BufferBindingType::Storage},
                },
            };
            static_assert(std::size(plsAtomicBufferBindingLayouts) == 3);

            wgpu::BindGroupLayoutDescriptor plsAtomicTextureBindingsDesc = {
                .entryCount = std::size(plsAtomicBufferBindingLayouts),
                .entries = plsAtomicBufferBindingLayouts,
            };

            m_bindGroupLayouts[PLS_TEXTURE_BINDINGS_SET] =
                impl->m_device.CreateBindGroupLayout(
                    &plsAtomicTextureBindingsDesc);
        }
#ifdef RIVE_WAGYU
        else if (interlockMode == gpu::InterlockMode::rasterOrdering &&
                 impl->m_capabilities.plsType ==
                     PixelLocalStorageType::
                         VK_EXT_rasterization_order_attachment_access)
        {
            WGPUWagyuInputTextureBindingLayout inputAttachmentLayout =
                WGPU_WAGYU_INPUT_TEXTURE_BINDING_LAYOUT_INIT;
            inputAttachmentLayout.viewDimension = WGPUTextureViewDimension_2D;

            WGPUBindGroupLayoutEntry inputAttachments[4];
            memset(inputAttachments, 0, sizeof(inputAttachments));
            for (uint32_t i = 0; i < std::size(inputAttachments); ++i)
            {
                inputAttachments[i].nextInChain = &inputAttachmentLayout.chain;
                inputAttachments[i].binding = i;
                inputAttachments[i].visibility = WGPUShaderStage_Fragment;
            }

            WGPUBindGroupLayoutDescriptor inputAttachmentsLayoutDesc =
                WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
            inputAttachmentsLayoutDesc.entryCount = std::size(inputAttachments);
            inputAttachmentsLayoutDesc.entries = inputAttachments;

            m_bindGroupLayouts[PLS_TEXTURE_BINDINGS_SET] =
                wgpu::BindGroupLayout::Acquire(wgpuDeviceCreateBindGroupLayout(
                    impl->m_device.Get(),
                    &inputAttachmentsLayoutDesc));
        }
#endif
        else
        {
            m_bindGroupLayouts[PLS_TEXTURE_BINDINGS_SET] =
                impl->m_emptyBindingsLayout;
        }

        // Sampler bindings. Mode-independent.
        wgpu::BindGroupLayoutEntry drawBindingSamplerLayouts[] = {
            {
                .binding = GRAD_TEXTURE_IDX,
                .visibility = wgpu::ShaderStage::Fragment,
                .sampler = {.type = wgpu::SamplerBindingType::Filtering},
            },
            {
                .binding = FEATHER_TEXTURE_IDX,
                .visibility =
                    wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
                .sampler = {.type = wgpu::SamplerBindingType::Filtering},
            },
            {
                .binding = ATLAS_TEXTURE_IDX,
                .visibility = wgpu::ShaderStage::Fragment,
                .sampler = {.type = wgpu::SamplerBindingType::Filtering},
            },
        };
        wgpu::BindGroupLayoutDescriptor samplerBindingsDesc = {
            .entryCount = std::size(drawBindingSamplerLayouts),
            .entries = drawBindingSamplerLayouts,
        };
        m_bindGroupLayouts[WEBGPU_SAMPLER_BINDINGS_SET] =
            impl->m_device.CreateBindGroupLayout(&samplerBindingsDesc);

        // Draw pipeline layout for this interlockMode.
        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount =
                static_cast<size_t>(WEBGPU_BINDINGS_SET_COUNT),
            .bindGroupLayouts = m_bindGroupLayouts.data(),
        };
        m_pipelineLayout =
            impl->m_device.CreatePipelineLayout(&pipelineLayoutDesc);
    }

    const wgpu::BindGroupLayoutEntry* perFlushBindingLayoutEntries() const
    {
        return m_perFlushBindingLayoutEntries.data();
    }
    const wgpu::BindGroupLayout& bindGroupLayout(size_t set) const
    {
        return m_bindGroupLayouts[set];
    }
    wgpu::PipelineLayout pipelineLayout() const { return m_pipelineLayout; }

private:
    std::array<wgpu::BindGroupLayoutEntry, DRAW_BINDINGS_COUNT>
        m_perFlushBindingLayoutEntries;
    std::array<wgpu::BindGroupLayout, WEBGPU_BINDINGS_SET_COUNT>
        m_bindGroupLayouts;
    wgpu::PipelineLayout m_pipelineLayout;
};

const RenderContextWebGPUImpl::DrawPipelineLayout& RenderContextWebGPUImpl::
    drawPipelineLayout(gpu::InterlockMode mode)
{
    auto idx = static_cast<size_t>(mode);
    assert(idx < m_drawPipelineLayouts.size());
    auto& slot = m_drawPipelineLayouts[idx];
    if (!slot)
    {
        slot = std::make_unique<DrawPipelineLayout>(this, mode);
    }
    return *slot;
}

// Renders color ramps to the gradient texture.
class RenderContextWebGPUImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(RenderContextWebGPUImpl* impl)
    {
        const wgpu::Device device = impl->device();

        wgpu::BindGroupLayoutDescriptor colorRampBindingsDesc = {
            .entryCount = COLOR_RAMP_BINDINGS_COUNT,
            .entries =
                impl->drawPipelineLayout(gpu::InterlockMode::rasterOrdering)
                    .perFlushBindingLayoutEntries(),
        };

        m_bindGroupLayout =
            device.CreateBindGroupLayout(&colorRampBindingsDesc);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &m_bindGroupLayout,
        };

        wgpu::PipelineLayout pipelineLayout =
            device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShaderModule, fragmentShaderModule;
#ifdef RIVE_WAGYU
        if (impl->m_capabilities.backendType == wgpu::BackendType::OpenGLES)
        {
            // Rive shaders tend to be long and prone to vendor bugs in the
            // compiler. Instead of wgsl, send down the raw Rive GLSL sources,
            // which have various workarounds for known issues and are tested
            // regularly.
            std::ostringstream glsl;
            glsl << "#define " << GLSL_POST_INVERT_Y << " true\n";
            glsl << glsl::glsl << "\n";
            glsl << glsl::constants << "\n";
            glsl << glsl::flush_uniforms << '\n';
            glsl << glsl::common << "\n";
            glsl << glsl::color_ramp << "\n";

            std::ostringstream vertexGLSL;
            vertexGLSL << "#version 310 es\n";
            vertexGLSL << "#define " GLSL_VERTEX " true\n";
            vertexGLSL << glsl.str();
            vertexShaderModule =
                compile_shader_module_wagyu(device,
                                            vertexGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);

            std::ostringstream fragmentGLSL;
            fragmentGLSL << "#version 310 es\n";
            fragmentGLSL << "#define " GLSL_FRAGMENT " true\n";
            fragmentGLSL << glsl.str();
            fragmentShaderModule =
                compile_shader_module_wagyu(device,
                                            fragmentGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);
        }
        else
#endif
        {
            vertexShaderModule =
                compile_shader_module_wgsl(device, wgsl::color_ramp_vert);
            fragmentShaderModule =
                compile_shader_module_wgsl(device, wgsl::color_ramp_frag);
        }

        wgpu::VertexAttribute attrs[] = {
            {
                .format = wgpu::VertexFormat::Uint32x4,
                .offset = 0,
                .shaderLocation = 0,
            },
        };

        wgpu::VertexBufferLayout vertexBufferLayout = {
            .attributeCount = std::size(attrs),
            .attributes = attrs,
        };
        // arrayStride and stepMode are defined in different orders in webgpu1
        // vs webgpu2, so can't be in the designated initializer.
        vertexBufferLayout.arrayStride = sizeof(gpu::GradientSpan);
        vertexBufferLayout.stepMode = wgpu::VertexStepMode::Instance;

        wgpu::ColorTargetState colorTargetState = {
            .format = wgpu::TextureFormat::RGBA8Unorm,
        };

        wgpu::FragmentState fragmentState = {
            .module = fragmentShaderModule,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .label = "RIVE_ColorRampPipeline",
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = vertexShaderModule,
                    .entryPoint = "main",
                    .bufferCount = 1,
                    .buffers = &vertexBufferLayout,
                },
            .primitive =
                {
                    .topology = wgpu::PrimitiveTopology::TriangleStrip,
                    .frontFace = RIVE_FRONT_FACE,
                    .cullMode = wgpu::CullMode::None,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = device.CreateRenderPipeline(&desc);
    }

    const wgpu::BindGroupLayout& bindGroupLayout() const
    {
        return m_bindGroupLayout;
    }
    wgpu::RenderPipeline renderPipeline() const { return m_renderPipeline; }

private:
    wgpu::BindGroupLayout m_bindGroupLayout;
    wgpu::RenderPipeline m_renderPipeline;
};

// Renders tessellated vertices to the tessellation texture.
class RenderContextWebGPUImpl::TessellatePipeline
{
public:
    TessellatePipeline(RenderContextWebGPUImpl* impl)
    {
        const wgpu::Device device = impl->device();
        const auto& drawPipelineLayout =
            impl->drawPipelineLayout(gpu::InterlockMode::rasterOrdering);

        wgpu::BindGroupLayoutDescriptor perFlushBindingsDesc = {
            .entryCount = TESS_BINDINGS_COUNT,
            .entries = drawPipelineLayout.perFlushBindingLayoutEntries(),
        };

        m_perFlushBindingsLayout =
            device.CreateBindGroupLayout(&perFlushBindingsDesc);

        wgpu::BindGroupLayout layouts[] = {
            m_perFlushBindingsLayout,
            impl->m_emptyBindingsLayout,
            impl->m_emptyBindingsLayout,
            drawPipelineLayout.bindGroupLayout(WEBGPU_SAMPLER_BINDINGS_SET),
        };

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = std::size(layouts),
            .bindGroupLayouts = layouts,
        };

        wgpu::PipelineLayout pipelineLayout =
            device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShaderModule, fragmentShaderModule;
#ifdef RIVE_WAGYU
        if (impl->m_capabilities.backendType == wgpu::BackendType::OpenGLES)
        {
            // Rive shaders tend to be long and prone to vendor bugs in the
            // compiler. Instead of wgsl, send down the raw Rive GLSL sources,
            // which have various workarounds for known issues and are tested
            // regularly.
            std::ostringstream glsl;
            if (impl->m_capabilities.polyfillVertexStorageBuffers)
            {
                glsl << "#define " GLSL_DISABLE_SHADER_STORAGE_BUFFERS
                        " true\n";
            }
            glsl << "#define " << GLSL_POST_INVERT_Y << " true\n";
            glsl << glsl::glsl << "\n";
            glsl << glsl::constants << "\n";
            glsl << glsl::flush_uniforms << "\n";
            glsl << glsl::common << "\n";
            glsl << glsl::bezier_utils << "\n";
            glsl << glsl::tessellate << "\n";

            std::ostringstream vertexGLSL;
            vertexGLSL << "#version 310 es\n";
            vertexGLSL << "#define " GLSL_VERTEX " true\n";
            vertexGLSL << glsl.str();
            vertexShaderModule =
                compile_shader_module_wagyu(device,
                                            vertexGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);

            std::ostringstream fragmentGLSL;
            fragmentGLSL << "#version 310 es\n";
            fragmentGLSL << "#define " GLSL_FRAGMENT " true\n";
            fragmentGLSL << glsl.str();
            fragmentShaderModule =
                compile_shader_module_wagyu(device,
                                            fragmentGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);
        }
        else
#endif
        {
            vertexShaderModule = compile_shader_module_wgsl(
                device,
                impl->m_capabilities.polyfillVertexStorageBuffers
                    ? wgsl::tessellate_webgpu_nossbo_vert
                    : wgsl::tessellate_webgpu_vert);
            fragmentShaderModule =
                compile_shader_module_wgsl(device,
                                           wgsl::tessellate_webgpu_frag);
        }

        wgpu::VertexAttribute attrs[] = {
            {
                .format = wgpu::VertexFormat::Float32x4,
                .offset = 0,
                .shaderLocation = 0,
            },
            {
                .format = wgpu::VertexFormat::Float32x4,
                .offset = 4 * sizeof(float),
                .shaderLocation = 1,
            },
            {
                .format = wgpu::VertexFormat::Float32x4,
                .offset = 8 * sizeof(float),
                .shaderLocation = 2,
            },
            {
                .format = wgpu::VertexFormat::Uint32x4,
                .offset = 12 * sizeof(float),
                .shaderLocation = 3,
            },
        };

        wgpu::VertexBufferLayout vertexBufferLayout = {
            .attributeCount = std::size(attrs),
            .attributes = attrs,
        };
        // arrayStride and stepMode are defined in different orders in webgpu1
        // vs webgpu2, so can't be in the designated initializer.
        vertexBufferLayout.arrayStride = sizeof(gpu::TessVertexSpan);
        vertexBufferLayout.stepMode = wgpu::VertexStepMode::Instance;

        wgpu::ColorTargetState colorTargetState = {
            .format = wgpu::TextureFormat::RGBA32Uint,
        };

        wgpu::FragmentState fragmentState = {
            .module = fragmentShaderModule,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .label = "RIVE_TessellatePipeline",
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = vertexShaderModule,
                    .entryPoint = "main",
                    .bufferCount = 1,
                    .buffers = &vertexBufferLayout,
                },
            .primitive =
                {
                    .topology = wgpu::PrimitiveTopology::TriangleList,
                    .frontFace = RIVE_FRONT_FACE,
                    .cullMode = wgpu::CullMode::None,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = device.CreateRenderPipeline(&desc);
    }

    wgpu::BindGroupLayout perFlushBindingsLayout() const
    {
        return m_perFlushBindingsLayout;
    }
    wgpu::RenderPipeline renderPipeline() const { return m_renderPipeline; }

private:
    wgpu::BindGroupLayout m_perFlushBindingsLayout;
    wgpu::RenderPipeline m_renderPipeline;
};

// Renders tessellated vertices to the tessellation texture.
class RenderContextWebGPUImpl::AtlasPipeline
{
public:
    AtlasPipeline(RenderContextWebGPUImpl* impl)
    {
        const wgpu::Device device = impl->device();
        const auto& drawPipelineLayout =
            impl->drawPipelineLayout(gpu::InterlockMode::rasterOrdering);

        wgpu::BindGroupLayoutDescriptor perFlushBindingsDesc = {
            .entryCount = ATLAS_BINDINGS_COUNT,
            .entries = drawPipelineLayout.perFlushBindingLayoutEntries(),
        };

        m_perFlushBindingsLayout =
            device.CreateBindGroupLayout(&perFlushBindingsDesc);

        wgpu::BindGroupLayout layouts[] = {
            m_perFlushBindingsLayout,
            impl->m_emptyBindingsLayout,
            impl->m_emptyBindingsLayout,
            drawPipelineLayout.bindGroupLayout(WEBGPU_SAMPLER_BINDINGS_SET),
        };

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = std::size(layouts),
            .bindGroupLayouts = layouts,
        };

        wgpu::PipelineLayout pipelineLayout =
            device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShaderModule;
        wgpu::ShaderModule fillFragmentShaderModule, strokeFragmentShaderModule;
#ifdef RIVE_WAGYU
        if (impl->m_capabilities.backendType == wgpu::BackendType::OpenGLES)
        {
            // Rive shaders tend to be long and prone to vendor bugs in the
            // compiler. Instead of wgsl, send down the raw Rive GLSL sources,
            // which have various workarounds for known issues and are tested
            // regularly.
            std::ostringstream glsl;
            glsl << "#define " << GLSL_DRAW_PATH << " true\n";
            glsl << "#define " << GLSL_ENABLE_FEATHER << " true\n";
            glsl << "#define " << GLSL_ENABLE_INSTANCE_INDEX << " true\n";
            glsl << "#define " << GLSL_BASE_INSTANCE_UNIFORM_NAME << ' '
                 << BASE_INSTANCE_UNIFORM_NAME << '\n';
            glsl << "#define " << GLSL_POST_INVERT_Y << " true\n";
            if (impl->m_capabilities.polyfillVertexStorageBuffers)
            {
                glsl << "#define " GLSL_DISABLE_SHADER_STORAGE_BUFFERS
                        " true\n";
            }
            glsl << glsl::glsl << '\n';
            glsl << glsl::constants << '\n';
            glsl << glsl::flush_uniforms << '\n';
            glsl << glsl::common << '\n';
            glsl << glsl::draw_path_common << '\n';
            glsl << glsl::render_atlas << '\n';

            std::ostringstream vertexGLSL;
            vertexGLSL << "#version 310 es\n";
            vertexGLSL << "#pragma shader_stage(vertex)\n";
            vertexGLSL << "#define " GLSL_VERTEX " true\n";
            vertexGLSL << glsl.str();
            vertexShaderModule =
                compile_shader_module_wagyu(device,
                                            vertexGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);

            std::ostringstream fillGLSL;
            fillGLSL << "#version 310 es\n";
            fillGLSL << "#pragma shader_stage(fragment)\n";
            fillGLSL << "#define " GLSL_FRAGMENT " true\n";
            fillGLSL << "#define " GLSL_ATLAS_FEATHERED_FILL " true\n";
            fillGLSL << glsl.str();
            fillFragmentShaderModule =
                compile_shader_module_wagyu(device,
                                            fillGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);

            std::ostringstream strokeGLSL;
            strokeGLSL << "#version 310 es\n";
            strokeGLSL << "#pragma shader_stage(fragment)\n";
            strokeGLSL << "#define " GLSL_FRAGMENT " true\n";
            strokeGLSL << "#define " GLSL_ATLAS_FEATHERED_STROKE " true\n";
            strokeGLSL << glsl.str();
            strokeFragmentShaderModule =
                compile_shader_module_wagyu(device,
                                            strokeGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);
        }
        else
#endif
        {
            vertexShaderModule = compile_shader_module_wgsl(
                device,
                impl->m_capabilities.polyfillVertexStorageBuffers
                    ? wgsl::render_atlas_webgpu_nossbo_vert
                    : wgsl::render_atlas_webgpu_vert);
            fillFragmentShaderModule =
                compile_shader_module_wgsl(device,
                                           wgsl::render_atlas_fill_webgpu_frag);
            strokeFragmentShaderModule = compile_shader_module_wgsl(
                device,
                wgsl::render_atlas_stroke_webgpu_frag);
        }

        wgpu::VertexAttribute attrs[] = {
            {
                .format = wgpu::VertexFormat::Float32x4,
                .offset = 0,
                .shaderLocation = 0,
            },
            {
                .format = wgpu::VertexFormat::Float32x4,
                .offset = 4 * sizeof(float),
                .shaderLocation = 1,
            },
        };

        wgpu::VertexBufferLayout vertexBufferLayout = {
            .attributeCount = std::size(attrs),
            .attributes = attrs,
        };
        // arrayStride and stepMode are defined in different orders in webgpu1
        // vs webgpu2, so can't be in the designated initializer.
        vertexBufferLayout.arrayStride = sizeof(gpu::PatchVertex);
        vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

        wgpu::BlendState blendState = {
            .color = {
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::One,
                .dstFactor = wgpu::BlendFactor::One,
            }};

        wgpu::ColorTargetState colorTargetState = {
            .format = wgpu::TextureFormat::R16Float,
            .blend = &blendState,
        };

        wgpu::FragmentState fragmentState = {
            .module = fillFragmentShaderModule,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .label = "RIVE_AtlasPipeline",
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = vertexShaderModule,
                    .entryPoint = "main",
                    .bufferCount = 1,
                    .buffers = &vertexBufferLayout,
                },
            .primitive =
                {
                    .topology = wgpu::PrimitiveTopology::TriangleList,
                    .frontFace = RIVE_FRONT_FACE,
                    .cullMode = wgpu::CullMode::None,
                },
            .fragment = &fragmentState,
        };

        m_fillPipeline = device.CreateRenderPipeline(&desc);

        blendState.color.operation = wgpu::BlendOperation::Max;
        fragmentState.module = strokeFragmentShaderModule;
        m_strokePipeline = device.CreateRenderPipeline(&desc);
    }

    wgpu::BindGroupLayout perFlushBindingsLayout() const
    {
        return m_perFlushBindingsLayout;
    }
    wgpu::RenderPipeline fillPipeline() const { return m_fillPipeline; }
    wgpu::RenderPipeline strokePipeline() const { return m_strokePipeline; }

private:
    wgpu::BindGroupLayout m_perFlushBindingsLayout;
    wgpu::RenderPipeline m_fillPipeline;
    wgpu::RenderPipeline m_strokePipeline;
};

// Draw paths and image meshes using the gradient and tessellation textures.
class RenderContextWebGPUImpl::DrawPipeline
{
public:
    DrawPipeline(RenderContextWebGPUImpl* context,
                 DrawType drawType,
                 gpu::ShaderFeatures shaderFeatures,
                 gpu::InterlockMode interlockMode,
                 gpu::ShaderMiscFlags shaderMiscFlags,
                 const gpu::PipelineState& pipelineState,
                 bool targetIsGLFBO0)
    {
        const bool fixedFunctionColorOutput =
            enums::is_flag_set(shaderMiscFlags,
                               gpu::ShaderMiscFlags::fixedFunctionColorOutput);
        wgpu::ShaderModule vertexModule, fragmentModule;
        // Override info for the WGSL path. Stays nullptr on the WAGYU/GLSL
        // path, where permutations are baked in via #defines instead of
        // pipeline-overridable constants.
        const wgsl::Shader* vertexShader = nullptr;
        const wgsl::Shader* fragmentShader = nullptr;
        switch (interlockMode)
        {
            case gpu::InterlockMode::rasterOrdering:
            case gpu::InterlockMode::clockwise:
            {
#ifdef RIVE_WAGYU
                assert(using_pls(interlockMode));
                PixelLocalStorageType plsType = context->m_capabilities.plsType;
                WGPUWagyuShaderLanguage language;
                const char* versionString;
                std::ostringstream glsl;
                auto addDefine = [&glsl](const char* name) {
                    glsl << "#define " << name << " true\n";
                };
                if (context->m_capabilities.backendType ==
                    wgpu::BackendType::OpenGLES)
                {
                    language = WGPUWagyuShaderLanguage_GLSLRAW;
                    versionString = "#version 310 es";
                    if (!targetIsGLFBO0)
                    {
                        addDefine(GLSL_POST_INVERT_Y);
                    }
                    glsl << "#define " << GLSL_BASE_INSTANCE_UNIFORM_NAME << ' '
                         << BASE_INSTANCE_UNIFORM_NAME << '\n';
                }
                else
                {
                    language = WGPUWagyuShaderLanguage_GLSL;
                    versionString = "#version 460";
                    addDefine(GLSL_TARGET_SPIRV);
                }
                if (plsType ==
                    PixelLocalStorageType::GL_EXT_shader_pixel_local_storage)
                {
                    glsl << "#ifdef GL_EXT_shader_pixel_local_storage\n";
                    addDefine(GLSL_PLS_IMPL_EXT_NATIVE);
                    glsl << "#else\n";
                    // If we are being compiled by SPIRV transpiler for
                    // introspection, GL_EXT_shader_pixel_local_storage will
                    // not be defined.
                    glsl << "#extension GL_EXT_samplerless_texture_functions : "
                            "enable\n";
                    addDefine(GLSL_PLS_IMPL_NONE);
                    glsl << "#endif\n";
                }
                else
                {
                    glsl << "#extension GL_EXT_samplerless_texture_functions : "
                            "enable\n";
                    addDefine(
                        plsType ==
                                PixelLocalStorageType::
                                    VK_EXT_rasterization_order_attachment_access
                            ? GLSL_PLS_IMPL_SUBPASS_LOAD
                            : GLSL_PLS_IMPL_NONE);
                }
                if (context->m_capabilities.polyfillVertexStorageBuffers)
                {
                    addDefine(GLSL_DISABLE_SHADER_STORAGE_BUFFERS);
                }
                switch (drawType)
                {
                    case DrawType::midpointFanPatches:
                    case DrawType::midpointFanCenterAAPatches:
                    case DrawType::outerCurvePatches:
                        addDefine(GLSL_DRAW_PATH);
                        addDefine(GLSL_ENABLE_INSTANCE_INDEX);
                        break;
                    case DrawType::interiorTriangulation:
                        addDefine(GLSL_DRAW_INTERIOR_TRIANGLES);
                        break;
                    case DrawType::atlasBlit:
                        addDefine(GLSL_ATLAS_BLIT);
                        break;
                    case DrawType::imageRect:
                        addDefine(GLSL_DRAW_IMAGE);
                        addDefine(GLSL_DRAW_IMAGE_RECT);
                        RIVE_UNREACHABLE();
                        break;
                    case DrawType::imageMesh:
                        addDefine(GLSL_DRAW_IMAGE);
                        addDefine(GLSL_DRAW_IMAGE_MESH);
                        break;
                    case DrawType::msaaStrokes:
                    case DrawType::msaaMidpointFanBorrowedCoverage:
                    case DrawType::msaaMidpointFans:
                    case DrawType::msaaMidpointFanStencilReset:
                    case DrawType::msaaMidpointFanPathsStencil:
                    case DrawType::msaaMidpointFanPathsCover:
                    case DrawType::msaaOuterCubics:
                    case DrawType::clipReset:
                    case DrawType::renderPassInitialize:
                    case DrawType::renderPassResolve:
                        RIVE_UNREACHABLE();
                        break;
                }
                for (size_t i = 0; i < gpu::kShaderFeatureCount; ++i)
                {
                    const auto feature = ShaderFeatures(1 << i);
                    if (enums::is_flag_set(shaderFeatures, feature))
                    {
                        addDefine(GetShaderFeatureGLSLName(feature));
                    }
                }
                if (fixedFunctionColorOutput)
                {
                    addDefine(GLSL_FIXED_FUNCTION_COLOR_OUTPUT);
                }
                if (enums::is_flag_set(shaderMiscFlags,
                                       gpu::ShaderMiscFlags::clockwiseFill))
                {
                    addDefine(GLSL_CLOCKWISE_FILL);
                }
                if (enums::is_flag_set(
                        shaderMiscFlags,
                        gpu::ShaderMiscFlags::borrowedCoveragePass))
                {
                    addDefine(GLSL_BORROWED_COVERAGE_PASS);
                }
                glsl << gpu::glsl::glsl << '\n';
                glsl << gpu::glsl::constants << '\n';
                glsl << glsl::flush_uniforms << '\n';
                glsl << gpu::glsl::common << '\n';
                if (enums::is_flag_set(shaderFeatures,
                                       ShaderFeatures::ENABLE_ADVANCED_BLEND))
                {
                    glsl << gpu::glsl::advanced_blend << '\n';
                }
                glsl << "#define " << GLSL_OPTIONALLY_FLAT;
                if (!context->platformFeatures().avoidFlatVaryings)
                {
                    glsl << " flat";
                }
                glsl << '\n';
                switch (drawType)
                {
                    case DrawType::midpointFanPatches:
                    case DrawType::midpointFanCenterAAPatches:
                    case DrawType::outerCurvePatches:
                    case DrawType::interiorTriangulation:
                        glsl << gpu::glsl::draw_path_common << '\n';
                        glsl << gpu::glsl::draw_path_vert << '\n';
                        if (interlockMode == gpu::InterlockMode::rasterOrdering)
                        {
                            glsl << gpu::glsl::draw_raster_order_path_frag
                                 << '\n';
                        }
                        else
                        {
                            assert(interlockMode ==
                                   gpu::InterlockMode::clockwise);
                            glsl << (enums::is_flag_set(
                                         shaderMiscFlags,
                                         gpu::ShaderMiscFlags::clipUpdateOnly)
                                         ? gpu::glsl::draw_clockwise_clip_frag
                                         : gpu::glsl::draw_clockwise_path_frag)
                                 << '\n';
                        }
                        break;
                    case DrawType::atlasBlit:
                        glsl << gpu::glsl::draw_path_common << '\n';
                        glsl << gpu::glsl::draw_path_vert << '\n';
                        glsl << gpu::glsl::draw_mesh_frag << '\n';
                        break;
                    case DrawType::imageMesh:
                        glsl << gpu::glsl::draw_image_mesh_vert << '\n';
                        glsl << gpu::glsl::draw_mesh_frag << '\n';
                        break;
                    case DrawType::imageRect:
                    case DrawType::msaaStrokes:
                    case DrawType::msaaMidpointFanBorrowedCoverage:
                    case DrawType::msaaMidpointFans:
                    case DrawType::msaaMidpointFanStencilReset:
                    case DrawType::msaaMidpointFanPathsStencil:
                    case DrawType::msaaMidpointFanPathsCover:
                    case DrawType::msaaOuterCubics:
                    case DrawType::clipReset:
                    case DrawType::renderPassInitialize:
                    case DrawType::renderPassResolve:
                        RIVE_UNREACHABLE();
                }

                std::ostringstream vertexGLSL;
                vertexGLSL << versionString << "\n";
                vertexGLSL << "#pragma shader_stage(vertex)\n";
                vertexGLSL << "#define " GLSL_VERTEX " true\n";
                vertexGLSL << glsl.str();
                vertexModule =
                    compile_shader_module_wagyu(context->m_device,
                                                vertexGLSL.str().c_str(),
                                                language);

                std::ostringstream fragmentGLSL;
                fragmentGLSL << versionString << "\n";
                fragmentGLSL << "#pragma shader_stage(fragment)\n";
                fragmentGLSL << "#define " GLSL_FRAGMENT " true\n";
                fragmentGLSL << glsl.str();
                fragmentModule =
                    compile_shader_module_wagyu(context->m_device,
                                                fragmentGLSL.str().c_str(),
                                                language);
#else
                RIVE_UNREACHABLE();
#endif
                break;
            }

            case gpu::InterlockMode::atomics:
            {
                switch (drawType)
                {
                    case DrawType::midpointFanPatches:
                    case DrawType::midpointFanCenterAAPatches:
                    case DrawType::outerCurvePatches:
                        vertexShader = &wgsl::atomic_draw_path_webgpu_vert;
                        fragmentShader =
                            fixedFunctionColorOutput
                                ? &wgsl::atomic_draw_path_webgpu_fixedcolor_frag
                                : &wgsl::atomic_draw_path_webgpu_frag;
                        break;

                    case DrawType::interiorTriangulation:
                        vertexShader =
                            &wgsl::atomic_draw_interior_triangles_webgpu_vert;
                        fragmentShader =
                            fixedFunctionColorOutput
                                ? &wgsl::
                                      atomic_draw_interior_triangles_webgpu_fixedcolor_frag
                                : &wgsl::
                                      atomic_draw_interior_triangles_webgpu_frag;
                        break;

                    case DrawType::atlasBlit:
                        vertexShader =
                            &wgsl::atomic_draw_atlas_blit_webgpu_vert;
                        fragmentShader =
                            fixedFunctionColorOutput
                                ? &wgsl::
                                      atomic_draw_atlas_blit_webgpu_fixedcolor_frag
                                : &wgsl::atomic_draw_atlas_blit_webgpu_frag;
                        break;

                    case DrawType::imageRect:
                        vertexShader =
                            &wgsl::atomic_draw_image_rect_webgpu_vert;
                        fragmentShader =
                            fixedFunctionColorOutput
                                ? &wgsl::
                                      atomic_draw_image_rect_webgpu_fixedcolor_frag
                                : &wgsl::atomic_draw_image_rect_webgpu_frag;
                        break;

                    case DrawType::imageMesh:
                        vertexShader =
                            &wgsl::atomic_draw_image_mesh_webgpu_vert;
                        fragmentShader =
                            fixedFunctionColorOutput
                                ? &wgsl::
                                      atomic_draw_image_mesh_webgpu_fixedcolor_frag
                                : &wgsl::atomic_draw_image_mesh_webgpu_frag;
                        break;

                    case DrawType::renderPassResolve:
                        if (fixedFunctionColorOutput)
                        {
                            vertexShader = &wgsl::atomic_resolve_webgpu_vert;
                            fragmentShader =
                                &wgsl::atomic_resolve_webgpu_fixedcolor_frag;
                        }
                        else
                        {
                            // Since we use storage buffers for atomic PLS,
                            // WebGPU always does coalesced resolve & transfer
                            // when not using fixedFunctionColorOutput.
                            assert(enums::is_flag_set(
                                shaderMiscFlags,
                                gpu::ShaderMiscFlags::
                                    coalescedResolveAndTransfer));
                            vertexShader =
                                &wgsl::atomic_resolve_coalesced_webgpu_vert;
                            fragmentShader =
                                &wgsl::atomic_resolve_coalesced_webgpu_frag;
                        }
                        break;

                    case DrawType::renderPassInitialize:
                        vertexShader = &wgsl::atomic_init_webgpu_vert;
                        fragmentShader =
                            fixedFunctionColorOutput
                                ? &wgsl::atomic_init_webgpu_fixedcolor_frag
                                : &wgsl::atomic_init_webgpu_frag;
                        break;

                    case DrawType::msaaStrokes:
                    case DrawType::msaaMidpointFanBorrowedCoverage:
                    case DrawType::msaaMidpointFans:
                    case DrawType::msaaMidpointFanStencilReset:
                    case DrawType::msaaMidpointFanPathsStencil:
                    case DrawType::msaaMidpointFanPathsCover:
                    case DrawType::msaaOuterCubics:
                    case DrawType::clipReset:
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
                        if (context->m_capabilities
                                .polyfillVertexStorageBuffers)
                        {
                            vertexShader =
                                enums::is_flag_set(
                                    shaderFeatures,
                                    ShaderFeatures::ENABLE_CLIP_RECT)
                                    ? &wgsl::draw_msaa_path_webgpu_nossbo_vert
                                    : &wgsl::
                                          draw_msaa_path_webgpu_nossbo_noclipdistance_vert;
                        }
                        else
                        {
                            vertexShader =
                                enums::is_flag_set(
                                    shaderFeatures,
                                    ShaderFeatures::ENABLE_CLIP_RECT)
                                    ? &wgsl::draw_msaa_path_webgpu_vert
                                    : &wgsl::
                                          draw_msaa_path_webgpu_noclipdistance_vert;
                        }
                        fragmentShader =
                            fixedFunctionColorOutput
                                ? &wgsl::draw_msaa_path_webgpu_fixedcolor_frag
                                : &wgsl::draw_msaa_path_webgpu_frag;
                        break;

                    case DrawType::clipReset:
                        vertexShader = &wgsl::draw_msaa_stencil_vert;
                        fragmentShader = &wgsl::draw_msaa_stencil_frag;
                        break;

                    case DrawType::interiorTriangulation:
                        // Interior triangulation is not yet implemented for
                        // MSAA.
                        RIVE_UNREACHABLE();
                        break;

                    case DrawType::atlasBlit:
                        if (context->m_capabilities
                                .polyfillVertexStorageBuffers)
                        {
                            vertexShader =
                                enums::is_flag_set(
                                    shaderFeatures,
                                    ShaderFeatures::ENABLE_CLIP_RECT)
                                    ? &wgsl::
                                          draw_msaa_atlas_blit_webgpu_nossbo_vert
                                    : &wgsl::
                                          draw_msaa_atlas_blit_webgpu_nossbo_noclipdistance_vert;
                        }
                        else
                        {
                            vertexShader =
                                enums::is_flag_set(
                                    shaderFeatures,
                                    ShaderFeatures::ENABLE_CLIP_RECT)
                                    ? &wgsl::draw_msaa_atlas_blit_webgpu_vert
                                    : &wgsl::
                                          draw_msaa_atlas_blit_webgpu_noclipdistance_vert;
                        }
                        fragmentShader =
                            fixedFunctionColorOutput
                                ? &wgsl::
                                      draw_msaa_atlas_blit_webgpu_fixedcolor_frag
                                : &wgsl::draw_msaa_atlas_blit_webgpu_frag;
                        break;

                    case DrawType::imageMesh:
                        vertexShader =
                            enums::is_flag_set(shaderFeatures,
                                               ShaderFeatures::ENABLE_CLIP_RECT)
                                ? &wgsl::draw_msaa_image_mesh_webgpu_vert
                                : &wgsl::
                                      draw_msaa_image_mesh_webgpu_noclipdistance_vert;
                        fragmentShader =
                            fixedFunctionColorOutput
                                ? &wgsl::
                                      draw_msaa_image_mesh_webgpu_fixedcolor_frag
                                : &wgsl::draw_msaa_image_mesh_webgpu_frag;
                        break;

                    case DrawType::renderPassInitialize:
                        // MSAA render passes get initialized by drawing the
                        // previous contents into the framebuffer.
                        // (LoadAction::preserveRenderTarget only.)
                        vertexShader =
                            &wgsl::blit_texture_as_draw_filtered_webgpu_vert;
                        fragmentShader =
                            &wgsl::blit_texture_as_draw_filtered_webgpu_frag;
                        break;

                    case DrawType::imageRect:
                    case DrawType::renderPassResolve:
                        RIVE_UNREACHABLE();
                }
                break;
            }

            case gpu::InterlockMode::clockwiseAtomic:
                RIVE_UNREACHABLE();
        }

        // rasterOrdering and clockwise modes compile raw GLSL directly to
        // vertex/fragmentModules, and skip the (WGSL) vertex/fragmentShaders.
        // Everything else just selects WGSL vertex/fragmentShaders to be
        // compiled into modules.
        if (!vertexModule)
        {
            assert(vertexShader != nullptr);
            vertexModule =
                compile_shader_module_wgsl(context->m_device, *vertexShader);
        }
        if (!fragmentModule)
        {
            assert(fragmentShader != nullptr);
            fragmentModule =
                compile_shader_module_wgsl(context->m_device, *fragmentShader);
        }

        for (auto framebufferFormat :
             {wgpu::TextureFormat::BGRA8Unorm, wgpu::TextureFormat::RGBA8Unorm})
        {
            int pipelineIdx = RenderPipelineIdx(framebufferFormat);
            m_renderPipelines[pipelineIdx] =
                context->makeDrawPipeline(drawType,
                                          shaderFeatures,
                                          interlockMode,
                                          shaderMiscFlags,
                                          framebufferFormat,
                                          vertexModule,
                                          fragmentModule,
                                          vertexShader,
                                          fragmentShader,
                                          pipelineState);
        }
    }

    wgpu::RenderPipeline renderPipeline(
        wgpu::TextureFormat framebufferFormat) const
    {
        return m_renderPipelines[RenderPipelineIdx(framebufferFormat)];
    }

private:
    static int RenderPipelineIdx(wgpu::TextureFormat framebufferFormat)
    {
        assert(framebufferFormat == wgpu::TextureFormat::BGRA8Unorm ||
               framebufferFormat == wgpu::TextureFormat::RGBA8Unorm);
        return framebufferFormat == wgpu::TextureFormat::BGRA8Unorm ? 1 : 0;
    }

    wgpu::RenderPipeline m_renderPipelines[2];
};

#ifdef RIVE_WAGYU
static void parse_vendor_extensions(WGPUAdapter adapter,
                                    WGPUDevice device,
                                    RenderContextWebGPUImpl::Capabilities* caps)
{
    WGPUWagyuStringArray extensions = WGPU_WAGYU_STRING_ARRAY_INIT;
    if (caps->backendType == wgpu::BackendType::Vulkan)
    {
        wgpuWagyuDeviceGetExtensions(device, &extensions);
        for (size_t i = 0; i < extensions.stringCount; ++i)
        {
            if (!strcmp(extensions.strings[i].data,
                        "VK_EXT_rasterization_order_attachment_access"))
            {
                caps->VK_EXT_rasterization_order_attachment_access = true;
            }
        }
    }
    else if (caps->backendType == wgpu::BackendType::OpenGLES)
    {
        wgpuWagyuAdapterGetExtensions(adapter, &extensions);
        for (size_t i = 0; i < extensions.stringCount; ++i)
        {
            if (!strcmp(extensions.strings[i].data,
                        "GL_EXT_shader_pixel_local_storage"))
            {
                caps->GL_EXT_shader_pixel_local_storage = true;
            }
            else if (!strcmp(extensions.strings[i].data,
                             "GL_EXT_shader_pixel_local_storage2"))
            {
                caps->GL_EXT_shader_pixel_local_storage2 = true;
            }
        }
    }
    wgpuWagyuStringArrayFreeMembers(extensions);
}
#endif

RenderContextWebGPUImpl::RenderContextWebGPUImpl(
    wgpu::Adapter adapter,
    wgpu::Device device,
    wgpu::Queue queue,
    const ContextOptions& contextOptions) :
    m_device(device), m_queue(queue), m_contextOptions(contextOptions)
{
    wgpu::Limits deviceLimits = {};
    // CompatibilityModeLimits (and compatibility mode itself) don't exist on
    // legacy WebGPU v1.
#if RIVE_WEBGPU > 1
    wgpu::CompatibilityModeLimits compatModeLimits = {};
    if (contextOptions.compatibilityMode)
    {
        deviceLimits.nextInChain = &compatModeLimits;
    }
#endif
    const bool deviceLimitsValid = m_device.GetLimits(&deviceLimits);

#ifdef RIVE_WAGYU
    m_capabilities.backendType = static_cast<wgpu::BackendType>(
        wgpuWagyuAdapterGetBackend(adapter.Get()));

    parse_vendor_extensions(adapter.Get(), device.Get(), &m_capabilities);

    // Determine plsType.
    if (m_capabilities.VK_EXT_rasterization_order_attachment_access)
    {
        m_capabilities.plsType =
            PixelLocalStorageType::VK_EXT_rasterization_order_attachment_access;
        m_platformFeatures.supportsRasterOrderingMode = true;
    }
    else if (m_capabilities.GL_EXT_shader_pixel_local_storage)
    {
        m_capabilities.plsType =
            PixelLocalStorageType::GL_EXT_shader_pixel_local_storage;
        m_platformFeatures.supportsRasterOrderingMode = true;
        m_platformFeatures.supportsClockwiseMode = true;
        m_platformFeatures.supportsClockwiseFixedFunctionMode =
            m_capabilities.GL_EXT_shader_pixel_local_storage2;
    }
#endif

    uint32_t maxStorageBuffersInVertexStage = wgpu::kLimitU32Undefined;
    if (deviceLimitsValid)
    {
        maxStorageBuffersInVertexStage =
#if RIVE_WEBGPU > 1
            m_contextOptions.compatibilityMode
                // In compatibility mode, the vertex stage has its own
                // storage-buffer limit.
                ? compatModeLimits.maxStorageBuffersInVertexStage
                :
#endif
                // Outside compatibility mode, there's no such split:
                // maxStorageBuffersPerShaderStage applies to every stage,
                // vertex included
                deviceLimits.maxStorageBuffersPerShaderStage;
    }
    if (maxStorageBuffersInVertexStage == wgpu::kLimitU32Undefined)
    {
        maxStorageBuffersInVertexStage =
#if RIVE_WEBGPU > 1
            m_contextOptions.compatibilityMode
                ? 0 // Compat mode doesn't guarantee any vertex storage buffers.
                :
#endif
                8; // Core WebGPU guarantees at least 8 buffers per stage.
    }
    if (maxStorageBuffersInVertexStage < gpu::kMaxStorageBuffers)
    {
        // Rive uses storage buffers in the vertex shader. Polyfill them via
        // textures if the device doesn't support a sufficient number of
        // vertex-stage storage buffers.
#if RIVE_WEBGPU > 1
        assert(m_contextOptions.compatibilityMode);
#endif
        m_capabilities.polyfillVertexStorageBuffers = true;
    }

    // InterlockMode::atomics binds the color, clip, and coverage PLS planes as
    // storage buffers in the fragment stage. Only advertise support for atomic
    // mode if the device allows at least 3 buffers per stage.
    m_platformFeatures.supportsAtomicMode =
        deviceLimitsValid && deviceLimits.maxStorageBuffersPerShaderStage >= 3;
    if (!m_platformFeatures.supportsAtomicMode)
    {
        printf(
            "WARNING: atomic mode disabled because deviceLimits.maxStorageBuffersPerShaderStage is not at least 3.");
    }
    m_platformFeatures.atomicPLSInitNeedsDraw = true;

    m_platformFeatures.clipSpaceBottomUp = true;
    m_platformFeatures.framebufferBottomUp = false;
    m_platformFeatures.msaaColorPreserveNeedsDraw = true;

    m_platformFeatures.supportsTextureCompressionBC =
        m_device.HasFeature(wgpu::FeatureName::TextureCompressionBC);
    m_platformFeatures.supportsTextureCompressionASTC =
        m_device.HasFeature(wgpu::FeatureName::TextureCompressionASTC);
    m_platformFeatures.supportsTextureCompressionETC2 =
        m_device.HasFeature(wgpu::FeatureName::TextureCompressionETC2);
}

void RenderContextWebGPUImpl::initGPUObjects()
{
    // All bind group layouts (per-flush, per-draw, PLS texture, sampler) and
    // the draw pipeline layout are owned by PipelineLayout, lazily built per
    // InterlockMode. PipelineLayout needs m_emptyBindingsLayout for the
    // WAGYU/no-PLS path, so create that first.
    wgpu::BindGroupLayoutDescriptor emptyBindingsDesc = {};
    m_emptyBindingsLayout = m_device.CreateBindGroupLayout(&emptyBindingsDesc);

    wgpu::BufferDescriptor nullStorageBufferDesc = {
        .usage = wgpu::BufferUsage::Storage,
        .size = sizeof(uint32_t),
    };
    m_nullStorageBuffer = m_device.CreateBuffer(&nullStorageBufferDesc);

    wgpu::SamplerDescriptor linearSamplerDesc = {
        .addressModeU = wgpu::AddressMode::ClampToEdge,
        .addressModeV = wgpu::AddressMode::ClampToEdge,
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear,
        .mipmapFilter = wgpu::MipmapFilterMode::Nearest,
    };

    m_linearSampler = m_device.CreateSampler(&linearSamplerDesc);

    for (size_t i = 0; i < ImageSampler::MAX_SAMPLER_PERMUTATIONS; ++i)
    {
        ImageWrap wrapX = ImageSampler::GetWrapXOptionFromKey(i);
        ImageWrap wrapY = ImageSampler::GetWrapYOptionFromKey(i);
        ImageFilter filter = ImageSampler::GetFilterOptionFromKey(i);
        wgpu::FilterMode minMagFilter = webgpu_filter_mode(filter);

        wgpu::SamplerDescriptor samplerDesc = {
            .addressModeU = webgpu_address_mode(wrapX),
            .addressModeV = webgpu_address_mode(wrapY),
            .magFilter = minMagFilter,
            .minFilter = minMagFilter,
            .mipmapFilter = webgpu_mipmap_filter_mode(filter),
        };

        m_imageSamplers[i] = m_device.CreateSampler(&samplerDesc);
    }

    wgpu::BindGroupEntry samplerBindingEntries[] = {
        {
            .binding = GRAD_TEXTURE_IDX,
            .sampler = m_linearSampler,
        },
        {
            .binding = FEATHER_TEXTURE_IDX,
            .sampler = m_linearSampler,
        },
        {
            .binding = ATLAS_TEXTURE_IDX,
            .sampler = m_linearSampler,
        },
    };

    wgpu::BindGroupDescriptor samplerBindGroupDesc = {
        .layout = drawPipelineLayout(gpu::InterlockMode::rasterOrdering)
                      .bindGroupLayout(WEBGPU_SAMPLER_BINDINGS_SET),
        .entryCount = std::size(samplerBindingEntries),
        .entries = samplerBindingEntries,
    };

    m_samplerBindings = m_device.CreateBindGroup(&samplerBindGroupDesc);

    static_assert(PLS_TEXTURE_BINDINGS_SET == WEBGPU_BINDINGS_SET_COUNT - 2);
    static_assert(WEBGPU_SAMPLER_BINDINGS_SET == WEBGPU_BINDINGS_SET_COUNT - 1);

#ifdef RIVE_WAGYU
    const bool supportsShaderPixelLocalStorageEXT =
        m_capabilities.plsType ==
        PixelLocalStorageType::GL_EXT_shader_pixel_local_storage;
    if (supportsShaderPixelLocalStorageEXT)
    {
        // We have to manually implement load/store operations from a shader
        // when using EXT_shader_pixel_local_storage.
        std::ostringstream glsl;
        glsl << "#version 310 es\n";
        glsl << "#define " GLSL_VERTEX " true\n";
        // If we are being compiled by SPIRV transpiler for introspection, use
        // gl_VertexIndex instead of gl_VertexID.
        glsl << "#ifndef GL_EXT_shader_pixel_local_storage\n";
        glsl << "#define gl_VertexID gl_VertexIndex\n";
        glsl << "#endif\n";
        glsl << "#define " GLSL_ENABLE_CLIPPING " true\n";
        BuildLoadStoreEXTGLSL(glsl, LoadStoreActionsEXT::none);
        m_loadStoreEXTVertexShader =
            compile_shader_module_wagyu(m_device,
                                        glsl.str().c_str(),
                                        WGPUWagyuShaderLanguage_GLSLRAW);
        m_loadStoreEXTUniforms = makeUniformBufferRing(sizeof(float) * 4);
    }
#endif

    wgpu::BufferDescriptor tessSpanIndexBufferDesc = {
        .usage = wgpu::BufferUsage::Index,
        .size = sizeof(gpu::kTessSpanIndices),
        .mappedAtCreation = true,
    };
    m_tessSpanIndexBuffer = m_device.CreateBuffer(&tessSpanIndexBufferDesc);
    memcpy(m_tessSpanIndexBuffer.GetMappedRange(),
           gpu::kTessSpanIndices,
           sizeof(gpu::kTessSpanIndices));
    m_tessSpanIndexBuffer.Unmap();

    wgpu::BufferDescriptor patchBufferDesc = {
        .usage = wgpu::BufferUsage::Vertex,
        .size = kPatchVertexBufferCount * sizeof(PatchVertex),
        .mappedAtCreation = true,
    };
    m_pathPatchVertexBuffer = m_device.CreateBuffer(&patchBufferDesc);

    patchBufferDesc.size = (kPatchIndexBufferCount * sizeof(uint16_t));
    // WebGPU buffer sizes must be multiples of 4.
    patchBufferDesc.size =
        math::round_up_to_multiple_of<4>(patchBufferDesc.size);
    patchBufferDesc.usage = wgpu::BufferUsage::Index;
    m_pathPatchIndexBuffer = m_device.CreateBuffer(&patchBufferDesc);

    GeneratePatchBufferData(
        reinterpret_cast<PatchVertex*>(
            m_pathPatchVertexBuffer.GetMappedRange()),
        reinterpret_cast<uint16_t*>(m_pathPatchIndexBuffer.GetMappedRange()));

    m_pathPatchVertexBuffer.Unmap();
    m_pathPatchIndexBuffer.Unmap();

    wgpu::BufferDescriptor imageRectVertexBufferDesc = {
        .usage = wgpu::BufferUsage::Vertex,
        .size = sizeof(gpu::kImageRectVertices),
        .mappedAtCreation = true,
    };
    m_imageRectVertexBuffer = m_device.CreateBuffer(&imageRectVertexBufferDesc);
    memcpy(m_imageRectVertexBuffer.GetMappedRange(),
           gpu::kImageRectVertices,
           sizeof(gpu::kImageRectVertices));
    m_imageRectVertexBuffer.Unmap();

    wgpu::BufferDescriptor imageRectIndexBufferDesc = {
        .usage = wgpu::BufferUsage::Index,
        // WebGPU buffer sizes must be multiples of 4.
        .size =
            math::round_up_to_multiple_of<4>(sizeof(gpu::kImageRectIndices)),
        .mappedAtCreation = true,
    };
    m_imageRectIndexBuffer = m_device.CreateBuffer(&imageRectIndexBufferDesc);
    memcpy(m_imageRectIndexBuffer.GetMappedRange(),
           gpu::kImageRectIndices,
           sizeof(gpu::kImageRectIndices));
    m_imageRectIndexBuffer.Unmap();

    wgpu::TextureDescriptor featherTextureDesc = {
        .usage =
            wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
        .dimension = wgpu::TextureDimension::e2D,
        .size = {gpu::GAUSSIAN_TABLE_SIZE, FEATHER_TEXTURE_1D_ARRAY_LENGTH},
        .format = wgpu::TextureFormat::R16Float,
    };

    m_featherTexture = m_device.CreateTexture(&featherTextureDesc);
    wgpu::TexelCopyTextureInfo dest = {.texture = m_featherTexture};
    wgpu::TexelCopyBufferLayout layout = {
        .bytesPerRow = sizeof(gpu::g_gaussianIntegralTableF16),
    };
    wgpu::Extent3D extent = {
        .width = gpu::GAUSSIAN_TABLE_SIZE,
        .height = 1,
    };
    m_queue.WriteTexture(&dest,
                         gpu::g_gaussianIntegralTableF16,
                         sizeof(gpu::g_gaussianIntegralTableF16),
                         &layout,
                         &extent);
    dest.origin.y = 1;
    m_queue.WriteTexture(&dest,
                         gpu::g_inverseGaussianIntegralTableF16,
                         sizeof(gpu::g_inverseGaussianIntegralTableF16),
                         &layout,
                         &extent);
    m_featherTextureView = m_featherTexture.CreateView();

    wgpu::TextureDescriptor nullTextureDesc = {
        .usage = wgpu::TextureUsage::TextureBinding,
        .dimension = wgpu::TextureDimension::e2D,
        .size = {1, 1},
        .format = wgpu::TextureFormat::RGBA8Unorm,
    };

    m_nullTexture = m_device.CreateTexture(&nullTextureDesc);
    m_nullTextureView = m_nullTexture.CreateView();

    m_colorRampPipeline = std::make_unique<ColorRampPipeline>(this);
    m_tessellatePipeline = std::make_unique<TessellatePipeline>(this);
    m_atlasPipeline = std::make_unique<AtlasPipeline>(this);
}

RenderContextWebGPUImpl::~RenderContextWebGPUImpl() {}

RenderTargetWebGPU::RenderTargetWebGPU(
    wgpu::Device device,
    const RenderContextWebGPUImpl::Capabilities& capabilities,
    wgpu::TextureFormat framebufferFormat,
    uint32_t width,
    uint32_t height) :
    RenderTarget(width, height),
    m_device(std::move(device)),
    m_framebufferFormat(framebufferFormat),
    m_transientPLSUsage(wgpu::TextureUsage::RenderAttachment),
    m_targetTextureView{} // Will be configured later by setTargetTexture().
{
#ifdef RIVE_WAGYU
    if (capabilities.plsType ==
        RenderContextWebGPUImpl::PixelLocalStorageType::
            VK_EXT_rasterization_order_attachment_access)
    {
        m_transientPLSUsage |= static_cast<wgpu::TextureUsage>(
            WGPUTextureUsage_WagyuInputAttachment |
            WGPUTextureUsage_WagyuTransientAttachment);
    }
#endif
}

void RenderTargetWebGPU::setTargetTextureView(wgpu::TextureView textureView,
                                              wgpu::Texture texture)
{
    m_targetTexture = texture;
    m_targetTextureView = textureView;
}

wgpu::TextureView RenderTargetWebGPU::coverageTextureView()
{
    if (m_coverageTextureView == nullptr)
    {
        wgpu::TextureDescriptor desc = {
            .usage = m_transientPLSUsage,
            .size = {static_cast<uint32_t>(width()),
                     static_cast<uint32_t>(height())},
            .format = wgpu::TextureFormat::R32Uint,
        };
        m_coverageTexture = m_device.CreateTexture(&desc);
        m_coverageTextureView = m_coverageTexture.CreateView();
    }
    return m_coverageTextureView;
}

wgpu::TextureView RenderTargetWebGPU::clipTextureView()
{
    if (m_clipTexture == nullptr)
    {
        wgpu::TextureDescriptor desc = {
            .usage = m_transientPLSUsage,
            .size = {static_cast<uint32_t>(width()),
                     static_cast<uint32_t>(height())},
            .format = wgpu::TextureFormat::R32Uint,
        };
        m_clipTexture = m_device.CreateTexture(&desc);
        m_clipTextureView = m_clipTexture.CreateView();
    }
    return m_clipTextureView;
}

wgpu::TextureView RenderTargetWebGPU::scratchColorTextureView()
{
    if (m_scratchColorTexture == nullptr)
    {
        wgpu::TextureDescriptor desc = {
            .usage = m_transientPLSUsage,
            .size = {static_cast<uint32_t>(width()),
                     static_cast<uint32_t>(height())},
            .format = m_framebufferFormat,
        };
        m_scratchColorTexture = m_device.CreateTexture(&desc);
        m_scratchColorTextureView = m_scratchColorTexture.CreateView();
    }
    return m_scratchColorTextureView;
}

wgpu::TextureView RenderTargetWebGPU::msaaColorTextureView()
{
    if (m_msaaColorTexture == nullptr)
    {
        wgpu::TextureDescriptor desc = {
            .usage = wgpu::TextureUsage::RenderAttachment,
            .size = {static_cast<uint32_t>(width()),
                     static_cast<uint32_t>(height())},
            .format = m_framebufferFormat,
            .sampleCount = MSAA_SAMPLE_COUNT,
        };
        m_msaaColorTexture = m_device.CreateTexture(&desc);
        m_msaaColorTextureView = m_msaaColorTexture.CreateView();
    }
    return m_msaaColorTextureView;
}

wgpu::TextureView RenderTargetWebGPU::msaaDepthStencilTextureView()
{
    if (m_msaaDepthStencilTexture == nullptr)
    {
        wgpu::TextureDescriptor desc = {
            .usage = wgpu::TextureUsage::RenderAttachment,
            .size = {static_cast<uint32_t>(width()),
                     static_cast<uint32_t>(height())},
            .format = wgpu::TextureFormat::Depth24PlusStencil8,
            .sampleCount = MSAA_SAMPLE_COUNT,
        };
        m_msaaDepthStencilTexture = m_device.CreateTexture(&desc);
        m_msaaDepthStencilTextureView = m_msaaDepthStencilTexture.CreateView();
    }
    return m_msaaDepthStencilTextureView;
}

wgpu::Texture RenderTargetWebGPU::dstColorTexture()
{
    if (m_dstColorTexture == nullptr)
    {
        wgpu::TextureDescriptor desc = {
            .usage = wgpu::TextureUsage::CopyDst |
                     wgpu::TextureUsage::TextureBinding |
                     wgpu::TextureUsage::RenderAttachment,
            .size = {static_cast<uint32_t>(width()),
                     static_cast<uint32_t>(height())},
            .format = m_framebufferFormat,
            .sampleCount = 1,
        };
        m_dstColorTexture = m_device.CreateTexture(&desc);
    }
    return m_dstColorTexture;
}

wgpu::TextureView RenderTargetWebGPU::dstColorTextureView()
{
    if (m_dstColorTextureView == nullptr)
    {
        m_dstColorTextureView = dstColorTexture().CreateView();
    }
    return m_dstColorTextureView;
}

void RenderTargetWebGPU::copyTargetToDstColorTexture(
    wgpu::CommandEncoder commandEncoder,
    IAABB dstReadBounds)
{
    const wgpu::Origin3D origin = {
        .x = static_cast<uint32_t>(dstReadBounds.left),
        .y = static_cast<uint32_t>(dstReadBounds.top),
    };
    const wgpu::TexelCopyTextureInfo src = {
        .texture = m_targetTexture,
        .origin = origin,
    };
    const wgpu::TexelCopyTextureInfo dst = {
        .texture = dstColorTexture(),
        .origin = origin,
    };
    const wgpu::Extent3D copySize = {
        .width = static_cast<uint32_t>(dstReadBounds.width()),
        .height = static_cast<uint32_t>(dstReadBounds.height()),
    };
    commandEncoder.CopyTextureToTexture(&src, &dst, &copySize);
}

rcp<RenderTargetWebGPU> RenderContextWebGPUImpl::makeRenderTarget(
    wgpu::TextureFormat framebufferFormat,
    uint32_t width,
    uint32_t height)
{
    return rcp(new RenderTargetWebGPU(m_device,
                                      m_capabilities,
                                      framebufferFormat,
                                      width,
                                      height));
}

#ifdef RIVE_CANVAS
rcp<RenderCanvas> RenderContextWebGPUImpl::makeRenderCanvas(uint32_t width,
                                                            uint32_t height)
{
    wgpu::TextureDescriptor textureDesc = {
        .usage = wgpu::TextureUsage::TextureBinding |
                 wgpu::TextureUsage::RenderAttachment |
                 wgpu::TextureUsage::CopySrc,
        .dimension = wgpu::TextureDimension::e2D,
        .size = {width, height},
        .format = wgpu::TextureFormat::RGBA8Unorm,
    };

    auto texture =
        make_rcp<TextureWebGPUImpl>(width,
                                    height,
                                    m_device.CreateTexture(&textureDesc));

    auto renderTarget =
        makeRenderTarget(wgpu::TextureFormat::RGBA8Unorm, width, height);
    renderTarget->setTargetTextureView(texture->textureView(),
                                       texture->texture());

    auto renderImage = make_rcp<RiveRenderImage>(std::move(texture));

    return make_rcp<RenderCanvas>(std::move(renderImage),
                                  std::move(renderTarget));
}
std::unique_ptr<rive::ore::Context> RenderContextWebGPUImpl::makeOreContext()
{
    return rive::ore::ContextWGPU::Make(m_device,
                                        m_queue,
                                        m_capabilities.backendType);
}
#endif

class RenderBufferWebGPUImpl : public RenderBuffer
{
public:
    RenderBufferWebGPUImpl(wgpu::Device device,
                           wgpu::Queue queue,
                           RenderBufferType renderBufferType,
                           RenderBufferFlags renderBufferFlags,
                           size_t sizeInBytes) :
        RenderBuffer(renderBufferType, renderBufferFlags, sizeInBytes),
        m_device(device),
        m_queue(queue)
    {
        bool mappedOnceAtInitialization =
            enums::is_flag_set(flags(),
                               RenderBufferFlags::mappedOnceAtInitialization);
        int bufferCount = mappedOnceAtInitialization ? 1 : gpu::kBufferRingSize;
        wgpu::BufferDescriptor desc = {
            .usage = type() == RenderBufferType::index
                         ? wgpu::BufferUsage::Index
                         : wgpu::BufferUsage::Vertex,
            // WebGPU buffer sizes must be multiples of 4.
            .size = math::round_up_to_multiple_of<4>(sizeInBytes),
            .mappedAtCreation = mappedOnceAtInitialization,
        };
        if (!mappedOnceAtInitialization)
        {
            desc.usage |= wgpu::BufferUsage::CopyDst;
        }
        for (int i = 0; i < bufferCount; ++i)
        {
            m_buffers[i] = device.CreateBuffer(&desc);
        }
    }

    wgpu::Buffer submittedBuffer() const
    {
        return m_buffers[m_submittedBufferIdx];
    }

protected:
    void* onMap() override
    {
        m_submittedBufferIdx =
            (m_submittedBufferIdx + 1) % gpu::kBufferRingSize;
        assert(m_buffers[m_submittedBufferIdx] != nullptr);
        if (enums::is_flag_set(flags(),
                               RenderBufferFlags::mappedOnceAtInitialization))
        {
            return m_buffers[m_submittedBufferIdx].GetMappedRange();
        }
        else
        {
            if (m_stagingBuffer == nullptr)
            {
                m_stagingBuffer.reset(new uint8_t[sizeInBytes()]);
            }
            return m_stagingBuffer.get();
        }
    }

    void onUnmap() override
    {
        if (enums::is_flag_set(flags(),
                               RenderBufferFlags::mappedOnceAtInitialization))
        {
            m_buffers[m_submittedBufferIdx].Unmap();
        }
        else
        {
            m_queue.WriteBuffer(m_buffers[m_submittedBufferIdx],
                                0,
                                m_stagingBuffer.get(),
                                sizeInBytes());
        }
    }

private:
    const wgpu::Device m_device;
    const wgpu::Queue m_queue;
    wgpu::Buffer m_buffers[gpu::kBufferRingSize];
    int m_submittedBufferIdx = -1;
    std::unique_ptr<uint8_t[]> m_stagingBuffer;
};

rcp<RenderBuffer> RenderContextWebGPUImpl::makeRenderBuffer(
    RenderBufferType type,
    RenderBufferFlags flags,
    size_t sizeInBytes)
{
    return make_rcp<RenderBufferWebGPUImpl>(m_device,
                                            m_queue,
                                            type,
                                            flags,
                                            sizeInBytes);
}

#ifndef RIVE_WAGYU
// Blits texture-to-texture using a draw command.
class RenderContextWebGPUImpl::BlitTextureAsDrawPipeline
{
public:
    BlitTextureAsDrawPipeline(RenderContextWebGPUImpl* impl)
    {
        const wgpu::Device device = impl->device();

        wgpu::BindGroupLayoutEntry bindingEntries[] = {
            {
                .binding = IMAGE_TEXTURE_IDX,
                .visibility = wgpu::ShaderStage::Fragment,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Float,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            },
            {
                .binding = WEBGPU_IMAGE_SAMPLER_IDX,
                .visibility = wgpu::ShaderStage::Fragment,
                .sampler =
                    {
                        .type = wgpu::SamplerBindingType::Filtering,
                    },
            },
        };

        wgpu::BindGroupLayoutDescriptor bindingsDesc = {
            .entryCount = std::size(bindingEntries),
            .entries = bindingEntries,
        };

        m_perDrawBindGroupLayout = device.CreateBindGroupLayout(&bindingsDesc);

        wgpu::BindGroupLayout layouts[] = {
            impl->m_emptyBindingsLayout,
            m_perDrawBindGroupLayout,
        };
        static_assert(PER_DRAW_BINDINGS_SET == 1);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = std::size(layouts),
            .bindGroupLayouts = layouts,
        };

        wgpu::PipelineLayout pipelineLayout =
            device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShaderModule = compile_shader_module_wgsl(
            device,
            wgsl::blit_texture_as_draw_filtered_webgpu_vert);

        wgpu::ShaderModule fragmentShaderModule = compile_shader_module_wgsl(
            device,
            wgsl::blit_texture_as_draw_filtered_webgpu_frag);

        wgpu::ColorTargetState colorTargetState = {
            .format = wgpu::TextureFormat::RGBA8Unorm,
        };

        wgpu::FragmentState fragmentState = {
            .module = fragmentShaderModule,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .label = "RIVE_BlitTextureAsDrawPipeline",
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = vertexShaderModule,
                    .entryPoint = "main",
                },
            .primitive =
                {
                    .topology = wgpu::PrimitiveTopology::TriangleStrip,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = device.CreateRenderPipeline(&desc);
    }

    const wgpu::BindGroupLayout& perDrawBindGroupLayout() const
    {
        return m_perDrawBindGroupLayout;
    }
    wgpu::RenderPipeline renderPipeline() const { return m_renderPipeline; }

private:
    wgpu::BindGroupLayout m_perDrawBindGroupLayout;
    wgpu::RenderPipeline m_renderPipeline;
};
#endif

void RenderContextWebGPUImpl::generateMipmaps(wgpu::Texture texture)
{
    wgpu::CommandEncoder mipEncoder = m_device.CreateCommandEncoder();

#ifdef RIVE_WAGYU
    wgpuWagyuCommandEncoderGenerateMipmap(mipEncoder.Get(), texture.Get());
#else
    // Generate the mipmaps manually by drawing each layer.
    if (m_blitTextureAsDrawPipeline == nullptr)
    {
        m_blitTextureAsDrawPipeline =
            std::make_unique<BlitTextureAsDrawPipeline>(this);
    }

    wgpu::TextureViewDescriptor textureViewDesc = {
        .baseMipLevel = 0,
        .mipLevelCount = 1,
    };

    wgpu::TextureView dstView, srcView = texture.CreateView(&textureViewDesc);

    for (uint32_t level = 1; level < texture.GetMipLevelCount();
         ++level, srcView = std::move(dstView))
    {
        textureViewDesc.baseMipLevel = level;
        dstView = texture.CreateView(&textureViewDesc);

        wgpu::RenderPassColorAttachment attachment = {
            .view = dstView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {},
        };

        wgpu::RenderPassDescriptor mipPassDesc = {
            .label = "RIVE_MipMap_Generation_Pass",
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
        };

        wgpu::RenderPassEncoder mipPass =
            mipEncoder.BeginRenderPass(&mipPassDesc);

        wgpu::BindGroupEntry bindingEntries[] = {
            {
                .binding = IMAGE_TEXTURE_IDX,
                .textureView = srcView,
            },
            {
                .binding = WEBGPU_IMAGE_SAMPLER_IDX,
                .sampler = m_linearSampler,
            },
        };

        wgpu::BindGroupDescriptor bindGroupDesc = {
            .layout = m_blitTextureAsDrawPipeline->perDrawBindGroupLayout(),
            .entryCount = std::size(bindingEntries),
            .entries = bindingEntries,
        };

        wgpu::BindGroup bindings = m_device.CreateBindGroup(&bindGroupDesc);
        mipPass.SetBindGroup(PER_DRAW_BINDINGS_SET, bindings);

        mipPass.SetPipeline(m_blitTextureAsDrawPipeline->renderPipeline());
        mipPass.Draw(4);
        mipPass.End();
    }
#endif

    wgpu::CommandBuffer commands = mipEncoder.Finish();
    m_queue.Submit(1, &commands);
}

rcp<Texture> RenderContextWebGPUImpl::makeImageTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevelCount,
    GPUTextureFormat format,
    const uint8_t imageData[],
    uint8_t blockWidth,
    uint8_t blockHeight,
    bool /*srgb*/,
    bool generateRemainingMips)
{
    wgpu::TextureFormat wgpuFormat = wgpu::TextureFormat::RGBA8Unorm;
    uint32_t bytesPerBlock = 4;

    RIVE_DEBUG_CODE(bool isCompressed = false);

    switch (format)
    {
        case GPUTextureFormat::rgba32:
            assert(blockWidth == 1 && blockHeight == 1);
            break;
        case GPUTextureFormat::bc7:
            wgpuFormat = wgpu::TextureFormat::BC7RGBAUnorm;
            bytesPerBlock = 16;
            RIVE_DEBUG_CODE(isCompressed = true);
            break;
        case GPUTextureFormat::astc:
        {
            // wgpu ASTC enums are sequential in spec footprint order
            // starting at ASTC4x4Unorm. SRGB variant lives one entry later.
            const int idx = rive::astcFootprintIndex(blockWidth, blockHeight);
            if (idx < 0)
            {
                assert(!"unsupported ASTC block footprint");
                return nullptr;
            }
            wgpuFormat = static_cast<wgpu::TextureFormat>(
                static_cast<uint32_t>(wgpu::TextureFormat::ASTC4x4Unorm) +
                2 * idx);

            bytesPerBlock = 16;
            RIVE_DEBUG_CODE(isCompressed = true);
            break;
        }
        case GPUTextureFormat::etc2:

            wgpuFormat = wgpu::TextureFormat::ETC2RGBA8Unorm;
            bytesPerBlock = 16;

            break;
        default:
            assert(!"unsupported format");
            return nullptr;
    }

    assert(!(generateRemainingMips && isCompressed) &&
           "WebGPU mip generation is undefined on compressed formats");

    wgpu::TextureDescriptor textureDesc = {
        .usage =
            wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
        .dimension = wgpu::TextureDimension::e2D,
        .size = {width, height},
        .format = wgpuFormat,

        .mipLevelCount = mipLevelCount,
    };
    if (generateRemainingMips && mipLevelCount > 1)
    {
#ifdef RIVE_WAGYU
        // Wagyu generates mipmaps with copies.
        textureDesc.usage |= wgpu::TextureUsage::CopySrc;
#else
        // Unextended WebGPU implements mipmaps with draws.
        textureDesc.usage |= wgpu::TextureUsage::RenderAttachment;
#endif
    }

    wgpu::Texture texture = m_device.CreateTexture(&textureDesc);

    // Upload mip 0 only when caller wants auto-mipgen; otherwise upload all.
    const uint32_t levelsToUpload = generateRemainingMips ? 1u : mipLevelCount;
    size_t srcOffset = 0;
    for (uint32_t i = 0; i < levelsToUpload; ++i)
    {
        const uint32_t logW = std::max<uint32_t>(1u, width >> i);
        const uint32_t logH = std::max<uint32_t>(1u, height >> i);
        const uint32_t blocksX = (logW + blockWidth - 1) / blockWidth;
        const uint32_t blocksY = (logH + blockHeight - 1) / blockHeight;
        const uint32_t bytesPerRow = blocksX * bytesPerBlock;
        const size_t levelBytes = static_cast<size_t>(bytesPerRow) * blocksY;

        wgpu::TexelCopyTextureInfo dest = {.texture = texture, .mipLevel = i};
        wgpu::TexelCopyBufferLayout layout = {.bytesPerRow = bytesPerRow};
        wgpu::Extent3D extent = {logW, logH};
        m_queue.WriteTexture(&dest,
                             imageData + srcOffset,
                             levelBytes,
                             &layout,
                             &extent);
        srcOffset += levelBytes;
    }

    if (generateRemainingMips && mipLevelCount > 1)
    {
        generateMipmaps(texture);
    }

    return make_rcp<TextureWebGPUImpl>(width, height, std::move(texture));
}

class BufferWebGPU : public BufferRing
{
public:
    static std::unique_ptr<BufferWebGPU> Make(wgpu::Device device,
                                              wgpu::Queue queue,
                                              size_t capacityInBytes,
                                              wgpu::BufferUsage usage)
    {
        return std::make_unique<BufferWebGPU>(device,
                                              queue,
                                              capacityInBytes,
                                              usage);
    }

    BufferWebGPU(wgpu::Device device,
                 wgpu::Queue queue,
                 size_t capacityInBytesUnRounded,
                 wgpu::BufferUsage usage) :
        // Storage buffers must be multiples of 4 in size.
        BufferRing(math::round_up_to_multiple_of<4>(
            std::max<size_t>(capacityInBytesUnRounded, 1))),
        m_queue(queue)
    {
        wgpu::BufferDescriptor desc = {
            .usage = wgpu::BufferUsage::CopyDst | usage,
            .size = capacityInBytes(),
        };
        for (int i = 0; i < gpu::kBufferRingSize; ++i)
        {
            m_buffers[i] = device.CreateBuffer(&desc);
        }
    }

    wgpu::Buffer submittedBuffer() const
    {
        return m_buffers[submittedBufferIdx()];
    }

protected:
    void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        return shadowBuffer();
    }

    void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) override
    {
        m_queue.WriteBuffer(m_buffers[bufferIdx],
                            0,
                            shadowBuffer(),
                            mapSizeInBytes);
    }

    const wgpu::Queue m_queue;
    wgpu::Buffer m_buffers[kBufferRingSize];
};

// GL TextureFormat to use for a texture that polyfills a storage buffer.
static wgpu::TextureFormat storage_texture_format(
    gpu::StorageBufferStructure bufferStructure)
{
    switch (bufferStructure)
    {
        case gpu::StorageBufferStructure::uint32x4:
            return wgpu::TextureFormat::RGBA32Uint;
        case gpu::StorageBufferStructure::uint32x2:
            return wgpu::TextureFormat::RG32Uint;
        case gpu::StorageBufferStructure::float32x4:
            return wgpu::TextureFormat::RGBA32Float;
    }
    RIVE_UNREACHABLE();
}

// Buffer ring with a texture used to polyfill storage buffers when they are
// disabled.
class StorageTextureBufferWebGPU : public BufferWebGPU
{
public:
    StorageTextureBufferWebGPU(wgpu::Device device,
                               wgpu::Queue queue,
                               size_t capacityInBytes,
                               gpu::StorageBufferStructure bufferStructure) :
        BufferWebGPU(
            device,
            queue,
            gpu::StorageTextureBufferSize(capacityInBytes, bufferStructure),
            wgpu::BufferUsage::CopySrc),
        m_bufferStructure(bufferStructure)
    {
        // Create a texture to mirror the buffer contents.
        auto [textureWidth, textureHeight] =
            gpu::StorageTextureSize(this->capacityInBytes(), bufferStructure);

        wgpu::TextureDescriptor desc{
            .usage = wgpu::TextureUsage::TextureBinding |
                     wgpu::TextureUsage::CopyDst,
            .size = {textureWidth, textureHeight},
            .format = storage_texture_format(bufferStructure),
        };

        m_texture = device.CreateTexture(&desc);
        m_textureView = m_texture.CreateView();
    }

    void updateTextureFromBuffer(size_t bindingSizeInBytes,
                                 size_t offsetSizeInBytes,
                                 wgpu::CommandEncoder commandEncoder) const
    {
        auto [updateWidth, updateHeight] =
            gpu::StorageTextureSize(bindingSizeInBytes, m_bufferStructure);
        wgpu::TexelCopyBufferInfo srcBuffer = {
            .layout =
                {
                    .offset = offsetSizeInBytes,
                    .bytesPerRow = (STORAGE_TEXTURE_WIDTH *
                                    gpu::StorageBufferElementSizeInBytes(
                                        m_bufferStructure)),
                },
            .buffer = submittedBuffer(),
        };
        wgpu::TexelCopyTextureInfo dstTexture = {
            .texture = m_texture,
            .origin = {0, 0, 0},
        };
        wgpu::Extent3D copySize = {
            .width = updateWidth,
            .height = updateHeight,
        };
        commandEncoder.CopyBufferToTexture(&srcBuffer, &dstTexture, &copySize);
    }

    wgpu::TextureView textureView() const { return m_textureView; }

private:
    const StorageBufferStructure m_bufferStructure;
    wgpu::Texture m_texture;
    wgpu::TextureView m_textureView;
};

std::unique_ptr<BufferRing> RenderContextWebGPUImpl::makeUniformBufferRing(
    size_t capacityInBytes)
{
    // Uniform blocks must be multiples of 256 bytes in size.
    capacityInBytes = std::max<size_t>(capacityInBytes, 256);
    assert(capacityInBytes % 256 == 0);
    return std::make_unique<BufferWebGPU>(m_device,
                                          m_queue,
                                          capacityInBytes,
                                          wgpu::BufferUsage::Uniform);
}

std::unique_ptr<BufferRing> RenderContextWebGPUImpl::makeStorageBufferRing(
    size_t capacityInBytes,
    gpu::StorageBufferStructure bufferStructure)
{
    if (m_capabilities.polyfillVertexStorageBuffers)
    {
        return std::make_unique<StorageTextureBufferWebGPU>(m_device,
                                                            m_queue,
                                                            capacityInBytes,
                                                            bufferStructure);
    }
    else
    {
        return std::make_unique<BufferWebGPU>(m_device,
                                              m_queue,
                                              capacityInBytes,
                                              wgpu::BufferUsage::Storage);
    }
}

std::unique_ptr<BufferRing> RenderContextWebGPUImpl::makeVertexBufferRing(
    size_t capacityInBytes)
{
    return std::make_unique<BufferWebGPU>(m_device,
                                          m_queue,
                                          capacityInBytes,
                                          wgpu::BufferUsage::Vertex);
}

void RenderContextWebGPUImpl::resizeGradientTexture(uint32_t width,
                                                    uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);

    wgpu::TextureDescriptor desc{
        .usage = wgpu::TextureUsage::RenderAttachment |
                 wgpu::TextureUsage::TextureBinding,
        .size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        .format = wgpu::TextureFormat::RGBA8Unorm,
    };

    m_gradientTexture = m_device.CreateTexture(&desc);
    m_gradientTextureView = m_gradientTexture.CreateView();
}

void RenderContextWebGPUImpl::resizeTessellationTexture(uint32_t width,
                                                        uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);

    wgpu::TextureDescriptor desc{
        .usage = wgpu::TextureUsage::RenderAttachment |
                 wgpu::TextureUsage::TextureBinding,
        .size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        .format = wgpu::TextureFormat::RGBA32Uint,
    };

    m_tessVertexTexture = m_device.CreateTexture(&desc);
    m_tessVertexTextureView = m_tessVertexTexture.CreateView();
}

void RenderContextWebGPUImpl::resizeAtlasTexture(uint32_t width,
                                                 uint32_t height)
{
    width = std::max(width, 1u);
    height = std::max(height, 1u);

    wgpu::TextureDescriptor desc{
        .usage = wgpu::TextureUsage::RenderAttachment |
                 wgpu::TextureUsage::TextureBinding,
        .size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
        .format = wgpu::TextureFormat::R16Float,
    };

    m_atlasTexture = m_device.CreateTexture(&desc);
    m_atlasTextureView = m_atlasTexture.CreateView();
}

void RenderContextWebGPUImpl::resizeAtomicCoverageBacking(uint32_t width,
                                                          uint32_t height)
{
    m_atomicPLSBackingBufferSize =
        math::round_up_to_multiple_of<BUFFER_IMAGE_TILE_SIZE, uint64_t>(
            height) *
        math::round_up_to_multiple_of<BUFFER_IMAGE_TILE_SIZE, uint64_t>(width) *
        sizeof(uint32_t);
    m_atomicPLSColorBuffer = {};
    m_atomicPLSClipBuffer = {};
    m_atomicPLSCoverageBuffer = {};
}

constexpr static wgpu::BufferDescriptor plsBackingBufferDesc(uint64_t size)
{
    return {.usage = wgpu::BufferUsage::Storage, .size = size};
}

wgpu::Buffer RenderContextWebGPUImpl::atomicPLSColorBuffer()
{
    if (m_atomicPLSColorBuffer == nullptr)
    {
        auto desc = plsBackingBufferDesc(m_atomicPLSBackingBufferSize);
        m_atomicPLSColorBuffer = m_device.CreateBuffer(&desc);
    }
    return m_atomicPLSColorBuffer;
}

wgpu::Buffer RenderContextWebGPUImpl::atomicPLSClipBuffer()
{
    if (m_atomicPLSClipBuffer == nullptr)
    {
        auto desc = plsBackingBufferDesc(m_atomicPLSBackingBufferSize);
        m_atomicPLSClipBuffer = m_device.CreateBuffer(&desc);
    }
    return m_atomicPLSClipBuffer;
}

wgpu::Buffer RenderContextWebGPUImpl::atomicPLSCoverageBuffer()
{
    if (m_atomicPLSCoverageBuffer == nullptr)
    {
        auto desc = plsBackingBufferDesc(m_atomicPLSBackingBufferSize);
        m_atomicPLSCoverageBuffer = m_device.CreateBuffer(&desc);
    }
    return m_atomicPLSCoverageBuffer;
}

// Appends Rive's ImageDrawInstance attribs. The caller is responsible for
// placing these in a vertex buffer layout with WGPUVertexStepMode_Instance and
// arrayStride = sizeof(gpu::ImageDrawInstance).
template <uint32_t Capacity>
static void appendImageDrawInstanceAttribs(
    StackVector<WGPUVertexAttribute, Capacity>& attrs)
{
    attrs.push_back({
        .format = WGPUVertexFormat_Float32x4,
        .offset = (IMAGE_VIEW_MATRIX_ATTRIB_IDX - IMAGE_FIRST_ATTRIB_IDX) *
                  sizeof(uint32_t) * 4,
        .shaderLocation = IMAGE_VIEW_MATRIX_ATTRIB_IDX,
    });
    attrs.push_back({
        .format = WGPUVertexFormat_Float32x4,
        .offset = (IMAGE_CLIP_RECT_INVERSE_MATRIX_ATTRIB_IDX -
                   IMAGE_FIRST_ATTRIB_IDX) *
                  sizeof(uint32_t) * 4,
        .shaderLocation = IMAGE_CLIP_RECT_INVERSE_MATRIX_ATTRIB_IDX,
    });
    attrs.push_back({
        .format = WGPUVertexFormat_Float32x4,
        .offset = (IMAGE_TRANSLATES_ATTRIB_IDX - IMAGE_FIRST_ATTRIB_IDX) *
                  sizeof(uint32_t) * 4,
        .shaderLocation = IMAGE_TRANSLATES_ATTRIB_IDX,
    }); // translations
    attrs.push_back({
        .format = WGPUVertexFormat_Uint32x4,
        .offset = (IMAGE_PACKED_ATTRIBS_IDX - IMAGE_FIRST_ATTRIB_IDX) *
                  sizeof(uint32_t) * 4,
        .shaderLocation = IMAGE_PACKED_ATTRIBS_IDX,
    });
}

wgpu::RenderPipeline RenderContextWebGPUImpl::makeDrawPipeline(
    gpu::DrawType drawType,
    gpu::ShaderFeatures shaderFeatures,
    gpu::InterlockMode interlockMode,
    gpu::ShaderMiscFlags shaderMiscFlags,
    wgpu::TextureFormat framebufferFormat,
    wgpu::ShaderModule vertexShaderModule,
    wgpu::ShaderModule fragmentShaderModule,
    const wgsl::Shader* vertexShader,
    const wgsl::Shader* fragmentShader,
    const gpu::PipelineState& pipelineState)
{
    // The most vertex buffers any draw type binds is the image mesh: position,
    // uv, and the per-instance attribute buffer.
    StackVector<WGPUVertexBufferLayout, 3> vertexBufferLayouts;
    // The image-draw attribs come last, so the most vertex attribs used across
    // all 3 buffers is determined by the final image attrib.
    StackVector<WGPUVertexAttribute, IMAGE_LAST_ATTRIB_IDX + 1> attrs;
    WGPUPrimitiveTopology topology;
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
        case DrawType::msaaOuterCubics:
        case DrawType::msaaStrokes:
        case DrawType::msaaMidpointFanBorrowedCoverage:
        case DrawType::msaaMidpointFans:
        case DrawType::msaaMidpointFanStencilReset:
        case DrawType::msaaMidpointFanPathsStencil:
        case DrawType::msaaMidpointFanPathsCover:
        {
            attrs.push_back({
                .format = WGPUVertexFormat_Float32x4,
                .offset = 0,
                .shaderLocation = 0,
            });
            attrs.push_back({
                .format = WGPUVertexFormat_Float32x4,
                .offset = 4 * sizeof(float),
                .shaderLocation = 1,
            });

            vertexBufferLayouts.push_back(WGPU_VERTEX_BUFFER_LAYOUT_INIT);
            vertexBufferLayouts[0].attributeCount = attrs.size();
            vertexBufferLayouts[0].attributes = attrs.data();
            vertexBufferLayouts[0].arrayStride = sizeof(gpu::PatchVertex);
            vertexBufferLayouts[0].stepMode = WGPUVertexStepMode_Vertex;

            topology = WGPUPrimitiveTopology_TriangleList;
            break;
        }
        case DrawType::clipReset:
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
        {
            attrs.push_back({
                .format = WGPUVertexFormat_Float32x3,
                .offset = 0,
                .shaderLocation = 0,
            });

            vertexBufferLayouts.push_back(WGPU_VERTEX_BUFFER_LAYOUT_INIT);
            vertexBufferLayouts[0].attributeCount = attrs.size();
            vertexBufferLayouts[0].attributes = attrs.data();
            vertexBufferLayouts[0].arrayStride = sizeof(gpu::TriangleVertex);
            vertexBufferLayouts[0].stepMode = WGPUVertexStepMode_Vertex;

            topology = WGPUPrimitiveTopology_TriangleList;
            break;
        }
        case DrawType::imageRect:
        {
            attrs.push_back({
                .format = WGPUVertexFormat_Float32x4,
                .offset = 0,
                .shaderLocation = 0,
            });
            appendImageDrawInstanceAttribs(attrs);

            vertexBufferLayouts.push_back_n(2, WGPU_VERTEX_BUFFER_LAYOUT_INIT);
            vertexBufferLayouts[0].attributeCount = 1;
            vertexBufferLayouts[0].attributes = &attrs[0];
            vertexBufferLayouts[0].arrayStride = sizeof(gpu::ImageRectVertex);
            vertexBufferLayouts[0].stepMode = WGPUVertexStepMode_Vertex;

            assert(attrs.size() == 1 + IMAGE_ATTRIB_COUNT);
            vertexBufferLayouts[1].attributeCount = IMAGE_ATTRIB_COUNT;
            vertexBufferLayouts[1].attributes = &attrs[1];
            vertexBufferLayouts[1].arrayStride = sizeof(gpu::ImageDrawInstance);
            vertexBufferLayouts[1].stepMode = WGPUVertexStepMode_Instance;

            topology = WGPUPrimitiveTopology_TriangleList;
            break;
        }
        case DrawType::imageMesh:
        {
            attrs.push_back({
                .format = WGPUVertexFormat_Float32x2,
                .offset = 0,
                .shaderLocation = 0,
            });
            attrs.push_back({
                .format = WGPUVertexFormat_Float32x2,
                .offset = 0,
                .shaderLocation = 1,
            });
            appendImageDrawInstanceAttribs(attrs);

            vertexBufferLayouts.push_back_n(3, WGPU_VERTEX_BUFFER_LAYOUT_INIT);

            vertexBufferLayouts[0].attributeCount = 1;
            vertexBufferLayouts[0].attributes = &attrs[0];
            vertexBufferLayouts[0].arrayStride = sizeof(float) * 2;
            vertexBufferLayouts[0].stepMode = WGPUVertexStepMode_Vertex;

            vertexBufferLayouts[1].attributeCount = 1;
            vertexBufferLayouts[1].attributes = &attrs[1];
            vertexBufferLayouts[1].arrayStride = sizeof(float) * 2;
            vertexBufferLayouts[1].stepMode = WGPUVertexStepMode_Vertex;

            assert(attrs.size() == 2 + IMAGE_ATTRIB_COUNT);
            vertexBufferLayouts[2].attributeCount = IMAGE_ATTRIB_COUNT;
            vertexBufferLayouts[2].attributes = &attrs[2];
            vertexBufferLayouts[2].arrayStride = sizeof(gpu::ImageDrawInstance);
            vertexBufferLayouts[2].stepMode = WGPUVertexStepMode_Instance;

            topology = WGPUPrimitiveTopology_TriangleList;
            break;
        }
        case DrawType::renderPassInitialize:
        case DrawType::renderPassResolve:
        {
            // No attrs.
            topology = WGPUPrimitiveTopology_TriangleStrip;
            break;
        }
    }

    WGPUChainedStruct* extraColorTargetState = nullptr;
#ifdef RIVE_WAGYU
    const bool usingPLSInputAttachments =
        using_pls(interlockMode) &&
        m_capabilities.plsType ==
            PixelLocalStorageType::VK_EXT_rasterization_order_attachment_access;
    WGPUWagyuColorTargetState wagyuColorTargetState =
        WGPU_WAGYU_COLOR_TARGET_STATE_INIT;
    if (usingPLSInputAttachments)
    {
        // WGPUWagyu needs us to tell it when color attachments are also used as
        // input attachments.
        wagyuColorTargetState.usedAsInput = WGPUOptionalBool_True;
        extraColorTargetState = &wagyuColorTargetState.chain;
    }
#endif

    StackVector<WGPUColorTargetState, PLS_PLANE_COUNT> colorAttachments;

    assert(colorAttachments.size() == COLOR_PLANE_IDX);
    assert(pipelineState.blendEquation == gpu::BlendEquation::none ||
           pipelineState.blendEquation == gpu::BlendEquation::srcOver);
    colorAttachments.push_back({
        .nextInChain = extraColorTargetState,
        .format = static_cast<WGPUTextureFormat>(framebufferFormat),
        .blend = (pipelineState.blendEquation == gpu::BlendEquation::srcOver)
                     ? &BLEND_STATE_SRC_OVER
                     : nullptr,
        .writeMask = pipelineState.colorWriteEnabled ? WGPUColorWriteMask_All
                                                     : WGPUColorWriteMask_None,
    });

#ifdef RIVE_WAGYU
    if (usingPLSInputAttachments)
    {
        assert(colorAttachments.size() == CLIP_PLANE_IDX);
        colorAttachments.push_back({
            .nextInChain = extraColorTargetState,
            .format = WGPUTextureFormat_R32Uint,
            .writeMask = WGPUColorWriteMask_All,
        });

        assert(colorAttachments.size() == SCRATCH_COLOR_PLANE_IDX);
        colorAttachments.push_back({
            .nextInChain = extraColorTargetState,
            .format = static_cast<WGPUTextureFormat>(framebufferFormat),
            .writeMask = WGPUColorWriteMask_All,
        });

        assert(colorAttachments.size() == COVERAGE_PLANE_IDX);
        colorAttachments.push_back({
            .nextInChain = extraColorTargetState,
            .format = WGPUTextureFormat_R32Uint,
            .writeMask = WGPUColorWriteMask_All,
        });
    }
#endif

    // Override constants for the WGSL shaders. Mirrors the SPIRV specialization
    // layout — same indices, same data, just delivered through WebGPU's
    // override mechanism.
    double shaderPermutationFlags[] = {
        static_cast<double>(
            enums::is_flag_set(shaderFeatures,
                               gpu::ShaderFeatures::ENABLE_CLIPPING)),
        static_cast<double>(
            enums::is_flag_set(shaderFeatures,
                               gpu::ShaderFeatures::ENABLE_CLIP_RECT)),
        static_cast<double>(
            enums::is_flag_set(shaderFeatures,
                               gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND)),
        static_cast<double>(
            enums::is_flag_set(shaderFeatures,
                               gpu::ShaderFeatures::ENABLE_FEATHER)),
        static_cast<double>(
            enums::is_flag_set(shaderFeatures,
                               gpu::ShaderFeatures::ENABLE_EVEN_ODD)),
        static_cast<double>(
            enums::is_flag_set(shaderFeatures,
                               gpu::ShaderFeatures::ENABLE_NESTED_CLIPPING)),
        static_cast<double>(
            enums::is_flag_set(shaderFeatures,
                               gpu::ShaderFeatures::ENABLE_HSL_BLEND_MODES)),
        static_cast<double>(
            enums::is_flag_set(shaderFeatures,
                               gpu::ShaderFeatures::ENABLE_DITHER)),
        static_cast<double>(
            enums::is_flag_set(shaderMiscFlags,
                               gpu::ShaderMiscFlags::clockwiseFill)),
        static_cast<double>(
            enums::is_flag_set(shaderMiscFlags,
                               gpu::ShaderMiscFlags::nestedClipUpdateOnly)),
        static_cast<double>(
            enums::is_flag_set(shaderMiscFlags,
                               gpu::ShaderMiscFlags::borrowedCoveragePass)),
        static_cast<double>(
            enums::is_flag_set(shaderMiscFlags,
                               gpu::ShaderMiscFlags::storeColorClear)),
        static_cast<double>(
            enums::is_flag_set(shaderMiscFlags,
                               gpu::ShaderMiscFlags::loadColorFromDstTexture)),
        0.0, // VULKAN_VENDOR_ARM — ignored for now, but we may want to detect
             // this if it we find ARM bugs on WebGPU that get fixed by shader
             // modifications.
    };
    static_assert(std::size(shaderPermutationFlags) == SPECIALIZATION_COUNT);
    static_assert(CLIPPING_SPECIALIZATION_IDX == 0);
    static_assert(CLIP_RECT_SPECIALIZATION_IDX == 1);
    static_assert(ADVANCED_BLEND_SPECIALIZATION_IDX == 2);
    static_assert(FEATHER_SPECIALIZATION_IDX == 3);
    static_assert(EVEN_ODD_SPECIALIZATION_IDX == 4);
    static_assert(NESTED_CLIPPING_SPECIALIZATION_IDX == 5);
    static_assert(HSL_BLEND_MODES_SPECIALIZATION_IDX == 6);
    static_assert(DITHER_SPECIALIZATION_IDX == 7);
    static_assert(CLOCKWISE_FILL_SPECIALIZATION_IDX == 8);
    static_assert(NESTED_CLIP_UPDATE_ONLY_IDX == 9);
    static_assert(BORROWED_COVERAGE_PASS_SPECIALIZATION_IDX == 10);
    static_assert(STORE_COLOR_CLEAR_SPECIALIZATION_IDX == 11);
    static_assert(LOAD_COLOR_FROM_DST_TEXTURE_SPECIALIZATION_IDX == 12);
    static_assert(VULKAN_VENDOR_ARM_SPECIALIZATION_IDX == 13);
    static_assert(SPECIALIZATION_COUNT == 14);

    // Build a per-stage WGPUConstantEntry[] from the shader's own override
    // list.
    auto buildConstants = [&](const wgsl::Shader* shader,
                              WGPUConstantEntry* out) -> size_t {
        if (shader == nullptr)
        {
            return 0;
        }

        constexpr const char* SpecializationIdxIDs[] = {
            "0",
            "1",
            "2",
            "3",
            "4",
            "5",
            "6",
            "7",
            "8",
            "9",
            "10",
            "11",
            "12",
            "13",
        };
        static_assert(std::size(SpecializationIdxIDs) == SPECIALIZATION_COUNT);

        size_t count = 0;
        for (uint32_t i = 0; i < SPECIALIZATION_COUNT; ++i)
        {
            if (!shader->usedOverrides[i])
                continue;
            out[count++] = {
                .nextInChain = nullptr,
                .key = WGPU_STRING_VIEW(SpecializationIdxIDs[i]),
                .value = shaderPermutationFlags[i],
            };
        }

        return count;
    };

    WGPUConstantEntry vertexConstantEntries[SPECIALIZATION_COUNT];
    WGPUConstantEntry fragmentConstantEntries[SPECIALIZATION_COUNT];
    size_t vertexConstantCount =
        buildConstants(vertexShader, vertexConstantEntries);
    size_t fragmentConstantCount =
        buildConstants(fragmentShader, fragmentConstantEntries);

    WGPUFragmentState fragmentState = {
        .module = fragmentShaderModule.Get(),
        .entryPoint = WGPU_STRING_VIEW("main"),
        .constantCount = fragmentConstantCount,
        .constants = fragmentConstantEntries,
        .targetCount = colorAttachments.size(),
        .targets = colorAttachments.data(),
    };

#ifdef RIVE_WAGYU
    WGPUWagyuInputAttachmentState inputAttachments[PLS_PLANE_COUNT];
    WGPUWagyuFragmentState wagyuFragmentState = WGPU_WAGYU_FRAGMENT_STATE_INIT;
    if (usingPLSInputAttachments)
    {
        for (uint32_t i = 0; i < PLS_PLANE_COUNT; ++i)
        {
            inputAttachments[i] = WGPU_WAGYU_INPUT_ATTACHMENT_STATE_INIT;
            inputAttachments[i].format = colorAttachments[i].format;
            inputAttachments[i].usedAsColor = WGPUOptionalBool_True;
        }
        wagyuFragmentState.inputCount = std::size(inputAttachments);
        wagyuFragmentState.inputs = inputAttachments;
        wagyuFragmentState.featureFlags =
            WGPUWagyuFragmentStateFeaturesFlags_RasterizationOrderAttachmentAccess;
        fragmentState.nextInChain = &wagyuFragmentState.chain;
    }
#endif

    const std::string renderPipelineLabel =
        (std::ostringstream()
         << "RIVE_Draw{drawType=" << static_cast<int>(drawType)
         << ",interlockMode=" << static_cast<int>(interlockMode) << '}')
            .str();

    WGPURenderPipelineDescriptor renderPipelineDescriptor = {
        .label = WGPU_STRING_VIEW(renderPipelineLabel.c_str()),
        .layout = drawPipelineLayout(interlockMode).pipelineLayout().Get(),
        .vertex =
            {
                .module = vertexShaderModule.Get(),
                .entryPoint = WGPU_STRING_VIEW("main"),
                .constantCount = vertexConstantCount,
                .constants = vertexConstantEntries,
                .bufferCount = vertexBufferLayouts.size(),
                .buffers = vertexBufferLayouts.dataOrNull(),
            },
        .primitive =
            {
                .topology = topology,
                .frontFace = static_cast<WGPUFrontFace>(RIVE_FRONT_FACE),
                .cullMode = wgpu_cull_mode(pipelineState.cullFace),
            },
        .multisample =
            {
                .count = interlockMode == gpu::InterlockMode::msaa
                             ? MSAA_SAMPLE_COUNT
                             : 1u,
                .mask = 0xffffffff,
            },
        .fragment = &fragmentState,
    };

    WGPUDepthStencilState depthStencilState;
    if (interlockMode == gpu::InterlockMode::msaa)
    {
        depthStencilState = {
            .format = WGPUTextureFormat_Depth24PlusStencil8,
            .depthWriteEnabled = wgpu_bool(pipelineState.depthWriteEnabled),
            .depthCompare = pipelineState.depthTestEnabled
                                ? WGPUCompareFunction_Less
                                : WGPUCompareFunction_Always,
            .stencilFront =
                pipelineState.stencilTestEnabled
                    ? wgpu_stencil_face_state(pipelineState.stencilFrontOps)
                    : STENCIL_FACE_STATE_DISABLED,
            .stencilBack = pipelineState.stencilTestEnabled
                               ? wgpu_stencil_face_state(
                                     pipelineState.stencilDoubleSided
                                         ? pipelineState.stencilBackOps
                                         : pipelineState.stencilFrontOps)
                               : STENCIL_FACE_STATE_DISABLED,
            .stencilReadMask = pipelineState.stencilCompareMask,
            .stencilWriteMask = pipelineState.stencilWriteMask,
        };
        renderPipelineDescriptor.depthStencil = &depthStencilState;
    }

    return wgpu::RenderPipeline::Acquire(
        wgpuDeviceCreateRenderPipeline(m_device.Get(),
                                       &renderPipelineDescriptor));
}

// Base class for the per-flush draw render passes. Owns the live
// wgpu::RenderPassEncoder and the state shared by every InterlockMode (the
// owning impl, the flush descriptor, the render target, and the command
// encoder). Subclasses begin the pass in their constructor and restart it from
// barrier() as the DrawList is walked; state that doesn't change between
// restarts within a flush is cached on the subclass and reused.
class RenderContextWebGPUImpl::DrawRenderPass
{
public:
    virtual ~DrawRenderPass() { end(); }

    // The live render pass encoder. Null before the pass has begun (the MSAA
    // pass defers its begin until the first barrier) and after end().
    const wgpu::RenderPassEncoder& encoder() const { return m_encoder; }

    // Restart the render pass if this batch's barrier(s) require it.
    virtual void barrier(const gpu::DrawBatch&) = 0;

    // End the render pass. No-op if it isn't currently open.
    void end()
    {
        if (m_encoder != nullptr)
        {
            m_encoder.End();
            m_encoder = nullptr;
        }
    }

protected:
    DrawRenderPass(RenderContextWebGPUImpl* impl,
                   const FlushDescriptor& desc,
                   wgpu::CommandEncoder commandEncoder) :
        m_impl(impl),
        m_desc(desc),
        m_renderTarget(static_cast<RenderTargetWebGPU*>(desc.renderTarget)),
        m_commandEncoder(commandEncoder)
    {}

    // Apply the initial render pass state (viewport + sampler bindings) that
    // every draw pass needs to the freshly-begun m_encoder.
    void initDrawRenderPass()
    {
        m_encoder.SetViewport(0.f,
                              0.f,
                              m_renderTarget->width(),
                              m_renderTarget->height(),
                              0.0,
                              1.0);
        m_encoder.SetBindGroup(WEBGPU_SAMPLER_BINDINGS_SET,
                               m_impl->m_samplerBindings);
    }

    RenderContextWebGPUImpl* const m_impl;
    const FlushDescriptor& m_desc;
    RenderTargetWebGPU* const m_renderTarget;
    const wgpu::CommandEncoder m_commandEncoder;
    wgpu::RenderPassEncoder m_encoder;
};

// A Rive render pass that uses pixel local storage (the rasterOrdering,
// clockwise, and clockwiseAtomic modes). These modes resolve PLS dependencies
// in hardware, so the pass is begun once and never restarted: barrier() falls
// through to the no-op base implementation.
class RenderContextWebGPUImpl::PLSDrawRenderPass : public DrawRenderPass
{
public:
    PLSDrawRenderPass(RenderContextWebGPUImpl* impl,
                      const FlushDescriptor& desc,
                      wgpu::CommandEncoder commandEncoder) :
        DrawRenderPass(impl, desc, commandEncoder)
    {
        const auto colorLoadOp =
            (m_desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget)
                ? WGPULoadOp_Load
                : WGPULoadOp_Clear;

        StackVector<WGPURenderPassColorAttachment, PLS_PLANE_COUNT>
            plsAttachments;

        WGPUColor targetClearValue = math::bit_cast<WGPUColor>(
            wgpu_color_premul(m_desc.colorClearValue));

        WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        passDesc.label = WGPU_STRING_VIEW("RIVE_PLS_RenderPass");

        // framebuffer
        assert(plsAttachments.size() == COLOR_PLANE_IDX);
        plsAttachments.push_back({
            .view = m_renderTarget->targetTextureView().Get(),
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
            .loadOp = colorLoadOp,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = targetClearValue,
        });

#ifdef RIVE_WAGYU
        WGPUWagyuRenderPassDescriptor wagyuRenderPassDescriptor =
            WGPU_WAGYU_RENDER_PASS_DESCRIPTOR_INIT;

        StackVector<WGPUWagyuRenderPassInputAttachment, PLS_PLANE_COUNT>
            inputAttachments;

        if (using_pls(m_desc.interlockMode))
        {
            if (m_impl->m_capabilities.plsType ==
                PixelLocalStorageType::
                    VK_EXT_rasterization_order_attachment_access)
            {
                assert(inputAttachments.size() == COLOR_PLANE_IDX);
                inputAttachments.push_back({
                    .view = m_renderTarget->targetTextureView().Get(),
                    .clearValue = &targetClearValue,
                    .loadOp = colorLoadOp,
                    .storeOp = WGPUStoreOp_Store,
                });

                WGPUColor zeroClearValue = {};

                // clip
                assert(plsAttachments.size() == CLIP_PLANE_IDX);
                plsAttachments.push_back({
                    .view = m_renderTarget->clipTextureView().Get(),
                    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                    .loadOp = WGPULoadOp_Clear,
                    .storeOp = WGPUStoreOp_Discard,
                    .clearValue = {},
                });
                assert(inputAttachments.size() == CLIP_PLANE_IDX);
                inputAttachments.push_back({
                    .view = m_renderTarget->clipTextureView().Get(),
                    .clearValue = &zeroClearValue,
                    .loadOp = WGPULoadOp_Clear,
                    .storeOp = WGPUStoreOp_Discard,
                });

                // scratchColor
                assert(plsAttachments.size() == SCRATCH_COLOR_PLANE_IDX);
                plsAttachments.push_back({
                    .view = m_renderTarget->scratchColorTextureView().Get(),
                    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                    .loadOp = WGPULoadOp_Clear,
                    .storeOp = WGPUStoreOp_Discard,
                    .clearValue = {},
                });
                assert(inputAttachments.size() == SCRATCH_COLOR_PLANE_IDX);
                inputAttachments.push_back({
                    .view = m_renderTarget->scratchColorTextureView().Get(),
                    .clearValue = &zeroClearValue,
                    .loadOp = WGPULoadOp_Clear,
                    .storeOp = WGPUStoreOp_Discard,
                });

                // coverage
                assert(plsAttachments.size() == COVERAGE_PLANE_IDX);
                plsAttachments.push_back({
                    .view = m_renderTarget->coverageTextureView().Get(),
                    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                    .loadOp = WGPULoadOp_Clear,
                    .storeOp = WGPUStoreOp_Discard,
                    .clearValue = {},
                });
                assert(inputAttachments.size() == COVERAGE_PLANE_IDX);
                inputAttachments.push_back({
                    .view = m_renderTarget->coverageTextureView().Get(),
                    .clearValue = &zeroClearValue,
                    .loadOp = WGPULoadOp_Clear,
                    .storeOp = WGPUStoreOp_Discard,
                });

                assert(plsAttachments.size() == PLS_PLANE_COUNT);
                assert(inputAttachments.size() == PLS_PLANE_COUNT);

                wagyuRenderPassDescriptor.inputAttachmentCount =
                    inputAttachments.size();
                wagyuRenderPassDescriptor.inputAttachments =
                    inputAttachments.data();
            }
            else if (m_impl->m_capabilities.plsType ==
                     PixelLocalStorageType::GL_EXT_shader_pixel_local_storage)
            {
                wagyuRenderPassDescriptor.pixelLocalStorageEnabled =
                    WGPUOptionalBool_True;
                if (m_desc.fixedFunctionColorOutput)
                {
                    assert(m_impl->m_capabilities
                               .GL_EXT_shader_pixel_local_storage2);
                    wagyuRenderPassDescriptor.pixelLocalStorageSize =
                        2 * sizeof(uint32_t);
                }
            }
            passDesc.nextInChain = &wagyuRenderPassDescriptor.chain;
        }
#endif

        passDesc.colorAttachmentCount = plsAttachments.size();
        passDesc.colorAttachments = plsAttachments.data();

        m_encoder = wgpu::RenderPassEncoder::Acquire(
            wgpuCommandEncoderBeginRenderPass(m_commandEncoder.Get(),
                                              &passDesc));
        initDrawRenderPass();
    }

    void barrier(const gpu::DrawBatch& batch) override { RIVE_UNREACHABLE(); }
};

// A Rive render pass that uses InterlockMode::atomics. It's restarted once per
// "plsAtomic*" barrier.
class RenderContextWebGPUImpl::AtomicDrawRenderPass : public DrawRenderPass
{
public:
    AtomicDrawRenderPass(RenderContextWebGPUImpl* impl,
                         const FlushDescriptor& desc,
                         wgpu::CommandEncoder commandEncoder) :
        DrawRenderPass(impl, desc, commandEncoder)
    {
        wgpu::BindGroupEntry plsBufferBindingEntries[] = {
            {
                .binding = COLOR_PLANE_IDX,
                .buffer = m_desc.fixedFunctionColorOutput
                              ? m_impl->m_nullStorageBuffer // Color isn't used
                                                            // as PLS in FFCO,
                                                            // but bind a null
                                                            // buffer to satisfy
                                                            // layout.
                              : m_impl->atomicPLSColorBuffer(),
            },
            {
                .binding = CLIP_PLANE_IDX,
                .buffer = m_impl->atomicPLSClipBuffer(),
            },
            {
                .binding = COVERAGE_PLANE_IDX,
                .buffer = m_impl->atomicPLSCoverageBuffer(),
            },
        };
        wgpu::BindGroupDescriptor plsBufferBindGroupDesc = {
            .layout = m_impl->drawPipelineLayout(m_desc.interlockMode)
                          .bindGroupLayout(PLS_TEXTURE_BINDINGS_SET),
            .entryCount = std::size(plsBufferBindingEntries),
            .entries = plsBufferBindingEntries,
        };
        m_plsBindings =
            m_impl->m_device.CreateBindGroup(&plsBufferBindGroupDesc);

        // The first draw in atomic mode is always renderPassInitialize.
        begin(gpu::DrawType::renderPassInitialize);
    }

    void barrier(const gpu::DrawBatch& batch) override
    {
        if (enums::any_flag_set(batch.barriers,
                                gpu::BarrierFlags::plsAtomic |
                                    gpu::BarrierFlags::plsAtomicPreResolve))
        {
            assert(batch.drawType != gpu::DrawType::renderPassInitialize);
            end();
            begin(batch.drawType);
        }
    }

private:
    void begin(gpu::DrawType drawType)
    {
        wgpu::RenderPassColorAttachment colorAttachment;
        if (m_desc.fixedFunctionColorOutput ||
            drawType == gpu::DrawType::renderPassResolve)
        {
            // We are rendering to the actual render target as a color
            // attachment.
            colorAttachment = {
                .view = m_renderTarget->targetTextureView(),
                .storeOp = wgpu::StoreOp::Store,
            };

            if (m_desc.fixedFunctionColorOutput)
            {
                // FFCO renders color straight to the renderTarget, so a barrier
                // restart must preserve what this flush drew already.
                colorAttachment.loadOp =
                    (m_desc.colorLoadAction ==
                         gpu::LoadAction::preserveRenderTarget ||
                     drawType != gpu::DrawType::renderPassInitialize)
                        ? wgpu::LoadOp::Load
                        : wgpu::LoadOp::Clear;
                // Only use the real clear color when we actually need to clear
                // the render target. Otherwise (e.g., LoadAction::dontCare),
                // clear to zero. WebGPU doesn't support a "DontCare" LoadOp,
                // and clearing to zero is more likely to hit a fast clear path
                // than clearing to an arbitrary color.
                if (m_desc.colorLoadAction == gpu::LoadAction::clear &&
                    drawType == gpu::DrawType::renderPassInitialize)
                {
                    colorAttachment.clearValue =
                        wgpu_color_premul(m_desc.colorClearValue);
                }
            }
            else
            {
                // This is the !FFCO resolve, which overwrites every pixel
                // within renderTargetUpdateBounds. For
                // LoadAction::preserveRenderTarget, the
                // renderTargetUpdateBounds were copied into the storage buffer
                // at the start of flush, so we only need to preserve when
                // that's smaller than the full render target.
                assert(drawType == gpu::DrawType::renderPassResolve);
                colorAttachment.loadOp =
                    (m_desc.colorLoadAction ==
                         gpu::LoadAction::preserveRenderTarget &&
                     m_desc.renderTargetUpdateBounds !=
                         m_renderTarget->bounds())
                        ? wgpu::LoadOp::Load
                        // Ideally the LoadOp would be "DontCare", but the
                        // quickest we can do in WebGPU is a clear to zero.
                        : wgpu::LoadOp::Clear;
            }
        }
        else
        {
            // In !fixedFunctionColorOutput the atomic shaders write color
            // into the PLS storage buffer, not a color attachment. But WebGPU
            // still requires every render pass to have at least one attachment.
            // Use dstColorTextureView as a throwaway attachment: nothing writes
            // color this pass, so Clear/Discard keeps it ~free (no real load or
            // store).
            colorAttachment = {
                .view = m_renderTarget->dstColorTextureView(),
                // Ideally the LoadOp would be "DontCare", but the quickest we
                // can do in WebGPU is a clear to zero.
                .loadOp = wgpu::LoadOp::Clear,
                .storeOp = wgpu::StoreOp::Discard,
            };
        }

        wgpu::RenderPassDescriptor renderPassDescriptor = {
            .label = "RIVE_Atomic_RenderPass",
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
        };

        // The PLS buffers are initialized in-shader by the
        // DrawType::renderPassInitialize batch. No host-side ClearBuffer /
        // WriteBuffer needed here.

        m_encoder = m_commandEncoder.BeginRenderPass(&renderPassDescriptor);
        initDrawRenderPass();
        m_encoder.SetBindGroup(PLS_TEXTURE_BINDINGS_SET, m_plsBindings);
    }

    wgpu::BindGroup m_plsBindings;
};

// A Rive render pass that uses MSAA. It's restarted once per dstBlend barrier
// (and may defer its initial begin until the first such barrier); the
// attachment texture views don't change between restarts, so they're cached
// once in the constructor and reused.
class RenderContextWebGPUImpl::MSAADrawRenderPass : public DrawRenderPass
{
public:
    MSAADrawRenderPass(RenderContextWebGPUImpl* impl,
                       const FlushDescriptor& desc,
                       wgpu::CommandEncoder commandEncoder) :
        DrawRenderPass(impl, desc, commandEncoder),
        m_msaaColorTextureView(m_renderTarget->msaaColorTextureView()),
        m_targetTextureView(m_renderTarget->targetTextureView()),
        m_msaaDepthStencilTextureView(
            m_renderTarget->msaaDepthStencilTextureView())
    {
        // If we're preserving the render target with a draw, don't begin the
        // render pass yet. We will get a dstBlend barrier on the first draw
        // that does a copy and begins the pass.
        if (m_desc.drawList->empty() || m_desc.drawList->head()->drawType !=
                                            gpu::DrawType::renderPassInitialize)
        {
            begin(MSAABeginType::primary,
                  (m_desc.firstDstBlendBarrier != nullptr)
                      ? MSAAEndType::breakForDstCopy
                      : MSAAEndType::finish);
        }
    }

    void barrier(const gpu::DrawBatch& batch) override
    {
        // For a dstBlend barrier, our only option in unextended WebGPU is to
        // copy out the dst pixels we want to read into a separate texture.
        if (!enums::is_flag_set(batch.barriers, gpu::BarrierFlags::dstBlend))
        {
            return;
        }
        assert(!m_desc.fixedFunctionColorOutput ||
               batch.drawType == gpu::DrawType::renderPassInitialize);

        MSAABeginType msaaBeginType;
        if (batch.drawType == gpu::DrawType::renderPassInitialize)
        {
            assert(m_encoder == nullptr);
            assert(m_desc.colorLoadAction ==
                   gpu::LoadAction::preserveRenderTarget);

            // Copy out the entire target texture in order to seed the MSAA
            // color buffer. We can't seed from the target texture itself
            // because it's also the resolveTarget.
            m_renderTarget->copyTargetToDstColorTexture(
                m_commandEncoder,
                m_renderTarget->bounds());

            msaaBeginType = MSAABeginType::primary;
        }
        else
        {
            // Break the render pass to copy out a dst texture.
            end();

            // Copy the resolve texture into yet another "dstRead" texture for
            // shaders to read.
            for (const Draw* draw = batch.dstReadList; draw != nullptr;
                 draw = draw->nextDstRead())
            {
                assert(draw->blendMode() != BlendMode::srcOver);
                m_renderTarget->copyTargetToDstColorTexture(
                    m_commandEncoder,
                    m_desc.renderTargetUpdateBounds.intersect(
                        draw->pixelBounds()));
            }

            msaaBeginType = MSAABeginType::restartAfterDstCopy;
        }

        // Restart the render pass after the copies are finished.
        begin(msaaBeginType,
              (batch.nextDstBlendBarrier != nullptr)
                  ? MSAAEndType::breakForDstCopy
                  : MSAAEndType::finish);
    }

private:
    // Specifies how to load MSAA color/depth/stencil attachments when beginning
    // an MSAA render pass.
    enum class MSAABeginType : bool
    {
        primary,
        restartAfterDstCopy,
    };

    void begin(MSAABeginType msaaBeginType, MSAAEndType msaaEndType)
    {
        // Our MSAA buffers are treated as completely transient (i.e.,
        // Clear/Discard) unless we have to do render pass breaks for dst
        // copies. For LoadAction::preserveRenderTarget, we manually draw the
        // old content into the transient MSAA buffer, so we still load with
        // Clear.
        // TODO: wgpu::LoadOp::ExpandResolveTexture for the color buffer when
        // supported.
        const auto msaaLoadOp =
            msaaBeginType == MSAABeginType::restartAfterDstCopy
                ? wgpu::LoadOp::Load
                : wgpu::LoadOp::Clear;
        const auto msaaStoreOp = (msaaEndType == MSAAEndType::breakForDstCopy)
                                     ? wgpu::StoreOp::Store
                                     : wgpu::StoreOp::Discard;

        wgpu::RenderPassColorAttachment msaaColorAttachment = {
            .view = m_msaaColorTextureView,
            .resolveTarget = m_targetTextureView.Get(),
            .loadOp = msaaLoadOp,
            .storeOp = msaaStoreOp,
            .clearValue = wgpu_color_premul(m_desc.colorClearValue),
        };

        wgpu::RenderPassDepthStencilAttachment msaaDepthStencilAttachment = {
            .view = m_msaaDepthStencilTextureView,
            .depthLoadOp = msaaLoadOp,
            .depthStoreOp = msaaStoreOp,
            .depthClearValue = m_desc.depthClearValue,
            .depthReadOnly = false,
            .stencilLoadOp = msaaLoadOp,
            .stencilStoreOp = msaaStoreOp,
            .stencilClearValue = m_desc.stencilClearValue,
            .stencilReadOnly = false,
        };

        wgpu::RenderPassDescriptor renderPassDescriptor = {
            .label = "RIVE_MSAA_RenderPass",
            .colorAttachmentCount = 1,
            .colorAttachments = &msaaColorAttachment,
            .depthStencilAttachment = &msaaDepthStencilAttachment,
        };

        m_encoder = m_commandEncoder.BeginRenderPass(&renderPassDescriptor);
        initDrawRenderPass();
    }

    const wgpu::TextureView m_msaaColorTextureView;
    const wgpu::TextureView m_targetTextureView;
    const wgpu::TextureView m_msaaDepthStencilTextureView;
};

std::unique_ptr<RenderContextWebGPUImpl::DrawRenderPass>
RenderContextWebGPUImpl::makeDrawRenderPass(const FlushDescriptor& desc,
                                            wgpu::CommandEncoder commandEncoder)
{
    if (desc.interlockMode == gpu::InterlockMode::atomics)
    {
        return std::make_unique<AtomicDrawRenderPass>(this,
                                                      desc,
                                                      commandEncoder);
    }
    if (desc.interlockMode == gpu::InterlockMode::msaa)
    {
        return std::make_unique<MSAADrawRenderPass>(this, desc, commandEncoder);
    }
    return std::make_unique<PLSDrawRenderPass>(this, desc, commandEncoder);
}

static wgpu::Buffer webgpu_buffer(const BufferRing* bufferRing)
{
    assert(bufferRing != nullptr);
    return static_cast<const BufferWebGPU*>(bufferRing)->submittedBuffer();
}

template <typename HighLevelStruct>
void update_webgpu_storage_texture(const BufferRing* bufferRing,
                                   size_t bindingCount,
                                   size_t firstElement,
                                   wgpu::CommandEncoder commandEncoder)
{
    assert(bufferRing != nullptr);
    auto storageTextureBuffer =
        static_cast<const StorageTextureBufferWebGPU*>(bufferRing);
    storageTextureBuffer->updateTextureFromBuffer(
        bindingCount * sizeof(HighLevelStruct),
        firstElement * sizeof(HighLevelStruct),
        commandEncoder);
}

wgpu::TextureView webgpu_storage_texture_view(const BufferRing* bufferRing)
{
    assert(bufferRing != nullptr);
    return static_cast<const StorageTextureBufferWebGPU*>(bufferRing)
        ->textureView();
}

void RenderContextWebGPUImpl::flush(const FlushDescriptor& desc)
{
    auto* const renderTarget =
        static_cast<RenderTargetWebGPU*>(desc.renderTarget);
    const auto& drawPipelineLayout =
        this->drawPipelineLayout(desc.interlockMode);

    wgpu::CommandEncoder commandEncoder;
    {
        auto wgpuEncoder =
            static_cast<WGPUCommandEncoder>(desc.externalCommandBuffer);
        assert(wgpuEncoder != nullptr);
#if (defined(RIVE_WEBGPU) && RIVE_WEBGPU > 1) || defined(RIVE_DAWN)
        wgpuCommandEncoderAddRef(wgpuEncoder);
#else
        wgpuCommandEncoderReference(wgpuEncoder);
#endif
        commandEncoder = wgpu::CommandEncoder::Acquire(wgpuEncoder);
    }

    // If storage buffers are disabled, copy their contents to textures.
    if (m_capabilities.polyfillVertexStorageBuffers)
    {
        if (desc.pathCount > 0)
        {
            update_webgpu_storage_texture<gpu::PathData>(pathBufferRing(),
                                                         desc.pathCount,
                                                         desc.firstPath,
                                                         commandEncoder);
            update_webgpu_storage_texture<gpu::PaintData>(paintBufferRing(),
                                                          desc.pathCount,
                                                          desc.firstPaint,
                                                          commandEncoder);
            update_webgpu_storage_texture<gpu::PaintAuxData>(
                paintAuxBufferRing(),
                desc.pathCount,
                desc.firstPaintAux,
                commandEncoder);
        }
        if (desc.contourCount > 0)
        {
            update_webgpu_storage_texture<gpu::ContourData>(contourBufferRing(),
                                                            desc.contourCount,
                                                            desc.firstContour,
                                                            commandEncoder);
        }
    }

    wgpu::BindGroupEntry perFlushBindingEntries[DRAW_BINDINGS_COUNT] = {
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .buffer = webgpu_buffer(flushUniformBufferRing()),
            .offset = desc.flushUniformDataOffsetInBytes,
        },
        m_capabilities.polyfillVertexStorageBuffers
            ? wgpu::BindGroupEntry{.binding = PATH_BUFFER_IDX,
                                   .textureView = webgpu_storage_texture_view(
                                       pathBufferRing())}
            :
            wgpu::BindGroupEntry{
                .binding = PATH_BUFFER_IDX,
                .buffer = webgpu_buffer(pathBufferRing()),
                .offset = desc.firstPath * sizeof(gpu::PathData),
            },
        m_capabilities.polyfillVertexStorageBuffers ?
            wgpu::BindGroupEntry{
                .binding = PAINT_BUFFER_IDX,
                .textureView = webgpu_storage_texture_view(paintBufferRing()),
            } :
            wgpu::BindGroupEntry{
                .binding = PAINT_BUFFER_IDX,
                .buffer = webgpu_buffer(paintBufferRing()),
                .offset = desc.firstPaint * sizeof(gpu::PaintData),
            },
        m_capabilities.polyfillVertexStorageBuffers ?
            wgpu::BindGroupEntry{
                .binding = PAINT_AUX_BUFFER_IDX,
                .textureView = webgpu_storage_texture_view(paintAuxBufferRing()),
            } :
            wgpu::BindGroupEntry{
                .binding = PAINT_AUX_BUFFER_IDX,
                .buffer = webgpu_buffer(paintAuxBufferRing()),
                .offset = desc.firstPaintAux * sizeof(gpu::PaintAuxData),
            },
        m_capabilities.polyfillVertexStorageBuffers ?
            wgpu::BindGroupEntry{
                .binding = CONTOUR_BUFFER_IDX,
                .textureView = webgpu_storage_texture_view(contourBufferRing()),
            } :
            wgpu::BindGroupEntry{
                .binding = CONTOUR_BUFFER_IDX,
                .buffer = webgpu_buffer(contourBufferRing()),
                .offset = desc.firstContour * sizeof(gpu::ContourData),
            },
        {
            .binding = FEATHER_TEXTURE_IDX,
            .textureView = m_featherTextureView,
        },
        {
            .binding = TESS_VERTEX_TEXTURE_IDX,
            .textureView = m_tessVertexTextureView,
        },
        {
            .binding = ATLAS_TEXTURE_IDX,
            .textureView = m_atlasTextureView,
        },
        {
            .binding = GRAD_TEXTURE_IDX,
            .textureView = m_gradientTextureView,
        },
        {
            .binding = DST_COLOR_TEXTURE_IDX,
            .textureView = desc.interlockMode == gpu::InterlockMode::msaa &&
                           !desc.fixedFunctionColorOutput
                               ? renderTarget->dstColorTextureView()
                               : m_nullTextureView,
        },
    };

    // Render the complex color ramps to the gradient texture.
    if (desc.gradDataHeight > 0)
    {
        wgpu::BindGroupDescriptor colorRampBindGroupDesc = {
            .layout = m_colorRampPipeline->bindGroupLayout(),
            .entryCount = COLOR_RAMP_BINDINGS_COUNT,
            .entries = perFlushBindingEntries,
        };

        wgpu::RenderPassColorAttachment attachment = {
            .view = m_gradientTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {},
        };

        wgpu::RenderPassDescriptor gradPassDesc = {
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
        };

        wgpu::RenderPassEncoder gradPass =
            commandEncoder.BeginRenderPass(&gradPassDesc);
        gradPass.SetViewport(0.f,
                             0.f,
                             gpu::kGradTextureWidth,
                             static_cast<float>(desc.gradDataHeight),
                             0.0,
                             1.0);
        gradPass.SetPipeline(m_colorRampPipeline->renderPipeline());
        gradPass.SetVertexBuffer(0,
                                 webgpu_buffer(gradSpanBufferRing()),
                                 desc.firstGradSpan *
                                     sizeof(gpu::GradientSpan));
        gradPass.SetBindGroup(
            0,
            m_device.CreateBindGroup(&colorRampBindGroupDesc));
        gradPass.Draw(gpu::GRAD_SPAN_TRI_STRIP_VERTEX_COUNT,
                      desc.gradSpanCount,
                      0,
                      0);
        gradPass.End();
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        wgpu::BindGroupDescriptor tessBindGroupDesc = {
            .layout = m_tessellatePipeline->perFlushBindingsLayout(),
            .entryCount = TESS_BINDINGS_COUNT,
            .entries = perFlushBindingEntries,
        };

        wgpu::RenderPassColorAttachment attachment{
            .view = m_tessVertexTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {},
        };

        wgpu::RenderPassDescriptor tessPassDesc = {
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
        };

        wgpu::RenderPassEncoder tessPass =
            commandEncoder.BeginRenderPass(&tessPassDesc);
        tessPass.SetViewport(0.f,
                             0.f,
                             gpu::kTessTextureWidth,
                             static_cast<float>(desc.tessDataHeight),
                             0.0,
                             1.0);
        tessPass.SetPipeline(m_tessellatePipeline->renderPipeline());
        tessPass.SetVertexBuffer(0,
                                 webgpu_buffer(tessSpanBufferRing()),
                                 desc.firstTessVertexSpan *
                                     sizeof(gpu::TessVertexSpan));
        tessPass.SetIndexBuffer(m_tessSpanIndexBuffer,
                                wgpu::IndexFormat::Uint16);
        tessPass.SetBindGroup(PER_FLUSH_BINDINGS_SET,
                              m_device.CreateBindGroup(&tessBindGroupDesc));
        tessPass.SetBindGroup(WEBGPU_SAMPLER_BINDINGS_SET, m_samplerBindings);
        tessPass.DrawIndexed(std::size(gpu::kTessSpanIndices),
                             desc.tessVertexSpanCount,
                             0,
                             0,
                             0);
        tessPass.End();
    }

    // Render the atlas if we have any offscreen feathers.
    if ((desc.atlasFillBatchCount | desc.atlasStrokeBatchCount) != 0)
    {
        wgpu::BindGroupDescriptor atlasBindGroupDesc = {
            .layout = m_atlasPipeline->perFlushBindingsLayout(),
            .entryCount = ATLAS_BINDINGS_COUNT,
            .entries = perFlushBindingEntries,
        };

        wgpu::RenderPassColorAttachment attachment{
            .view = m_atlasTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {0, 0, 0, 0},
        };

        wgpu::RenderPassDescriptor atlasPassDesc = {
            .colorAttachmentCount = 1,
            .colorAttachments = &attachment,
        };

        wgpu::RenderPassEncoder atlasPass =
            commandEncoder.BeginRenderPass(&atlasPassDesc);
        atlasPass.SetViewport(0.f,
                              0.f,
                              desc.atlasContentWidth,
                              desc.atlasContentHeight,
                              0.0,
                              1.0);
        atlasPass.SetVertexBuffer(0, m_pathPatchVertexBuffer);
        atlasPass.SetIndexBuffer(m_pathPatchIndexBuffer,
                                 wgpu::IndexFormat::Uint16);
        atlasPass.SetBindGroup(PER_FLUSH_BINDINGS_SET,
                               m_device.CreateBindGroup(&atlasBindGroupDesc));
        atlasPass.SetBindGroup(WEBGPU_SAMPLER_BINDINGS_SET, m_samplerBindings);

        if (desc.atlasFillBatchCount != 0)
        {
            atlasPass.SetPipeline(m_atlasPipeline->fillPipeline());
            for (size_t i = 0; i < desc.atlasFillBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& fillBatch = desc.atlasFillBatches[i];
                atlasPass.SetScissorRect(fillBatch.scissor.left,
                                         fillBatch.scissor.top,
                                         fillBatch.scissor.width(),
                                         fillBatch.scissor.height());
                atlasPass.DrawIndexed(gpu::kMidpointFanCenterAAPatchIndexCount,
                                      fillBatch.patchCount,
                                      gpu::kMidpointFanCenterAAPatchBaseIndex,
                                      0,
                                      fillBatch.basePatch);
            }
        }

        if (desc.atlasStrokeBatchCount != 0)
        {
            atlasPass.SetPipeline(m_atlasPipeline->strokePipeline());
            for (size_t i = 0; i < desc.atlasStrokeBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& strokeBatch =
                    desc.atlasStrokeBatches[i];
                atlasPass.SetScissorRect(strokeBatch.scissor.left,
                                         strokeBatch.scissor.top,
                                         strokeBatch.scissor.width(),
                                         strokeBatch.scissor.height());
                atlasPass.DrawIndexed(gpu::kMidpointFanPatchBorderIndexCount,
                                      strokeBatch.patchCount,
                                      gpu::kMidpointFanPatchBaseIndex,
                                      0,
                                      strokeBatch.basePatch);
            }
        }

        atlasPass.End();
    }

    // The render pass owns its encoder and restarts itself on barriers (see the
    // DrawList loop below). drawEncoder mirrors its current encoder, refreshed
    // after each restart, and is the handle the draw commands issue through.
    std::unique_ptr<DrawRenderPass> renderPass =
        makeDrawRenderPass(desc, commandEncoder);
    wgpu::RenderPassEncoder drawEncoder = renderPass->encoder();

#ifdef RIVE_WAGYU
    const bool usingInputAttachmentBindings =
        using_pls(desc.interlockMode) &&
        m_capabilities.plsType ==
            PixelLocalStorageType::VK_EXT_rasterization_order_attachment_access;
    if (usingInputAttachmentBindings)
    {
        wgpu::BindGroupEntry plsTextureBindingEntries[] = {
            {
                .binding = COLOR_PLANE_IDX,
                .textureView = renderTarget->m_targetTextureView,
            },
            {
                .binding = CLIP_PLANE_IDX,
                .textureView = renderTarget->clipTextureView(),
            },
            {
                .binding = SCRATCH_COLOR_PLANE_IDX,
                .textureView = renderTarget->scratchColorTextureView(),
            },
            {
                .binding = COVERAGE_PLANE_IDX,
                .textureView = renderTarget->coverageTextureView(),
            },
        };

        wgpu::BindGroupDescriptor plsTextureBindingsGroupDesc = {
            .layout =
                drawPipelineLayout.bindGroupLayout(PLS_TEXTURE_BINDINGS_SET),
            .entryCount = std::size(plsTextureBindingEntries),
            .entries = plsTextureBindingEntries,
        };

        wgpu::BindGroup plsTextureBindings =
            m_device.CreateBindGroup(&plsTextureBindingsGroupDesc);
        drawEncoder.SetBindGroup(PLS_TEXTURE_BINDINGS_SET, plsTextureBindings);
    }
#endif

#ifdef RIVE_WAGYU
    const bool usingShaderPixelLocalStorageEXT =
        using_pls(desc.interlockMode) &&
        m_capabilities.plsType ==
            PixelLocalStorageType::GL_EXT_shader_pixel_local_storage;
    if (usingShaderPixelLocalStorageEXT)
    {
        if (desc.fixedFunctionColorOutput)
        {
            // Clear PLS using the EXT_shader_pixel_local_storage2 API.
            assert(m_capabilities.GL_EXT_shader_pixel_local_storage2);
            uint32_t plsClearValues[2] = {
                desc.coverageClearValue,
                /*clipClearValue=*/0u,
            };
            wgpuWagyuRenderPassEncoderClearPixelLocalStorage(
                drawEncoder.Get(),
                0,
                std::size(plsClearValues),
                plsClearValues);
        }
        else
        {
            // EXT_shader_pixel_local_storage doesn't have an initialization
            // API. Initialize PLS by drawing a fullscreen quad.
            std::array<float, 4> clearColor;
            LoadStoreActionsEXT loadActions =
                gpu::BuildLoadActionsEXT(desc, &clearColor);
            const LoadStoreEXTPipeline& loadPipeline =
                m_loadStoreEXTPipelines
                    .try_emplace(LoadStoreEXTPipeline::Key(
                                     loadActions,
                                     renderTarget->framebufferFormat()),
                                 this,
                                 loadActions,
                                 renderTarget->framebufferFormat())
                    .first->second;

            if (enums::is_flag_set(loadActions,
                                   LoadStoreActionsEXT::clearColor))
            {
                void* uniformData =
                    m_loadStoreEXTUniforms->mapBuffer(sizeof(clearColor));
                memcpy(uniformData, clearColor.data(), sizeof(clearColor));
                m_loadStoreEXTUniforms->unmapAndSubmitBuffer();

                wgpu::BindGroupEntry uniformBindingEntries[] = {
                    {
                        .binding = 0,
                        .buffer = webgpu_buffer(m_loadStoreEXTUniforms.get()),
                    },
                };

                wgpu::BindGroupDescriptor uniformBindGroupDesc = {
                    .layout = loadPipeline.bindGroupLayout(),
                    .entryCount = std::size(uniformBindingEntries),
                    .entries = uniformBindingEntries,
                };

                wgpu::BindGroup uniformBindings =
                    m_device.CreateBindGroup(&uniformBindGroupDesc);
                drawEncoder.SetBindGroup(0, uniformBindings);
            }

            drawEncoder.SetPipeline(
                loadPipeline.renderPipeline(renderTarget->framebufferFormat()));
            drawEncoder.Draw(4);
        }
    }
#endif

    wgpu::BindGroupDescriptor perFlushBindGroupDesc = {
        .layout = drawPipelineLayout.bindGroupLayout(PER_FLUSH_BINDINGS_SET),
        .entryCount = DRAW_BINDINGS_COUNT,
        .entries = perFlushBindingEntries,
    };

    wgpu::BindGroup perFlushBindings =
        m_device.CreateBindGroup(&perFlushBindGroupDesc);

    // The drawEncoder isn't necessarily created yet. (e.g., MSAA sometimes
    // defers creation of the drawEncoder until the first barrier.) So defer
    // binding the per-flush uniforms until we know the drawEncoder is valid.
    bool needsPerFlushBindings = true;

    wgpu::TextureView boundImageTextureView = {};
    const ImageSampler* boundImageSampler;

    // Execute the DrawList.
    for (const DrawBatch& batch : *desc.drawList)
    {
        DrawType drawType = batch.drawType;

        if (batch.barriers != gpu::BarrierFlags::none)
        {
            renderPass->barrier(batch);
            if (renderPass->encoder().Get() != drawEncoder.Get())
            {
                // The renderPass had to restart itself in order to implement
                // the barrier. Make sure to give the new drawEncoder our
                // current bindings.
                drawEncoder = renderPass->encoder();
                needsPerFlushBindings = true;
                boundImageTextureView = {};
            }
        }

        if (needsPerFlushBindings)
        {
            drawEncoder.SetBindGroup(PER_FLUSH_BINDINGS_SET,
                                     perFlushBindings,
                                     0,
                                     nullptr);
            needsPerFlushBindings = false;
        }

        // Bind the appropriate image texture, if any.
        wgpu::TextureView imageTextureView = m_nullTextureView;
        if (auto imageTexture =
                static_cast<const TextureWebGPUImpl*>(batch.imageTexture))
        {
            imageTextureView = imageTexture->textureView();
        }
        else if (drawType == gpu::DrawType::renderPassInitialize &&
                 desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget)
        {
            if ((desc.interlockMode == gpu::InterlockMode::atomics &&
                 !desc.fixedFunctionColorOutput))
            {
                // The atomic renderPassInitialize pass binds a throwaway color
                // attachment (not the target), so the target texture is free to
                // be sampled here to seed the color buffer. No dst copy needed.
                imageTextureView = renderTarget->targetTextureView();
            }
            else if (desc.interlockMode == gpu::InterlockMode::msaa)
            {
                // MSAA can't sample the target texture here because it's bound
                // as the resolve target, so it seeds from the dstColorTexture
                // (copied from the framebuffer previously) instead.
                imageTextureView = renderTarget->dstColorTextureView();
            }
        }
        if (boundImageTextureView.Get() != imageTextureView.Get() ||
            *boundImageSampler != batch.imageSampler)
        {
            wgpu::BindGroupEntry perDrawBindingEntries[] = {
                {
                    .binding = IMAGE_TEXTURE_IDX,
                    .textureView = imageTextureView,
                },
                {
                    .binding = WEBGPU_IMAGE_SAMPLER_IDX,
                    .sampler = m_imageSamplers[batch.imageSampler.asKey()],
                },
            };

            wgpu::BindGroupDescriptor perDrawBindGroupDesc = {
                .layout =
                    drawPipelineLayout.bindGroupLayout(PER_DRAW_BINDINGS_SET),
                .entryCount = std::size(perDrawBindingEntries),
                .entries = perDrawBindingEntries,
            };

            wgpu::BindGroup perDrawBindings =
                m_device.CreateBindGroup(&perDrawBindGroupDesc);
            drawEncoder.SetBindGroup(PER_DRAW_BINDINGS_SET,
                                     perDrawBindings,
                                     0,
                                     nullptr);

            boundImageTextureView = std::move(imageTextureView);
            boundImageSampler = &batch.imageSampler;
        }

        // Setup the pipeline for this specific drawType and shaderFeatures.
        bool targetIsGLFBO0 = false;
#ifdef RIVE_WAGYU
        if (m_capabilities.backendType == wgpu::BackendType::OpenGLES)
        {
            targetIsGLFBO0 = wgpuWagyuTextureIsSwapchain(
                renderTarget->m_targetTexture.Get());
        }
#endif
        const gpu::ShaderFeatures shaderFeatures =
            desc.interlockMode == gpu::InterlockMode::atomics
                ? desc.combinedShaderFeatures
                : batch.shaderFeatures;

        // For atomic renderPassInitialize, augment the batch's miscFlags
        // with which color-init branch the shader should run, based on
        // the flush's colorLoadAction. These become spec-const values that
        // gate STORE_COLOR_CLEAR / LOAD_COLOR_FROM_DST_TEXTURE inside the
        // shader.
        gpu::ShaderMiscFlags shaderMiscFlags = batch.shaderMiscFlags;
        if (desc.interlockMode == gpu::InterlockMode::atomics &&
            !desc.fixedFunctionColorOutput)
        {
            if (drawType == gpu::DrawType::renderPassInitialize)
            {
                if (desc.colorLoadAction == gpu::LoadAction::clear)
                {
                    shaderMiscFlags |= gpu::ShaderMiscFlags::storeColorClear;
                }
                else if (desc.colorLoadAction ==
                         gpu::LoadAction::preserveRenderTarget)
                {
                    shaderMiscFlags |=
                        gpu::ShaderMiscFlags::loadColorFromDstTexture;
                }
            }
            else if (drawType == gpu::DrawType::renderPassResolve)
            {
                // Since we use storage buffers for atomic PLS, WebGPU always
                // does coalesced resolve & transfer when not using
                // fixedFunctionColorOutput.
                shaderMiscFlags |=
                    gpu::ShaderMiscFlags::coalescedResolveAndTransfer;
            }
        }

        gpu::PipelineState pipelineState;
        gpu::get_pipeline_state(batch,
                                desc,
                                m_platformFeatures,
                                &pipelineState);
        if (desc.interlockMode == gpu::InterlockMode::atomics &&
            !desc.fixedFunctionColorOutput)
        {
            // atomic mode without fixedFunctionColorOutput renders to storage
            // buffers, so colorWriteEnabled is false. We turn it back on when
            // using coalescedResolveAndTransfer because that's the only case
            // where we render directly to the color buffer.
            assert(!pipelineState.colorWriteEnabled);
            if (drawType == gpu::DrawType::renderPassResolve)
            {
                assert(enums::is_flag_set(
                    shaderMiscFlags,
                    gpu::ShaderMiscFlags::coalescedResolveAndTransfer));
                pipelineState.colorWriteEnabled = true;
            }
        }
#ifdef RIVE_WAGYU
        else if (using_pls(desc.interlockMode) &&
                 !desc.fixedFunctionColorOutput)
        {
            // PLS render modes disable color writes by default on !FFCO, but
            // since we implement PLS via color attachments in Wagyu, we turn it
            // back on.
            assert(!pipelineState.colorWriteEnabled);
            pipelineState.colorWriteEnabled = true;
        }
#endif

        uint64_t pipelineKey =
            gpu::pipeline_unique_key(drawType,
                                     shaderFeatures,
                                     desc.interlockMode,
                                     shaderMiscFlags,
                                     batch.drawContents,
                                     desc.fixedFunctionColorOutput,
                                     batch.firstBlendMode,
                                     platformFeatures());

        pipelineKey = math::add_bits_to_key(pipelineKey, targetIsGLFBO0, 1);

        const DrawPipeline& drawPipeline = m_drawPipelines
                                               .try_emplace(pipelineKey,
                                                            this,
                                                            drawType,
                                                            shaderFeatures,
                                                            desc.interlockMode,
                                                            shaderMiscFlags,
                                                            pipelineState,
                                                            targetIsGLFBO0)
                                               .first->second;
        drawEncoder.SetPipeline(
            drawPipeline.renderPipeline(renderTarget->framebufferFormat()));
        if (pipelineState.stencilTestEnabled)
        {
            drawEncoder.SetStencilReference(pipelineState.stencilReference);
        }

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            {
                // Draw PLS patches that connect the tessellation vertices.
                drawEncoder.SetVertexBuffer(0, m_pathPatchVertexBuffer);
                drawEncoder.SetIndexBuffer(m_pathPatchIndexBuffer,
                                           wgpu::IndexFormat::Uint16);
                drawEncoder.DrawIndexed(batch.indexCountPerInstance,
                                        batch.elementCount,
                                        batch.baseIndex,
                                        0,
                                        batch.baseElement);
                break;
            }

            case DrawType::clipReset:
            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
            {
                drawEncoder.SetVertexBuffer(
                    0,
                    webgpu_buffer(triangleBufferRing()));
                drawEncoder.Draw(batch.elementCount, 1, batch.baseElement);
                break;
            }

            case DrawType::imageRect:
            {
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                drawEncoder.SetVertexBuffer(0, m_imageRectVertexBuffer);
                // Select the batch's fistInstance by offsetting the buffer
                // binding. We do this rather than "firstInstance=baseElement"
                // on the draw call because baseInstance support isn't portable
                // on GL, and we have seen drivers with bugs in their emulation.
                drawEncoder.SetVertexBuffer(
                    1,
                    webgpu_buffer(imageDrawInstanceBufferRing()),
                    batch.baseElement * sizeof(gpu::ImageDrawInstance));
                drawEncoder.SetIndexBuffer(m_imageRectIndexBuffer,
                                           wgpu::IndexFormat::Uint16);
                drawEncoder.DrawIndexed(batch.indexCountPerInstance,
                                        batch.elementCount,
                                        batch.baseIndex,
                                        0,
                                        0);
                break;
            }

            case DrawType::imageMesh:
            {
                auto vertexBuffer = static_cast<const RenderBufferWebGPUImpl*>(
                    batch.vertexBuffer);
                auto uvBuffer =
                    static_cast<const RenderBufferWebGPUImpl*>(batch.uvBuffer);
                auto indexBuffer = static_cast<const RenderBufferWebGPUImpl*>(
                    batch.indexBuffer);
                drawEncoder.SetVertexBuffer(0, vertexBuffer->submittedBuffer());
                drawEncoder.SetVertexBuffer(1, uvBuffer->submittedBuffer());
                // Select the batch's fistInstance by offsetting the buffer
                // binding. We do this rather than "firstInstance=baseElement"
                // on the draw call because baseInstance support isn't portable
                // on GL, and we have seen drivers with bugs in their emulation.
                drawEncoder.SetVertexBuffer(
                    2,
                    webgpu_buffer(imageDrawInstanceBufferRing()),
                    batch.baseElement * sizeof(gpu::ImageDrawInstance));
                drawEncoder.SetIndexBuffer(indexBuffer->submittedBuffer(),
                                           wgpu::IndexFormat::Uint16);
                drawEncoder.DrawIndexed(batch.indexCountPerInstance,
                                        batch.elementCount,
                                        batch.baseIndex,
                                        0,
                                        0);
                break;
            }

            case DrawType::renderPassInitialize:
            case DrawType::renderPassResolve:
                drawEncoder.Draw(4);
                break;
        }
    }

#ifdef RIVE_WAGYU
    if (usingShaderPixelLocalStorageEXT && !desc.fixedFunctionColorOutput)
    {
        // EXT_shader_pixel_local_storage doesn't support concurrent rendering
        // to PLS and the framebuffer. Now that we're done, issue a fullscreen
        // draw that transfers the color information from PLS to the main
        // framebuffer.
        LoadStoreActionsEXT storeActions = LoadStoreActionsEXT::storeColor;
        auto it = m_loadStoreEXTPipelines.try_emplace(
            LoadStoreEXTPipeline::Key(storeActions,
                                      renderTarget->framebufferFormat()),
            this,
            storeActions,
            renderTarget->framebufferFormat());
        LoadStoreEXTPipeline* storePipeline = &it.first->second;
        drawEncoder.SetPipeline(
            storePipeline->renderPipeline(renderTarget->framebufferFormat()));
        drawEncoder.Draw(4);
    }
#endif
}

std::unique_ptr<RenderContext> RenderContextWebGPUImpl::MakeContext(
    wgpu::Adapter adapter,
    wgpu::Device device,
    wgpu::Queue queue,
    const ContextOptions& contextOptions)
{
    auto impl = std::unique_ptr<RenderContextWebGPUImpl>(
        new RenderContextWebGPUImpl(adapter, device, queue, contextOptions));
    impl->initGPUObjects();
    return std::make_unique<RenderContext>(std::move(impl));
}

void* RenderContextWebGPUImpl::makeCommandBuffer()
{
    wgpu::CommandEncoder encoder = m_device.CreateCommandEncoder();
    // Release the C++ wrapper's ref and return the raw handle.
    // The caller (commitCommandBuffer) will re-acquire it.
    return encoder.MoveToCHandle();
}

void RenderContextWebGPUImpl::commitCommandBuffer(void* commandBuffer)
{
    if (commandBuffer == nullptr)
    {
        return;
    }
    // Re-acquire ownership of the raw handle.
    wgpu::CommandEncoder encoder = wgpu::CommandEncoder::Acquire(
        static_cast<WGPUCommandEncoder>(commandBuffer));
    wgpu::CommandBuffer commands = encoder.Finish();
    m_queue.Submit(1, &commands);
}
} // namespace rive::gpu

/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"

#include "rive/renderer/draw.hpp"
#include "rive/renderer/stack_vector.hpp"
#include "shaders/constants.glsl"

#include <sstream>
#include <string>

namespace spirv
{
#include "generated/shaders/spirv/blit_texture_as_draw_filtered.vert.h"
#include "generated/shaders/spirv/blit_texture_as_draw_filtered.frag.h"
#include "generated/shaders/spirv/color_ramp.vert.h"
#include "generated/shaders/spirv/color_ramp.frag.h"
#include "generated/shaders/spirv/tessellate.vert.h"
#include "generated/shaders/spirv/tessellate.frag.h"
#include "generated/shaders/spirv/render_atlas.vert.h"
#include "generated/shaders/spirv/render_atlas_fill.frag.h"
#include "generated/shaders/spirv/render_atlas_stroke.frag.h"
#include "generated/shaders/spirv/draw_msaa_path.webgpu_vert.h"
#include "generated/shaders/spirv/draw_msaa_path.webgpu_noclipdistance_vert.h"
#include "generated/shaders/spirv/draw_msaa_path.webgpu_frag.h"
#include "generated/shaders/spirv/draw_msaa_path.webgpu_fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_msaa_atlas_blit.webgpu_vert.h"
#include "generated/shaders/spirv/draw_msaa_atlas_blit.webgpu_noclipdistance_vert.h"
#include "generated/shaders/spirv/draw_msaa_atlas_blit.webgpu_frag.h"
#include "generated/shaders/spirv/draw_msaa_atlas_blit.webgpu_fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_msaa_image_mesh.webgpu_vert.h"
#include "generated/shaders/spirv/draw_msaa_image_mesh.webgpu_noclipdistance_vert.h"
#include "generated/shaders/spirv/draw_msaa_image_mesh.webgpu_frag.h"
#include "generated/shaders/spirv/draw_msaa_image_mesh.webgpu_fixedcolor_frag.h"
#include "generated/shaders/spirv/draw_msaa_stencil.vert.h"
#include "generated/shaders/spirv/draw_msaa_stencil.frag.h"
} // namespace spirv

#ifdef RIVE_DAWN
#include <dawn/webgpu_cpp.h>
#endif

#ifdef RIVE_WEBGPU
#include <webgpu/webgpu_cpp.h>
#include <emscripten.h>
#include <emscripten/html5.h>
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
using ShaderSourceSPIRV = ShaderModuleSPIRVDescriptor;
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

static wgpu::ShaderModule compile_shader_module_spirv(wgpu::Device device,
                                                      const uint32_t* code,
                                                      uint32_t codeSize)
{
    wgpu::ShaderSourceSPIRV spirvSource;
    spirvSource.code = code;
    spirvSource.codeSize = codeSize;
    wgpu::ShaderModuleDescriptor descriptor = {.nextInChain = &spirvSource};
    return device.CreateShaderModule(&descriptor);
}

#ifdef RIVE_WAGYU
#include <webgpu/webgpu_wagyu.h>

#include "generated/shaders/glsl.glsl.hpp"
#include "generated/shaders/constants.glsl.hpp"
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

EM_JS(int, gl_max_vertex_shader_storage_blocks, (), {
    const version = globalThis.nrdp ?.version || navigator.getNrdpVersion();
    return version.libraries.opengl.options.limits
        .GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS;
});

wgpu::ShaderModule compile_shader_module_wagyu(wgpu::Device device,
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
        if (actions & LoadStoreActionsEXT::clearColor)
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

        wgpu::ShaderModule fragmentShader;
        std::ostringstream glsl;
        glsl << "#version 310 es\n";
        glsl << "#define " GLSL_FRAGMENT " true\n";
        glsl << "#define " GLSL_ENABLE_CLIPPING " true\n";
        BuildLoadStoreEXTGLSL(glsl, actions);
        fragmentShader =
            compile_shader_module_wagyu(context->m_device,
                                        glsl.str().c_str(),
                                        WGPUWagyuShaderLanguage_GLSLRAW);

        wgpu::ColorTargetState colorTargetState = {
            .format = framebufferFormat,
        };

        wgpu::FragmentState fragmentState = {
            .module = fragmentShader,
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

// Renders color ramps to the gradient texture.
class RenderContextWebGPUImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(RenderContextWebGPUImpl* impl)
    {
        const wgpu::Device device = impl->device();

        wgpu::BindGroupLayoutDescriptor colorRampBindingsDesc = {
            .entryCount = COLOR_RAMP_BINDINGS_COUNT,
            .entries = impl->m_perFlushBindingLayouts.data(),
        };

        m_bindGroupLayout =
            device.CreateBindGroupLayout(&colorRampBindingsDesc);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &m_bindGroupLayout,
        };

        wgpu::PipelineLayout pipelineLayout =
            device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShader, fragmentShader;
#ifdef RIVE_WAGYU
        if (impl->m_capabilities.backendType == wgpu::BackendType::OpenGLES)
        {
            // Rive shaders tend to be long and prone to vendor bugs in the
            // compiler. Instead of SPIRV, send down the raw Rive GLSL sources,
            // which have various workarounds for known issues and are tested
            // regularly.
            std::ostringstream glsl;
            glsl << "#define " << GLSL_POST_INVERT_Y << " true\n";
            glsl << glsl::glsl << "\n";
            glsl << glsl::constants << "\n";
            glsl << glsl::common << "\n";
            glsl << glsl::color_ramp << "\n";

            std::ostringstream vertexGLSL;
            vertexGLSL << "#version 310 es\n";
            vertexGLSL << "#define " GLSL_VERTEX " true\n";
            vertexGLSL << glsl.str();
            vertexShader =
                compile_shader_module_wagyu(device,
                                            vertexGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);

            std::ostringstream fragmentGLSL;
            fragmentGLSL << "#version 310 es\n";
            fragmentGLSL << "#define " GLSL_FRAGMENT " true\n";
            fragmentGLSL << glsl.str();
            fragmentShader =
                compile_shader_module_wagyu(device,
                                            fragmentGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);
        }
        else
#endif
        {
            vertexShader =
                compile_shader_module_spirv(device,
                                            spirv::color_ramp_vert,
                                            std::size(spirv::color_ramp_vert));
            fragmentShader =
                compile_shader_module_spirv(device,
                                            spirv::color_ramp_frag,
                                            std::size(spirv::color_ramp_frag));
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
            .module = fragmentShader,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .label = "RIVE_ColorRampPipeline",
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = vertexShader,
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

        wgpu::BindGroupLayoutDescriptor perFlushBindingsDesc = {
            .entryCount = TESS_BINDINGS_COUNT,
            .entries = impl->m_perFlushBindingLayouts.data(),
        };

        m_perFlushBindingsLayout =
            device.CreateBindGroupLayout(&perFlushBindingsDesc);

        wgpu::BindGroupLayout layouts[] = {
            m_perFlushBindingsLayout,
            impl->m_emptyBindingsLayout,
            impl->m_drawBindGroupLayouts[IMMUTABLE_SAMPLER_BINDINGS_SET],
        };
        static_assert(IMMUTABLE_SAMPLER_BINDINGS_SET == 2);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = std::size(layouts),
            .bindGroupLayouts = layouts,
        };

        wgpu::PipelineLayout pipelineLayout =
            device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShader, fragmentShader;
#ifdef RIVE_WAGYU
        if (impl->m_capabilities.backendType == wgpu::BackendType::OpenGLES)
        {
            // Rive shaders tend to be long and prone to vendor bugs in the
            // compiler. Instead of SPIRV, send down the raw Rive GLSL sources,
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
            glsl << glsl::common << "\n";
            glsl << glsl::bezier_utils << "\n";
            glsl << glsl::tessellate << "\n";

            std::ostringstream vertexGLSL;
            vertexGLSL << "#version 310 es\n";
            vertexGLSL << "#define " GLSL_VERTEX " true\n";
            vertexGLSL << glsl.str();
            vertexShader =
                compile_shader_module_wagyu(device,
                                            vertexGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);

            std::ostringstream fragmentGLSL;
            fragmentGLSL << "#version 310 es\n";
            fragmentGLSL << "#define " GLSL_FRAGMENT " true\n";
            fragmentGLSL << glsl.str();
            fragmentShader =
                compile_shader_module_wagyu(device,
                                            fragmentGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);
        }
        else
#endif
        {
            vertexShader =
                compile_shader_module_spirv(device,
                                            spirv::tessellate_vert,
                                            std::size(spirv::tessellate_vert));
            fragmentShader =
                compile_shader_module_spirv(device,
                                            spirv::tessellate_frag,
                                            std::size(spirv::tessellate_frag));
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
            .module = fragmentShader,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .label = "RIVE_TessellatePipeline",
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = vertexShader,
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

        wgpu::BindGroupLayoutDescriptor perFlushBindingsDesc = {
            .entryCount = ATLAS_BINDINGS_COUNT,
            .entries = impl->m_perFlushBindingLayouts.data(),
        };

        m_perFlushBindingsLayout =
            device.CreateBindGroupLayout(&perFlushBindingsDesc);

        wgpu::BindGroupLayout layouts[] = {
            m_perFlushBindingsLayout,
            impl->m_emptyBindingsLayout,
            impl->m_drawBindGroupLayouts[IMMUTABLE_SAMPLER_BINDINGS_SET],
        };
        static_assert(IMMUTABLE_SAMPLER_BINDINGS_SET == 2);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = std::size(layouts),
            .bindGroupLayouts = layouts,
        };

        wgpu::PipelineLayout pipelineLayout =
            device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShader;
        wgpu::ShaderModule fillFragmentShader, strokeFragmentShader;
#ifdef RIVE_WAGYU
        if (impl->m_capabilities.backendType == wgpu::BackendType::OpenGLES)
        {
            // Rive shaders tend to be long and prone to vendor bugs in the
            // compiler. Instead of SPIRV, send down the raw Rive GLSL sources,
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
            glsl << glsl::common << '\n';
            glsl << glsl::draw_path_common << '\n';
            glsl << glsl::render_atlas << '\n';

            std::ostringstream vertexGLSL;
            vertexGLSL << "#version 310 es\n";
            vertexGLSL << "#pragma shader_stage(vertex)\n";
            vertexGLSL << "#define " GLSL_VERTEX " true\n";
            vertexGLSL << glsl.str();
            vertexShader =
                compile_shader_module_wagyu(device,
                                            vertexGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);

            std::ostringstream fillGLSL;
            fillGLSL << "#version 310 es\n";
            fillGLSL << "#pragma shader_stage(fragment)\n";
            fillGLSL << "#define " GLSL_FRAGMENT " true\n";
            fillGLSL << "#define " GLSL_ATLAS_FEATHERED_FILL " true\n";
            fillGLSL << glsl.str();
            fillFragmentShader =
                compile_shader_module_wagyu(device,
                                            fillGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);

            std::ostringstream strokeGLSL;
            strokeGLSL << "#version 310 es\n";
            strokeGLSL << "#pragma shader_stage(fragment)\n";
            strokeGLSL << "#define " GLSL_FRAGMENT " true\n";
            strokeGLSL << "#define " GLSL_ATLAS_FEATHERED_STROKE " true\n";
            strokeGLSL << glsl.str();
            strokeFragmentShader =
                compile_shader_module_wagyu(device,
                                            strokeGLSL.str().c_str(),
                                            WGPUWagyuShaderLanguage_GLSLRAW);
        }
        else
#endif
        {
            vertexShader = compile_shader_module_spirv(
                device,
                spirv::render_atlas_vert,
                std::size(spirv::render_atlas_vert));
            fillFragmentShader = compile_shader_module_spirv(
                device,
                spirv::render_atlas_fill_frag,
                std::size(spirv::render_atlas_fill_frag));
            strokeFragmentShader = compile_shader_module_spirv(
                device,
                spirv::render_atlas_stroke_frag,
                std::size(spirv::render_atlas_stroke_frag));
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
            .module = fillFragmentShader,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .label = "RIVE_AtlasPipeline",
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = vertexShader,
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
        fragmentState.module = strokeFragmentShader;
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
            shaderMiscFlags & gpu::ShaderMiscFlags::fixedFunctionColorOutput;
        wgpu::ShaderModule vertexShader, fragmentShader;
#ifdef RIVE_WAGYU
        PixelLocalStorageType plsType = context->m_capabilities.plsType;
        if (using_pls(interlockMode))
        {
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
                addDefine(GLSL_TARGET_VULKAN);
            }
            if (plsType ==
                PixelLocalStorageType::GL_EXT_shader_pixel_local_storage)
            {
                glsl << "#ifdef GL_EXT_shader_pixel_local_storage\n";
                addDefine(GLSL_PLS_IMPL_EXT_NATIVE);
                glsl << "#else\n";
                // If we are being compiled by SPIRV transpiler for
                // introspection, GL_EXT_shader_pixel_local_storage will not be
                // defined.
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
                    plsType == PixelLocalStorageType::
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
                case DrawType::msaaStencilClipReset:
                case DrawType::renderPassInitialize:
                case DrawType::renderPassResolve:
                    RIVE_UNREACHABLE();
                    break;
            }
            for (size_t i = 0; i < gpu::kShaderFeatureCount; ++i)
            {
                ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
                if (shaderFeatures & feature)
                {
                    addDefine(GetShaderFeatureGLSLName(feature));
                }
            }
            if (fixedFunctionColorOutput)
            {
                addDefine(GLSL_FIXED_FUNCTION_COLOR_OUTPUT);
            }
            if (shaderMiscFlags & gpu::ShaderMiscFlags::clockwiseFill)
            {
                addDefine(GLSL_CLOCKWISE_FILL);
            }
            if (shaderMiscFlags & gpu::ShaderMiscFlags::borrowedCoveragePass)
            {
                addDefine(GLSL_BORROWED_COVERAGE_PASS);
            }
            glsl << gpu::glsl::glsl << '\n';
            glsl << gpu::glsl::constants << '\n';
            glsl << gpu::glsl::common << '\n';
            if (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND)
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
                        glsl << gpu::glsl::draw_raster_order_path_frag << '\n';
                    }
                    else
                    {
                        assert(interlockMode == gpu::InterlockMode::clockwise);
                        glsl << ((shaderMiscFlags &
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
                case DrawType::msaaStencilClipReset:
                case DrawType::renderPassInitialize:
                case DrawType::renderPassResolve:
                    RIVE_UNREACHABLE();
            }

            std::ostringstream vertexGLSL;
            vertexGLSL << versionString << "\n";
            vertexGLSL << "#pragma shader_stage(vertex)\n";
            vertexGLSL << "#define " GLSL_VERTEX " true\n";
            vertexGLSL << glsl.str();
            vertexShader = compile_shader_module_wagyu(context->m_device,
                                                       vertexGLSL.str().c_str(),
                                                       language);

            std::ostringstream fragmentGLSL;
            fragmentGLSL << versionString << "\n";
            fragmentGLSL << "#pragma shader_stage(fragment)\n";
            fragmentGLSL << "#define " GLSL_FRAGMENT " true\n";
            fragmentGLSL << glsl.str();
            fragmentShader =
                compile_shader_module_wagyu(context->m_device,
                                            fragmentGLSL.str().c_str(),
                                            language);
        }
        else
#endif
        {
            assert(interlockMode == gpu::InterlockMode::msaa);
            Span<const uint32_t> vertCode;
            Span<const uint32_t> fragCode;
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
                            ? make_span(spirv::draw_msaa_path_webgpu_vert)
                            : make_span(
                                  spirv::
                                      draw_msaa_path_webgpu_noclipdistance_vert);
                    fragCode =
                        fixedFunctionColorOutput
                            ? make_span(
                                  spirv::draw_msaa_path_webgpu_fixedcolor_frag)
                            : make_span(spirv::draw_msaa_path_webgpu_frag);
                    break;

                case DrawType::msaaStencilClipReset:
                    vertCode = make_span(spirv::draw_msaa_stencil_vert);
                    fragCode = make_span(spirv::draw_msaa_stencil_frag);
                    break;

                case DrawType::interiorTriangulation:
                    // Interior triangulation is not yet implemented for MSAA.
                    RIVE_UNREACHABLE();
                    break;

                case DrawType::atlasBlit:
                    vertCode =
                        (shaderFeatures & ShaderFeatures::ENABLE_CLIP_RECT)
                            ? make_span(spirv::draw_msaa_atlas_blit_webgpu_vert)
                            : make_span(
                                  spirv::
                                      draw_msaa_atlas_blit_webgpu_noclipdistance_vert);
                    fragCode =
                        fixedFunctionColorOutput
                            ? make_span(
                                  spirv::
                                      draw_msaa_atlas_blit_webgpu_fixedcolor_frag)
                            : make_span(
                                  spirv::draw_msaa_atlas_blit_webgpu_frag);
                    break;

                case DrawType::imageMesh:
                    vertCode =
                        (shaderFeatures & ShaderFeatures::ENABLE_CLIP_RECT)
                            ? make_span(spirv::draw_msaa_image_mesh_webgpu_vert)
                            : make_span(
                                  spirv::
                                      draw_msaa_image_mesh_webgpu_noclipdistance_vert);
                    fragCode =
                        fixedFunctionColorOutput
                            ? make_span(
                                  spirv::
                                      draw_msaa_image_mesh_webgpu_fixedcolor_frag)
                            : make_span(
                                  spirv::draw_msaa_image_mesh_webgpu_frag);
                    break;

                case DrawType::renderPassInitialize:
                    // MSAA render passes get initialized by drawing the
                    // previous contents into the framebuffer.
                    // (LoadAction::preserveRenderTarget only.)
                    vertCode =
                        make_span(spirv::blit_texture_as_draw_filtered_vert);
                    fragCode =
                        make_span(spirv::blit_texture_as_draw_filtered_frag);
                    break;

                case DrawType::imageRect:
                case DrawType::renderPassResolve:
                    RIVE_UNREACHABLE();
            }
            vertexShader = compile_shader_module_spirv(
                context->m_device,
                vertCode.data(),
                math::lossless_numeric_cast<uint32_t>(vertCode.count()));
            fragmentShader = compile_shader_module_spirv(
                context->m_device,
                fragCode.data(),
                math::lossless_numeric_cast<uint32_t>(fragCode.count()));
        }

        for (auto framebufferFormat :
             {wgpu::TextureFormat::BGRA8Unorm, wgpu::TextureFormat::RGBA8Unorm})
        {
            int pipelineIdx = RenderPipelineIdx(framebufferFormat);
            m_renderPipelines[pipelineIdx] =
                context->makeDrawPipeline(drawType,
                                          interlockMode,
                                          shaderMiscFlags,
                                          framebufferFormat,
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

    // Compatibility workarounds.
    if (m_capabilities.backendType == wgpu::BackendType::OpenGLES &&
        gl_max_vertex_shader_storage_blocks() < 4)
    {
        // Rive requires 4 storage buffers in the vertex shader. Polyfill them
        // if the hardware doesn't support this.
        m_capabilities.polyfillVertexStorageBuffers = true;
    }
#endif

    m_platformFeatures.clipSpaceBottomUp = true;
    m_platformFeatures.framebufferBottomUp = false;
    m_platformFeatures.msaaColorPreserveNeedsDraw = true;
}

void RenderContextWebGPUImpl::initGPUObjects()
{
    m_perFlushBindingLayouts = {{
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .visibility =
                wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                },
        },
#ifdef RIVE_WAGYU
        m_capabilities.polyfillVertexStorageBuffers ?
            wgpu::BindGroupLayoutEntry{
                .binding = PATH_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Uint,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            } :
#endif
            wgpu::BindGroupLayoutEntry{
                .binding = PATH_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer =
                    {
                        .type = wgpu::BufferBindingType::ReadOnlyStorage,
                    },
            },
#ifdef RIVE_WAGYU
        m_capabilities.polyfillVertexStorageBuffers ?
            wgpu::BindGroupLayoutEntry{
                .binding = PAINT_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Uint,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            } :
#endif
            wgpu::BindGroupLayoutEntry{
                .binding = PAINT_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer =
                    {
                        .type = wgpu::BufferBindingType::ReadOnlyStorage,
                    },
            },
#ifdef RIVE_WAGYU
        m_capabilities.polyfillVertexStorageBuffers ?
            wgpu::BindGroupLayoutEntry{
                .binding = PAINT_AUX_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::UnfilterableFloat,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            } :
#endif
            wgpu::BindGroupLayoutEntry{
                .binding = PAINT_AUX_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer =
                    {
                        .type = wgpu::BufferBindingType::ReadOnlyStorage,
                    },
            },
#ifdef RIVE_WAGYU
        m_capabilities.polyfillVertexStorageBuffers ?
            wgpu::BindGroupLayoutEntry{
                .binding = CONTOUR_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Uint,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            } :
#endif
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
            .binding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .visibility =
                wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = true,
                    .minBindingSize = sizeof(gpu::ImageDrawUniforms),
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
    static_assert(DRAW_BINDINGS_COUNT == 11);
    static_assert(sizeof(m_perFlushBindingLayouts) ==
                  DRAW_BINDINGS_COUNT * sizeof(wgpu::BindGroupLayoutEntry));

    wgpu::BindGroupLayoutDescriptor perFlushBindingsDesc = {
        .entryCount = DRAW_BINDINGS_COUNT,
        .entries = m_perFlushBindingLayouts.data(),
    };

    m_drawBindGroupLayouts[PER_FLUSH_BINDINGS_SET] =
        m_device.CreateBindGroupLayout(&perFlushBindingsDesc);

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
            .binding = IMAGE_SAMPLER_IDX,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler = {.type = wgpu::SamplerBindingType::Filtering},
        },
    };

    wgpu::BindGroupLayoutDescriptor perDrawBindingsDesc = {
        .entryCount = std::size(perDrawBindingLayouts),
        .entries = perDrawBindingLayouts,
    };

    m_drawBindGroupLayouts[PER_DRAW_BINDINGS_SET] =
        m_device.CreateBindGroupLayout(&perDrawBindingsDesc);

    wgpu::BindGroupLayoutEntry drawBindingSamplerLayouts[] = {
        {
            .binding = GRAD_TEXTURE_IDX,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler =
                {
                    .type = wgpu::SamplerBindingType::Filtering,
                },
        },
        {
            .binding = FEATHER_TEXTURE_IDX,
            .visibility =
                wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
            .sampler =
                {
                    .type = wgpu::SamplerBindingType::Filtering,
                },
        },
        {
            .binding = ATLAS_TEXTURE_IDX,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler =
                {
                    .type = wgpu::SamplerBindingType::Filtering,
                },
        },
    };

    wgpu::BindGroupLayoutDescriptor samplerBindingsDesc = {
        .entryCount = std::size(drawBindingSamplerLayouts),
        .entries = drawBindingSamplerLayouts,
    };

    m_drawBindGroupLayouts[IMMUTABLE_SAMPLER_BINDINGS_SET] =
        m_device.CreateBindGroupLayout(&samplerBindingsDesc);

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
        .layout = m_drawBindGroupLayouts[IMMUTABLE_SAMPLER_BINDINGS_SET],
        .entryCount = std::size(samplerBindingEntries),
        .entries = samplerBindingEntries,
    };

    m_samplerBindings = m_device.CreateBindGroup(&samplerBindGroupDesc);

#ifdef RIVE_WAGYU
    const bool supportsPLSInputAttachmentBindings =
        m_capabilities.plsType ==
        PixelLocalStorageType::VK_EXT_rasterization_order_attachment_access;
    if (supportsPLSInputAttachmentBindings)
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
        inputAttachmentsLayoutDesc.entryCount = std::size(inputAttachments),
        inputAttachmentsLayoutDesc.entries = inputAttachments,

        m_drawBindGroupLayouts[PLS_TEXTURE_BINDINGS_SET] =
            wgpu::BindGroupLayout::Acquire(
                wgpuDeviceCreateBindGroupLayout(m_device.Get(),
                                                &inputAttachmentsLayoutDesc));
    }
#endif

    wgpu::PipelineLayoutDescriptor drawPipelineLayoutDesc = {
        .bindGroupLayoutCount = static_cast<size_t>(
#ifdef RIVE_WAGYU
            supportsPLSInputAttachmentBindings ? BINDINGS_SET_COUNT :
#endif
                                               BINDINGS_SET_COUNT - 1),
        .bindGroupLayouts = m_drawBindGroupLayouts,
    };
    static_assert(PLS_TEXTURE_BINDINGS_SET == BINDINGS_SET_COUNT - 1);

    m_drawPipelineLayout =
        m_device.CreatePipelineLayout(&drawPipelineLayoutDesc);

    wgpu::BindGroupLayoutDescriptor emptyBindingsDesc = {};
    m_emptyBindingsLayout = m_device.CreateBindGroupLayout(&emptyBindingsDesc);

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
                     wgpu::TextureUsage::TextureBinding,
            .size = {static_cast<uint32_t>(width()),
                     static_cast<uint32_t>(height())},
            .format = wgpu::TextureFormat::RGBA8Unorm,
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
            flags() & RenderBufferFlags::mappedOnceAtInitialization;
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
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
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
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
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
                .binding = IMAGE_SAMPLER_IDX,
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

        wgpu::ShaderModule vertexShader = compile_shader_module_spirv(
            device,
            spirv::blit_texture_as_draw_filtered_vert,
            std::size(spirv::blit_texture_as_draw_filtered_vert));

        wgpu::ShaderModule fragmentShader = compile_shader_module_spirv(
            device,
            spirv::blit_texture_as_draw_filtered_frag,
            std::size(spirv::blit_texture_as_draw_filtered_frag));

        wgpu::ColorTargetState colorTargetState = {
            .format = wgpu::TextureFormat::RGBA8Unorm,
        };

        wgpu::FragmentState fragmentState = {
            .module = fragmentShader,
            .entryPoint = "main",
            .targetCount = 1,
            .targets = &colorTargetState,
        };

        wgpu::RenderPipelineDescriptor desc = {
            .label = "RIVE_BlitTextureAsDrawPipeline",
            .layout = pipelineLayout,
            .vertex =
                {
                    .module = vertexShader,
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
                .binding = IMAGE_SAMPLER_IDX,
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
    const uint8_t imageDataRGBAPremul[])
{
    wgpu::TextureDescriptor textureDesc = {
        .usage =
            wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
        .dimension = wgpu::TextureDimension::e2D,
        .size = {width, height},
        .format = wgpu::TextureFormat::RGBA8Unorm,
        .mipLevelCount = mipLevelCount,
    };
    if (mipLevelCount > 1)
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

    wgpu::TexelCopyTextureInfo dest = {.texture = texture};
    wgpu::TexelCopyBufferLayout layout = {.bytesPerRow = width * 4};
    wgpu::Extent3D extent = {width, height};
    m_queue.WriteTexture(&dest,
                         imageDataRGBAPremul,
                         height * width * 4,
                         &layout,
                         &extent);

    if (mipLevelCount > 1)
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
#ifdef RIVE_WAGYU
    if (m_capabilities.polyfillVertexStorageBuffers)
    {
        return std::make_unique<StorageTextureBufferWebGPU>(m_device,
                                                            m_queue,
                                                            capacityInBytes,
                                                            bufferStructure);
    }
    else
#endif
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

wgpu::RenderPipeline RenderContextWebGPUImpl::makeDrawPipeline(
    gpu::DrawType drawType,
    gpu::InterlockMode interlockMode,
    gpu::ShaderMiscFlags shaderMiscFlags,
    wgpu::TextureFormat framebufferFormat,
    wgpu::ShaderModule vertexShader,
    wgpu::ShaderModule fragmentShader,
    const gpu::PipelineState& pipelineState)
{
    std::vector<WGPUVertexAttribute> attrs;
    std::vector<WGPUVertexBufferLayout> vertexBufferLayouts;
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
            attrs = {
                WGPUVertexAttribute{
                    .format = WGPUVertexFormat_Float32x4,
                    .offset = 0,
                    .shaderLocation = 0,
                },
                WGPUVertexAttribute{
                    .format = WGPUVertexFormat_Float32x4,
                    .offset = 4 * sizeof(float),
                    .shaderLocation = 1,
                },
            };

            vertexBufferLayouts = {WGPU_VERTEX_BUFFER_LAYOUT_INIT};
            vertexBufferLayouts[0].attributeCount = std::size(attrs);
            vertexBufferLayouts[0].attributes = attrs.data();
            vertexBufferLayouts[0].arrayStride = sizeof(gpu::PatchVertex);
            vertexBufferLayouts[0].stepMode = WGPUVertexStepMode_Vertex;

            topology = WGPUPrimitiveTopology_TriangleList;
            break;
        }
        case DrawType::msaaStencilClipReset:
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
        {
            attrs = {
                WGPUVertexAttribute{
                    .format = WGPUVertexFormat_Float32x3,
                    .offset = 0,
                    .shaderLocation = 0,
                },
            };

            vertexBufferLayouts = {WGPU_VERTEX_BUFFER_LAYOUT_INIT};
            vertexBufferLayouts[0].attributeCount = std::size(attrs);
            vertexBufferLayouts[0].attributes = attrs.data();
            vertexBufferLayouts[0].arrayStride = sizeof(gpu::TriangleVertex);
            vertexBufferLayouts[0].stepMode = WGPUVertexStepMode_Vertex;

            topology = WGPUPrimitiveTopology_TriangleList;
            break;
        }
        case DrawType::imageRect:
            RIVE_UNREACHABLE();
        case DrawType::imageMesh:
            attrs = {
                WGPUVertexAttribute{
                    .format = WGPUVertexFormat_Float32x2,
                    .offset = 0,
                    .shaderLocation = 0,
                },
                WGPUVertexAttribute{
                    .format = WGPUVertexFormat_Float32x2,
                    .offset = 0,
                    .shaderLocation = 1,
                },
            };

            vertexBufferLayouts = {WGPU_VERTEX_BUFFER_LAYOUT_INIT,
                                   WGPU_VERTEX_BUFFER_LAYOUT_INIT};

            vertexBufferLayouts[0].attributeCount = 1;
            vertexBufferLayouts[0].attributes = &attrs[0];
            vertexBufferLayouts[0].arrayStride = sizeof(float) * 2;
            vertexBufferLayouts[0].stepMode = WGPUVertexStepMode_Vertex;

            vertexBufferLayouts[1].attributeCount = 1;
            vertexBufferLayouts[1].attributes = &attrs[1];
            vertexBufferLayouts[1].arrayStride = sizeof(float) * 2;
            vertexBufferLayouts[1].stepMode = WGPUVertexStepMode_Vertex;

            topology = WGPUPrimitiveTopology_TriangleList;
            break;
        case DrawType::renderPassInitialize:
            // No attrs.
            topology = WGPUPrimitiveTopology_TriangleStrip;
            break;
        case DrawType::renderPassResolve:
            RIVE_UNREACHABLE();
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

    WGPUFragmentState fragmentState = {
        .module = fragmentShader.Get(),
        .entryPoint = WGPU_STRING_VIEW("main"),
        .targetCount = colorAttachments.size(),
        .targets = colorAttachments.data(),
    };

#ifdef RIVE_WAGYU
    WGPUWagyuInputAttachmentState inputAttachments[PLS_PLANE_COUNT];
    WGPUWagyuFragmentState wagyuFragmentState = WGPU_WAGYU_FRAGMENT_STATE_INIT;
    if (usingPLSInputAttachments)
    {
        for (size_t i = 0; i < PLS_PLANE_COUNT; ++i)
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
        .layout = m_drawPipelineLayout.Get(),
        .vertex =
            {
                .module = vertexShader.Get(),
                .entryPoint = WGPU_STRING_VIEW("main"),
                .bufferCount = std::size(vertexBufferLayouts),
                .buffers = vertexBufferLayouts.data(),
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

wgpu::RenderPassEncoder RenderContextWebGPUImpl::beginPLSRenderPass(
    wgpu::CommandEncoder commandEncoder,
    const FlushDescriptor& desc)
{
    auto* const renderTarget =
        static_cast<RenderTargetWebGPU*>(desc.renderTarget);

    const auto colorLoadOp =
        (desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget)
            ? WGPULoadOp_Load
            : WGPULoadOp_Clear;

    StackVector<WGPURenderPassColorAttachment, PLS_PLANE_COUNT> plsAttachments;

    WGPUColor targetClearValue =
        math::bit_cast<WGPUColor>(wgpu_color_premul(desc.colorClearValue));

    WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    passDesc.label = WGPU_STRING_VIEW("RIVE_PLS_RenderPass");

    // framebuffer
    assert(plsAttachments.size() == COLOR_PLANE_IDX);
    plsAttachments.push_back({
        .view = renderTarget->targetTextureView().Get(),
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

    if (using_pls(desc.interlockMode))
    {
        if (m_capabilities.plsType ==
            PixelLocalStorageType::VK_EXT_rasterization_order_attachment_access)
        {
            assert(inputAttachments.size() == COLOR_PLANE_IDX);
            inputAttachments.push_back({
                .view = renderTarget->targetTextureView().Get(),
                .clearValue = &targetClearValue,
                .loadOp = colorLoadOp,
                .storeOp = WGPUStoreOp_Store,
            });

            WGPUColor zeroClearValue = {};

            // clip
            assert(plsAttachments.size() == CLIP_PLANE_IDX);
            plsAttachments.push_back({
                .view = renderTarget->clipTextureView().Get(),
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Discard,
                .clearValue = {},
            });
            assert(inputAttachments.size() == CLIP_PLANE_IDX);
            inputAttachments.push_back({
                .view = renderTarget->clipTextureView().Get(),
                .clearValue = &zeroClearValue,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Discard,
            });

            // scratchColor
            assert(plsAttachments.size() == SCRATCH_COLOR_PLANE_IDX);
            plsAttachments.push_back({
                .view = renderTarget->scratchColorTextureView().Get(),
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Discard,
                .clearValue = {},
            });
            assert(inputAttachments.size() == SCRATCH_COLOR_PLANE_IDX);
            inputAttachments.push_back({
                .view = renderTarget->scratchColorTextureView().Get(),
                .clearValue = &zeroClearValue,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Discard,
            });

            // coverage
            assert(plsAttachments.size() == COVERAGE_PLANE_IDX);
            plsAttachments.push_back({
                .view = renderTarget->coverageTextureView().Get(),
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Discard,
                .clearValue = {},
            });
            assert(inputAttachments.size() == COVERAGE_PLANE_IDX);
            inputAttachments.push_back({
                .view = renderTarget->coverageTextureView().Get(),
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
        else if (m_capabilities.plsType ==
                 PixelLocalStorageType::GL_EXT_shader_pixel_local_storage)
        {
            wagyuRenderPassDescriptor.pixelLocalStorageEnabled =
                WGPUOptionalBool_True;
            if (desc.fixedFunctionColorOutput)
            {
                assert(m_capabilities.GL_EXT_shader_pixel_local_storage2);
                wagyuRenderPassDescriptor.pixelLocalStorageSize =
                    2 * sizeof(uint32_t);
            }
        }
        passDesc.nextInChain = &wagyuRenderPassDescriptor.chain;
    }
#endif

    passDesc.colorAttachmentCount = plsAttachments.size();
    passDesc.colorAttachments = plsAttachments.data();

    wgpu::RenderPassEncoder drawPass = wgpu::RenderPassEncoder::Acquire(
        wgpuCommandEncoderBeginRenderPass(commandEncoder.Get(), &passDesc));
    initDrawRenderPass(drawPass, desc);
    return drawPass;
}

wgpu::RenderPassEncoder RenderContextWebGPUImpl::beginMSAARenderPass(
    wgpu::CommandEncoder commandEncoder,
    MSAABeginType msaaBeginType,
    MSAAEndType msaaEndType,
    const FlushDescriptor& desc)
{
    auto* const renderTarget =
        static_cast<RenderTargetWebGPU*>(desc.renderTarget);

    // Our MSAA buffers are treated as completely transient (i.e.,
    // Clear/Discard) unless we have to do render pass breaks for dst copies.
    // For LoadAction::preserveRenderTarget, we manually draw the old content
    // into the transient MSAA buffer, so we still load with Clear.
    // TODO: wgpu::LoadOp::ExpandResolveTexture for the color buffer when
    // supported.
    const auto msaaLoadOp = msaaBeginType == MSAABeginType::restartAfterDstCopy
                                ? wgpu::LoadOp::Load
                                : wgpu::LoadOp::Clear;
    const auto msaaStoreOp = (msaaEndType == MSAAEndType::breakForDstCopy)
                                 ? wgpu::StoreOp::Store
                                 : wgpu::StoreOp::Discard;

    wgpu::RenderPassColorAttachment msaaColorAttachment = {
        .view = renderTarget->msaaColorTextureView(),
        .resolveTarget = renderTarget->targetTextureView().Get(),
        .loadOp = msaaLoadOp,
        .storeOp = msaaStoreOp,
        .clearValue = wgpu_color_premul(desc.colorClearValue),
    };

    wgpu::RenderPassDepthStencilAttachment msaaDepthStencilAttachment = {
        .view = renderTarget->msaaDepthStencilTextureView(),
        .depthLoadOp = msaaLoadOp,
        .depthStoreOp = msaaStoreOp,
        .depthClearValue = desc.depthClearValue,
        .depthReadOnly = false,
        .stencilLoadOp = msaaLoadOp,
        .stencilStoreOp = msaaStoreOp,
        .stencilClearValue = desc.stencilClearValue,
        .stencilReadOnly = false,
    };

    wgpu::RenderPassDescriptor renderPassDescriptor = {
        .label = "RIVE_MSAA_RenderPass",
        .colorAttachmentCount = 1,
        .colorAttachments = &msaaColorAttachment,
        .depthStencilAttachment = &msaaDepthStencilAttachment,
    };

    wgpu::RenderPassEncoder drawPass =
        commandEncoder.BeginRenderPass(&renderPassDescriptor);
    initDrawRenderPass(drawPass, desc);
    return drawPass;
}

void RenderContextWebGPUImpl::initDrawRenderPass(
    wgpu::RenderPassEncoder drawPass,
    const FlushDescriptor& desc)
{
    drawPass.SetViewport(0.f,
                         0.f,
                         desc.renderTarget->width(),
                         desc.renderTarget->height(),
                         0.0,
                         1.0);
    drawPass.SetBindGroup(IMMUTABLE_SAMPLER_BINDINGS_SET, m_samplerBindings);
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

#ifdef RIVE_WAGYU
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
#endif

    wgpu::BindGroupEntry perFlushBindingEntries[DRAW_BINDINGS_COUNT] = {
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .buffer = webgpu_buffer(flushUniformBufferRing()),
            .offset = desc.flushUniformDataOffsetInBytes,
        },
#ifdef RIVE_WAGYU
        m_capabilities.polyfillVertexStorageBuffers
            ? wgpu::BindGroupEntry{.binding = PATH_BUFFER_IDX,
                                   .textureView = webgpu_storage_texture_view(
                                       pathBufferRing())}
            :
#endif
            wgpu::BindGroupEntry{
                .binding = PATH_BUFFER_IDX,
                .buffer = webgpu_buffer(pathBufferRing()),
                .offset = desc.firstPath * sizeof(gpu::PathData),
            },
#ifdef RIVE_WAGYU
        m_capabilities.polyfillVertexStorageBuffers ?
            wgpu::BindGroupEntry{
                .binding = PAINT_BUFFER_IDX,
                .textureView = webgpu_storage_texture_view(paintBufferRing()),
            } :
#endif
            wgpu::BindGroupEntry{
                .binding = PAINT_BUFFER_IDX,
                .buffer = webgpu_buffer(paintBufferRing()),
                .offset = desc.firstPaint * sizeof(gpu::PaintData),
            },
#ifdef RIVE_WAGYU
        m_capabilities.polyfillVertexStorageBuffers ?
            wgpu::BindGroupEntry{
                .binding = PAINT_AUX_BUFFER_IDX,
                .textureView = webgpu_storage_texture_view(paintAuxBufferRing()),
            } :
#endif
            wgpu::BindGroupEntry{
                .binding = PAINT_AUX_BUFFER_IDX,
                .buffer = webgpu_buffer(paintAuxBufferRing()),
                .offset = desc.firstPaintAux * sizeof(gpu::PaintAuxData),
            },
#ifdef RIVE_WAGYU
        m_capabilities.polyfillVertexStorageBuffers ?
            wgpu::BindGroupEntry{
                .binding = CONTOUR_BUFFER_IDX,
                .textureView = webgpu_storage_texture_view(contourBufferRing()),
            } :
#endif
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
            .binding = IMAGE_DRAW_UNIFORM_BUFFER_IDX,
            .buffer = webgpu_buffer(imageDrawUniformBufferRing()),
            .size = sizeof(gpu::ImageDrawUniforms),
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
        tessPass.SetBindGroup(IMMUTABLE_SAMPLER_BINDINGS_SET,
                              m_samplerBindings);
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
        atlasPass.SetBindGroup(IMMUTABLE_SAMPLER_BINDINGS_SET,
                               m_samplerBindings);

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

    wgpu::RenderPassEncoder drawPass;
    if (desc.interlockMode == gpu::InterlockMode::msaa)
    {
        // If we're preserving the render target with a draw, don't start the
        // render pass yet. We will get a dstBlend barrier on the first draw
        // that does a copy and starts a new render pass.
        if (desc.drawList->empty() || desc.drawList->head()->drawType !=
                                          gpu::DrawType::renderPassInitialize)
        {
            drawPass =
                beginMSAARenderPass(commandEncoder,
                                    MSAABeginType::primary,
                                    (desc.firstDstBlendBarrier != nullptr)
                                        ? MSAAEndType::breakForDstCopy
                                        : MSAAEndType::finish,
                                    desc);
        }
    }
    else
    {
        drawPass = beginPLSRenderPass(commandEncoder, desc);
    }

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
            .layout = m_drawBindGroupLayouts[PLS_TEXTURE_BINDINGS_SET],
            .entryCount = std::size(plsTextureBindingEntries),
            .entries = plsTextureBindingEntries,
        };

        wgpu::BindGroup plsTextureBindings =
            m_device.CreateBindGroup(&plsTextureBindingsGroupDesc);
        drawPass.SetBindGroup(PLS_TEXTURE_BINDINGS_SET, plsTextureBindings);
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
                drawPass.Get(),
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

            if (loadActions & LoadStoreActionsEXT::clearColor)
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
                drawPass.SetBindGroup(0, uniformBindings);
            }

            drawPass.SetPipeline(
                loadPipeline.renderPipeline(renderTarget->framebufferFormat()));
            drawPass.Draw(4);
        }
    }
#endif

    wgpu::BindGroupDescriptor perFlushBindGroupDesc = {
        .layout = m_drawBindGroupLayouts[PER_FLUSH_BINDINGS_SET],
        .entryCount = DRAW_BINDINGS_COUNT,
        .entries = perFlushBindingEntries,
    };

    wgpu::BindGroup perFlushBindings =
        m_device.CreateBindGroup(&perFlushBindGroupDesc);

    // Execute the DrawList.
    bool needsNewBindings = true;
    for (const DrawBatch& batch : *desc.drawList)
    {
        DrawType drawType = batch.drawType;

        if (batch.barriers & gpu::BarrierFlags::dstBlend)
        {
            // For a dstBlend barrier, our only option in unextended WebGPU is
            // to copy out the dst pixels we want to read into a separate
            // texture.
            assert(desc.interlockMode == gpu::InterlockMode::msaa);
            assert(!desc.fixedFunctionColorOutput ||
                   drawType == gpu::DrawType::renderPassInitialize);
#ifdef RIVE_WAGYU
            assert(!usingInputAttachmentBindings);
#endif

            MSAABeginType msaaBeginType;
            if (drawType == gpu::DrawType::renderPassInitialize)
            {
                assert(drawPass == nullptr);
                assert(desc.colorLoadAction ==
                       gpu::LoadAction::preserveRenderTarget);

                // Copy out the entire target texture in order to seed the MSAA
                // color buffer. We can't seed from the target texture itself
                // because it's also the resolveTarget.
                renderTarget->copyTargetToDstColorTexture(
                    commandEncoder,
                    renderTarget->bounds());

                msaaBeginType = MSAABeginType::primary;
            }
            else
            {
                // Break the render pass to copy out a dst texture.
                drawPass.End();

                // Copy the resolve texture into yet another "dstRead" texture
                // for shaders to read.
                for (const Draw* draw = batch.dstReadList; draw != nullptr;
                     draw = draw->nextDstRead())
                {
                    assert(draw->blendMode() != BlendMode::srcOver);
                    renderTarget->copyTargetToDstColorTexture(
                        commandEncoder,
                        desc.renderTargetUpdateBounds.intersect(
                            draw->pixelBounds()));
                }

                msaaBeginType = MSAABeginType::restartAfterDstCopy;
            }

            // Restart the render pass after the copies are finished.
            drawPass =
                beginMSAARenderPass(commandEncoder,
                                    msaaBeginType,
                                    (batch.nextDstBlendBarrier != nullptr)
                                        ? MSAAEndType::breakForDstCopy
                                        : MSAAEndType::finish,
                                    desc);
            needsNewBindings = true;
        }

        // Bind the appropriate image texture, if any.
        wgpu::TextureView imageTextureView = m_nullTextureView;
        if (auto imageTexture =
                static_cast<const TextureWebGPUImpl*>(batch.imageTexture))
        {
            imageTextureView = imageTexture->textureView();
            needsNewBindings = true;
        }
        else if (drawType == gpu::DrawType::renderPassInitialize)
        {
            assert(desc.interlockMode == gpu::InterlockMode::msaa);
            assert(needsNewBindings);
            imageTextureView = renderTarget->dstColorTextureView();
        }

        if (needsNewBindings ||
            // Image draws always re-bind because they update the dynamic offset
            // to their uniforms.
            drawType == DrawType::imageRect || drawType == DrawType::imageMesh)
        {
            drawPass.SetBindGroup(PER_FLUSH_BINDINGS_SET,
                                  perFlushBindings,
                                  1,
                                  &batch.imageDrawDataOffset);

            wgpu::BindGroupEntry perDrawBindingEntries[] = {
                {
                    .binding = IMAGE_TEXTURE_IDX,
                    .textureView = imageTextureView,
                },
                {
                    .binding = IMAGE_SAMPLER_IDX,
                    .sampler = m_imageSamplers[batch.imageSampler.asKey()],
                },
            };

            wgpu::BindGroupDescriptor perDrawBindGroupDesc = {
                .layout = m_drawBindGroupLayouts[PER_DRAW_BINDINGS_SET],
                .entryCount = std::size(perDrawBindingEntries),
                .entries = perDrawBindingEntries,
            };

            wgpu::BindGroup perDrawBindings =
                m_device.CreateBindGroup(&perDrawBindGroupDesc);
            drawPass.SetBindGroup(PER_DRAW_BINDINGS_SET,
                                  perDrawBindings,
                                  0,
                                  nullptr);
        }

        gpu::PipelineState pipelineState;
        gpu::get_pipeline_state(batch,
                                desc,
                                m_platformFeatures,
                                &pipelineState);
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering ||
            desc.interlockMode == gpu::InterlockMode::clockwise)
        {
            // All forms of PLS used by our webgpu backend require color writes
            // to be enabled, so override the Rive state, which expects PLS to
            // work when color writes are disabled.
            // TODO: We shouldn't technically update the pipelineState without
            // also updating the uniqueKey, but this works for now because
            // interlockMode is also part of the pipeline key.
            pipelineState.colorWriteEnabled = true;
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
        uint64_t pipelineKey = gpu::ShaderUniqueKey(drawType,
                                                    batch.shaderFeatures,
                                                    desc.interlockMode,
                                                    batch.shaderMiscFlags);

        assert(pipelineKey << PipelineState::UNIQUE_KEY_BIT_COUNT >>
                   PipelineState::UNIQUE_KEY_BIT_COUNT ==
               pipelineKey);
        assert(pipelineState.uniqueKey <
               1 << PipelineState::UNIQUE_KEY_BIT_COUNT);
        pipelineKey = (pipelineKey << PipelineState::UNIQUE_KEY_BIT_COUNT) |
                      pipelineState.uniqueKey;

        assert(pipelineKey << 1 >> 1 == pipelineKey);
        pipelineKey =
            (pipelineKey << 1) | static_cast<uint32_t>(targetIsGLFBO0);

        const DrawPipeline& drawPipeline =
            m_drawPipelines
                .try_emplace(pipelineKey,
                             this,
                             drawType,
                             batch.shaderFeatures,
                             desc.interlockMode,
                             batch.shaderMiscFlags,
                             pipelineState,
                             targetIsGLFBO0)
                .first->second;
        drawPass.SetPipeline(
            drawPipeline.renderPipeline(renderTarget->framebufferFormat()));
        if (pipelineState.stencilTestEnabled)
        {
            drawPass.SetStencilReference(pipelineState.stencilReference);
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
                drawPass.SetVertexBuffer(0, m_pathPatchVertexBuffer);
                drawPass.SetIndexBuffer(m_pathPatchIndexBuffer,
                                        wgpu::IndexFormat::Uint16);
                drawPass.DrawIndexed(gpu::PatchIndexCount(drawType),
                                     batch.elementCount,
                                     gpu::PatchBaseIndex(drawType),
                                     0,
                                     batch.baseElement);
                break;
            }

            case DrawType::msaaStencilClipReset:
            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
            {
                drawPass.SetVertexBuffer(0,
                                         webgpu_buffer(triangleBufferRing()));
                drawPass.Draw(batch.elementCount, 1, batch.baseElement);
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
                drawPass.SetVertexBuffer(0, vertexBuffer->submittedBuffer());
                drawPass.SetVertexBuffer(1, uvBuffer->submittedBuffer());
                drawPass.SetIndexBuffer(indexBuffer->submittedBuffer(),
                                        wgpu::IndexFormat::Uint16);
                drawPass.DrawIndexed(batch.elementCount, 1, batch.baseElement);
                break;
            }

            case DrawType::renderPassInitialize:
                drawPass.Draw(4);
                break;

            case DrawType::imageRect:
            case DrawType::renderPassResolve:
                RIVE_UNREACHABLE();
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
        drawPass.SetPipeline(
            storePipeline->renderPipeline(renderTarget->framebufferFormat()));
        drawPass.Draw(4);
    }
#endif

    drawPass.End();
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
} // namespace rive::gpu

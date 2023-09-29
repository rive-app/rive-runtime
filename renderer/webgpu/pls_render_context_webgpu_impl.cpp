/*
 * Copyright 2023 Rive
 */

#include <sstream>
#include <string>

#include "rive/pls/webgpu/pls_render_context_webgpu_impl.hpp"
#include "shaders/constants.glsl"

#include "../out/obj/generated/spirv/color_ramp.vert.h"
#include "../out/obj/generated/spirv/color_ramp.frag.h"
#include "../out/obj/generated/spirv/tessellate.vert.h"
#include "../out/obj/generated/spirv/tessellate.frag.h"
#include "../out/obj/generated/spirv/draw_path.vert.h"
#include "../out/obj/generated/spirv/draw_path.frag.h"
#include "../out/obj/generated/spirv/draw_interior_triangles.vert.h"
#include "../out/obj/generated/spirv/draw_interior_triangles.frag.h"

#include "../out/obj/generated/pls_load_store_ext.glsl.hpp"
#include "../out/obj/generated/glsl.glsl.hpp"
#include "../out/obj/generated/constants.glsl.hpp"
#include "../out/obj/generated/common.glsl.hpp"
#include "../out/obj/generated/advanced_blend.glsl.hpp"
#include "../out/obj/generated/draw_path.glsl.hpp"
#include "../out/obj/generated/draw_image_mesh.glsl.hpp"

#ifdef RIVE_DAWN
#include <dawn/webgpu_cpp.h>
#include "../out/obj/generated/glsl.exports.h"

static void enable_shader_pixel_local_storage_ext(wgpu::RenderPassEncoder, bool enabled)
{
    RIVE_UNREACHABLE();
}
#endif

#ifdef RIVE_WEBGPU
#include <webgpu/webgpu_cpp.h>
#include <emscripten.h>
#include <emscripten/html5_webgpu.h>

EM_JS(void, enable_shader_pixel_local_storage_ext_js, (int renderPass, bool enabled), {
    renderPass = JsValStore.get(renderPass);
    renderPass.setShaderPixelLocalStorageEnabled(Boolean(enabled));
});

static void enable_shader_pixel_local_storage_ext(wgpu::RenderPassEncoder renderPass, bool enabled)
{
    enable_shader_pixel_local_storage_ext_js(
        emscripten_webgpu_export_render_pass_encoder(renderPass.Get()),
        enabled);
}
#endif

namespace rive::pls
{
// Draws emulated render-pass load/store actions for EXT_shader_pixel_local_storage.
class PLSRenderContextWebGPUImpl::LoadStoreEXTPipeline
{
public:
    LoadStoreEXTPipeline(PLSRenderContextWebGPUImpl* context,
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

            m_bindGroupLayout = context->m_device.CreateBindGroupLayout(&bindingsDesc);

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
        glsl << "#pragma shader_stage(fragment)\n";
        glsl << "#define " GLSL_FRAGMENT "\n";
        glsl << "#extension GL_EXT_shader_pixel_local_storage : enable\n";
        glsl << "#extension GL_ARM_shader_framebuffer_fetch : enable\n";
        glsl << "#extension GL_EXT_shader_framebuffer_fetch : enable\n";
        glsl << BuildLoadStoreEXTGLSL(actions);
        fragmentShader = m_fragmentShaderHandle.compileShaderModule(context->m_device,
                                                                    glsl.str().c_str(),
                                                                    "glsl-raw");

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
                    .frontFace = context->m_frontFaceForOnScreenDraws,
                    .cullMode = wgpu::CullMode::Back,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = context->m_device.CreateRenderPipeline(&desc);
    }

    const wgpu::BindGroupLayout& bindGroupLayout() const
    {
        assert(m_bindGroupLayout); // We only have a bind group if there is a clear color.
        return m_bindGroupLayout;
    }

    wgpu::RenderPipeline renderPipeline(wgpu::TextureFormat framebufferFormat) const
    {
        assert(framebufferFormat == m_framebufferFormat);
        return m_renderPipeline;
    }

private:
    const wgpu::TextureFormat m_framebufferFormat;
    wgpu::BindGroupLayout m_bindGroupLayout;
    EmJsHandle m_fragmentShaderHandle;
    wgpu::RenderPipeline m_renderPipeline;
};

// Renders color ramps to the gradient texture.
class PLSRenderContextWebGPUImpl::ColorRampPipeline
{
public:
    ColorRampPipeline(wgpu::Device device)
    {
        wgpu::BindGroupLayoutEntry bindingLayouts[] = {
            {
                .binding = FLUSH_UNIFORM_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
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

        m_bindGroupLayout = device.CreateBindGroupLayout(&bindingsDesc);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &m_bindGroupLayout,
        };

        wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShader =
            m_vertexShaderHandle.compileSPIRVShaderModule(device,
                                                          color_ramp_vert,
                                                          std::size(color_ramp_vert));

        wgpu::VertexAttribute attrs[] = {
            {
                .format = wgpu::VertexFormat::Uint32x4,
                .offset = 0,
                .shaderLocation = 0,
            },
        };

        wgpu::VertexBufferLayout vertexBufferLayout = {
            .arrayStride = sizeof(pls::GradientSpan),
            .stepMode = wgpu::VertexStepMode::Instance,
            .attributeCount = std::size(attrs),
            .attributes = attrs,
        };

        wgpu::ShaderModule fragmentShader =
            m_fragmentShaderHandle.compileSPIRVShaderModule(device,
                                                            color_ramp_frag,
                                                            std::size(color_ramp_frag));

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
                    .frontFace = kFrontFaceForOffscreenDraws,
                    .cullMode = wgpu::CullMode::Back,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = device.CreateRenderPipeline(&desc);
    }

    const wgpu::BindGroupLayout& bindGroupLayout() const { return m_bindGroupLayout; }
    const wgpu::RenderPipeline& renderPipeline() const { return m_renderPipeline; }

private:
    wgpu::BindGroupLayout m_bindGroupLayout;
    EmJsHandle m_vertexShaderHandle;
    EmJsHandle m_fragmentShaderHandle;
    wgpu::RenderPipeline m_renderPipeline;
};

// Renders tessellated vertices to the tessellation texture.
class PLSRenderContextWebGPUImpl::TessellatePipeline
{
public:
    TessellatePipeline(wgpu::Device device)
    {
        wgpu::BindGroupLayoutEntry bindingLayouts[] = {
            {
                .binding = PATH_TEXTURE_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Uint,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            },
            {
                .binding = CONTOUR_TEXTURE_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
                .texture =
                    {
                        .sampleType = wgpu::TextureSampleType::Uint,
                        .viewDimension = wgpu::TextureViewDimension::e2D,
                    },
            },
            {
                .binding = FLUSH_UNIFORM_BUFFER_IDX,
                .visibility = wgpu::ShaderStage::Vertex,
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

        m_bindGroupLayout = device.CreateBindGroupLayout(&bindingsDesc);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &m_bindGroupLayout,
        };

        wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

        wgpu::ShaderModule vertexShader =
            m_vertexShaderHandle.compileSPIRVShaderModule(device,
                                                          tessellate_vert,
                                                          std::size(tessellate_vert));

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
            .arrayStride = sizeof(pls::TessVertexSpan),
            .stepMode = wgpu::VertexStepMode::Instance,
            .attributeCount = std::size(attrs),
            .attributes = attrs,
        };

        wgpu::ShaderModule fragmentShader =
            m_fragmentShaderHandle.compileSPIRVShaderModule(device,
                                                            tessellate_frag,
                                                            std::size(tessellate_frag));

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
                    .frontFace = kFrontFaceForOffscreenDraws,
                    .cullMode = wgpu::CullMode::Back,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = device.CreateRenderPipeline(&desc);
    }

    const wgpu::BindGroupLayout& bindGroupLayout() const { return m_bindGroupLayout; }
    const wgpu::RenderPipeline renderPipeline() const { return m_renderPipeline; }

private:
    wgpu::BindGroupLayout m_bindGroupLayout;
    EmJsHandle m_vertexShaderHandle;
    EmJsHandle m_fragmentShaderHandle;
    wgpu::RenderPipeline m_renderPipeline;
};

// Draw paths and image meshes using the gradient and tessellation textures.
class PLSRenderContextWebGPUImpl::DrawPipeline
{
public:
    DrawPipeline(PLSRenderContextWebGPUImpl* context,
                 DrawType drawType,
                 ShaderFeatures shaderFeatures)
    {
        wgpu::ShaderModule vertexShader, fragmentShader;
        if (context->m_pixelLocalStorageType ==
            PixelLocalStorageType::EXT_shader_pixel_local_storage)
        {
            std::ostringstream glsl;
            auto addDefine = [&glsl](const char* name) { glsl << "#define " << name << "\n"; };
            // If we are being compiled by SPIRV transpiler for introspection,
            // GL_EXT_shader_pixel_local_storage will not be defined.
            glsl << "#ifdef GL_EXT_shader_pixel_local_storage\n";
            addDefine(GLSL_PLS_IMPL_EXT_NATIVE);
            glsl << "#else\n";
            glsl << "#extension GL_EXT_samplerless_texture_functions : enable\n";
            addDefine(GLSL_TARGET_VULKAN);
            glsl << "#endif\n";
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::outerCurvePatches:
                    addDefine(GLSL_ENABLE_INSTANCE_INDEX);
                    // The WebGPU layer automatically searches for a uniform named
                    // "SPIRV_Cross_BaseInstance" and manages it for us.
                    addDefine(GLSL_ENABLE_SPIRV_CROSS_BASE_INSTANCE);
                    break;
                case DrawType::interiorTriangulation:
                    addDefine(GLSL_DRAW_INTERIOR_TRIANGLES);
                    break;
                case DrawType::imageMesh:
                    break;
            }
            for (size_t i = 0; i < pls::kShaderFeatureCount; ++i)
            {
                ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
                if (shaderFeatures & feature)
                {
                    addDefine(GetShaderFeatureGLSLName(feature));
                }
            }
            glsl << pls::glsl::glsl << '\n';
            glsl << pls::glsl::constants << '\n';
            glsl << pls::glsl::common << '\n';
            if (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND)
            {
                glsl << pls::glsl::advanced_blend << '\n';
            }
            if (context->platformFeatures().avoidFlatVaryings)
            {
                addDefine(GLSL_OPTIONALLY_FLAT);
            }
            else
            {
                glsl << "#define " GLSL_OPTIONALLY_FLAT " flat\n";
            }
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::outerCurvePatches:
                case DrawType::interiorTriangulation:
                    glsl << pls::glsl::draw_path << '\n';
                    break;
                case DrawType::imageMesh:
                    glsl << pls::glsl::draw_image_mesh << '\n';
                    break;
            }

            std::ostringstream vertexGLSL;
            vertexGLSL << "#version 310 es\n";
            vertexGLSL << "#pragma shader_stage(vertex)\n";
            vertexGLSL << "#define " GLSL_VERTEX "\n";
            vertexGLSL << glsl.str();

            std::ostringstream fragmentGLSL;
            fragmentGLSL << "#version 310 es\n";
            fragmentGLSL << "#pragma shader_stage(fragment)\n";
            fragmentGLSL << "#define " GLSL_FRAGMENT "\n";
            fragmentGLSL << glsl.str();

            vertexShader = m_vertexShaderHandle.compileShaderModule(context->m_device,
                                                                    vertexGLSL.str().c_str(),
                                                                    "glsl-raw");
            fragmentShader = m_fragmentShaderHandle.compileShaderModule(context->m_device,
                                                                        fragmentGLSL.str().c_str(),
                                                                        "glsl-raw");
        }
        else
        {
            switch (drawType)
            {
                case DrawType::midpointFanPatches:
                case DrawType::outerCurvePatches:
                    vertexShader =
                        m_vertexShaderHandle.compileSPIRVShaderModule(context->m_device,
                                                                      draw_path_vert,
                                                                      std::size(draw_path_vert));
                    fragmentShader =
                        m_fragmentShaderHandle.compileSPIRVShaderModule(context->m_device,
                                                                        draw_path_frag,
                                                                        std::size(draw_path_frag));
                    break;
                case DrawType::interiorTriangulation:
                    vertexShader = m_vertexShaderHandle.compileSPIRVShaderModule(
                        context->m_device,
                        draw_interior_triangles_vert,
                        std::size(draw_interior_triangles_vert));
                    fragmentShader = m_fragmentShaderHandle.compileSPIRVShaderModule(
                        context->m_device,
                        draw_interior_triangles_frag,
                        std::size(draw_interior_triangles_frag));
                    break;
                case DrawType::imageMesh:
                    RIVE_UNREACHABLE(); // Unimplemented.
                    break;
            }
        }

        std::vector<wgpu::VertexAttribute> attrs;
        size_t attrsStride;
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
                attrs = {
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
                attrsStride = sizeof(pls::PatchVertex);
                break;
            case DrawType::interiorTriangulation:
                attrs = {
                    {
                        .format = wgpu::VertexFormat::Float32x3,
                        .offset = 0,
                        .shaderLocation = 0,
                    },
                };
                attrsStride = sizeof(pls::TriangleVertex);
                break;
            case DrawType::imageMesh:
                RIVE_UNREACHABLE();
                break;
        }

        wgpu::VertexBufferLayout vertexBufferLayout = {
            .arrayStride = attrsStride,
            .stepMode = wgpu::VertexStepMode::Vertex,
            .attributeCount = std::size(attrs),
            .attributes = attrs.data(),
        };

        for (auto framebufferFormat :
             {wgpu::TextureFormat::BGRA8Unorm, wgpu::TextureFormat::RGBA8Unorm})
        {
            wgpu::ColorTargetState colorTargets[] = {
                {.format = framebufferFormat},
                {.format = wgpu::TextureFormat::R32Uint},
                {.format = framebufferFormat},
                {.format = wgpu::TextureFormat::R32Uint},
            };

            wgpu::FragmentState fragmentState = {
                .module = fragmentShader,
                .entryPoint = "main",
                .targetCount = static_cast<size_t>(
                    context->m_pixelLocalStorageType ==
                            PixelLocalStorageType::EXT_shader_pixel_local_storage
                        ? 1
                        : 4),
                .targets = colorTargets,
            };

            wgpu::RenderPipelineDescriptor desc = {
                .layout = context->m_drawPipelineLayout,
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
                        .frontFace = context->m_frontFaceForOnScreenDraws,
                        .cullMode = wgpu::CullMode::Back,
                    },
                .fragment = &fragmentState,
            };

            m_renderPipelines[RenderPipelineIdx(framebufferFormat)] =
                context->m_device.CreateRenderPipeline(&desc);
        }
    }

    const wgpu::RenderPipeline renderPipeline(wgpu::TextureFormat framebufferFormat) const
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

    EmJsHandle m_vertexShaderHandle;
    EmJsHandle m_fragmentShaderHandle;
    wgpu::RenderPipeline m_renderPipelines[2];
};

PLSRenderContextWebGPUImpl::PLSRenderContextWebGPUImpl(
    wgpu::Device device,
    wgpu::Queue queue,
    const PlatformFeatures& baselinePlatformFeatures,
    PixelLocalStorageType pixelLocalStorageType) :
    m_device(device),
    m_queue(queue),
    m_pixelLocalStorageType(pixelLocalStorageType),
    m_frontFaceForOnScreenDraws(wgpu::FrontFace::CW),
    m_colorRampPipeline(std::make_unique<ColorRampPipeline>(m_device)),
    m_tessellatePipeline(std::make_unique<TessellatePipeline>(m_device))
{
    m_platformFeatures = baselinePlatformFeatures;
    m_platformFeatures.invertOffscreenY = true;

    if (m_pixelLocalStorageType == PixelLocalStorageType::EXT_shader_pixel_local_storage &&
        baselinePlatformFeatures.uninvertOnScreenY)
    {
        // We will use "glsl-raw" in order to access EXT_shader_pixel_local_storage, so the WebGPU
        // layer won't actually get a chance to negate Y like it thinks it will.
        m_platformFeatures.uninvertOnScreenY = false;
        // PLS always expects CW, but in this case, we need to specify CCW. This is because the
        // WebGPU layer thinks it's going to negate Y in our shader, and will therefore also flip
        // our frontFace. However, since we will use raw-glsl shaders, the WebGPU layer won't
        // actually get a chance to negate Y like it thinks it will. Therefore, we emit the wrong
        // frontFace, in anticipation of it getting flipped into the correct frontFace on its way to
        // the driver.
        m_frontFaceForOnScreenDraws = wgpu::FrontFace::CCW;
    }

    wgpu::BindGroupLayoutEntry drawBindingLayouts[] = {
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
            .binding = PATH_TEXTURE_IDX,
            .visibility = wgpu::ShaderStage::Vertex,
            .texture =
                {
                    .sampleType = wgpu::TextureSampleType::Uint,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                },
        },
        {
            .binding = CONTOUR_TEXTURE_IDX,
            .visibility = wgpu::ShaderStage::Vertex,
            .texture =
                {
                    .sampleType = wgpu::TextureSampleType::Uint,
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
            .binding = IMAGE_TEXTURE_IDX,
            .visibility = wgpu::ShaderStage::Fragment,
            .texture =
                {
                    .sampleType = wgpu::TextureSampleType::Float,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                },
        },
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                },
        },
        {
            .binding = IMAGE_MESH_UNIFORM_BUFFER_IDX,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                },
        },
    };

    wgpu::BindGroupLayoutDescriptor bindingsDesc = {
        .entryCount = std::size(drawBindingLayouts),
        .entries = drawBindingLayouts,
    };

    m_drawBindGroupLayouts[0] = m_device.CreateBindGroupLayout(&bindingsDesc);

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
            .binding = IMAGE_TEXTURE_IDX,
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

    m_drawBindGroupLayouts[SAMPLER_BINDINGS_SET] =
        m_device.CreateBindGroupLayout(&samplerBindingsDesc);

    wgpu::SamplerDescriptor linearSamplerDesc = {
        .addressModeU = wgpu::AddressMode::ClampToEdge,
        .addressModeV = wgpu::AddressMode::ClampToEdge,
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear,
        .mipmapFilter = wgpu::MipmapFilterMode::Nearest,
    };

    m_linearSampler = m_device.CreateSampler(&linearSamplerDesc);

    wgpu::SamplerDescriptor mipmapSamplerDesc = {
        .addressModeU = wgpu::AddressMode::ClampToEdge,
        .addressModeV = wgpu::AddressMode::ClampToEdge,
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear,
        .mipmapFilter = wgpu::MipmapFilterMode::Nearest,
    };

    m_mipmapSampler = m_device.CreateSampler(&mipmapSamplerDesc);

    wgpu::BindGroupEntry samplerBindings[] = {
        {
            .binding = GRAD_TEXTURE_IDX,
            .sampler = m_linearSampler,
        },
        {
            .binding = IMAGE_TEXTURE_IDX,
            .sampler = m_mipmapSampler,
        },
    };

    wgpu::BindGroupDescriptor samplerBindGroupDesc = {
        .layout = m_drawBindGroupLayouts[SAMPLER_BINDINGS_SET],
        .entryCount = std::size(samplerBindings),
        .entries = samplerBindings,
    };

    m_samplerBindGroup = m_device.CreateBindGroup(&samplerBindGroupDesc);

    wgpu::PipelineLayoutDescriptor drawPipelineLayoutDesc = {
        .bindGroupLayoutCount = std::size(m_drawBindGroupLayouts),
        .bindGroupLayouts = m_drawBindGroupLayouts,
    };

    m_drawPipelineLayout = m_device.CreatePipelineLayout(&drawPipelineLayoutDesc);

    if (m_pixelLocalStorageType == PixelLocalStorageType::EXT_shader_pixel_local_storage)
    {
        // We have to manually implement load/store operations from a shader when using
        // EXT_shader_pixel_local_storage.
        std::ostringstream glsl;
        glsl << "#version 310 es\n";
        glsl << "#pragma shader_stage(vertex)\n";
        glsl << "#define " GLSL_VERTEX "\n";
        // If we are being compiled by SPIRV transpiler for introspection, use gl_VertexIndex
        // instead of gl_VertexID.
        glsl << "#ifndef GL_EXT_shader_pixel_local_storage\n";
        glsl << "#define gl_VertexID gl_VertexIndex\n";
        glsl << "#endif\n";
        glsl << BuildLoadStoreEXTGLSL(LoadStoreActionsEXT::none);
        m_loadStoreEXTVertexShader =
            m_loadStoreEXTVertexShaderHandle.compileShaderModule(m_device,
                                                                 glsl.str().c_str(),
                                                                 "glsl-raw");
        m_loadStoreEXTUniforms = makeUniformBufferRing(1, sizeof(std::array<float, 4>));
    }

    wgpu::BufferDescriptor tessSpanIndexBufferDesc = {
        .usage = wgpu::BufferUsage::Index,
        .size = sizeof(pls::kTessSpanIndices),
        .mappedAtCreation = true,
    };
    m_tessSpanIndexBuffer = m_device.CreateBuffer(&tessSpanIndexBufferDesc);
    memcpy(m_tessSpanIndexBuffer.GetMappedRange(),
           pls::kTessSpanIndices,
           sizeof(pls::kTessSpanIndices));
    m_tessSpanIndexBuffer.Unmap();

    wgpu::BufferDescriptor patchBufferDesc = {
        .usage = wgpu::BufferUsage::Vertex,
        .size = kPatchVertexBufferCount * sizeof(PatchVertex),
        .mappedAtCreation = true,
    };
    m_pathPatchVertexBuffer = m_device.CreateBuffer(&patchBufferDesc);

    patchBufferDesc.size = (kPatchIndexBufferCount * sizeof(uint16_t));
    patchBufferDesc.size = (patchBufferDesc.size + 3) & ~3; // Round up to a multiple of 4.
    patchBufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index;
    m_pathPatchIndexBuffer = m_device.CreateBuffer(&patchBufferDesc);

    GeneratePatchBufferData(
        reinterpret_cast<PatchVertex*>(m_pathPatchVertexBuffer.GetMappedRange()),
        reinterpret_cast<uint16_t*>(m_pathPatchIndexBuffer.GetMappedRange()));

    m_pathPatchVertexBuffer.Unmap();
    m_pathPatchIndexBuffer.Unmap();

    wgpu::TextureDescriptor nullImagePaintTextureDesc = {
        .usage = wgpu::TextureUsage::TextureBinding,
        .dimension = wgpu::TextureDimension::e2D,
        .size = {1, 1},
        .format = wgpu::TextureFormat::RGBA8Unorm,
    };

    m_nullImagePaintTexture = device.CreateTexture(&nullImagePaintTextureDesc);
    m_nullImagePaintTextureView = m_nullImagePaintTexture.CreateView();
}

PLSRenderTargetWebGPU::PLSRenderTargetWebGPU(wgpu::Device device,
                                             wgpu::TextureFormat framebufferFormat,
                                             size_t width,
                                             size_t height) :
    PLSRenderTarget(width, height), m_framebufferFormat(framebufferFormat)
{
    wgpu::TextureDescriptor desc = {
        .usage = wgpu::TextureUsage::RenderAttachment, // | wgpu::TextureUsage::TransientAttachment,
        .size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
    };

    desc.format = m_framebufferFormat;
    m_originalDstColorMemorylessTexture = device.CreateTexture(&desc);

    desc.format = wgpu::TextureFormat::R32Uint;
    m_coverageMemorylessTexture = device.CreateTexture(&desc);
    m_clipMemorylessTexture = device.CreateTexture(&desc);

    m_targetTextureView = {}; // Will be configured later by setTargetTexture().
    m_coverageMemorylessTextureView = m_coverageMemorylessTexture.CreateView();
    m_originalDstColorMemorylessTextureView = m_originalDstColorMemorylessTexture.CreateView();
    m_clipMemorylessTextureView = m_clipMemorylessTexture.CreateView();
}

void PLSRenderTargetWebGPU::setTargetTextureView(wgpu::TextureView textureView)
{
    m_targetTextureView = textureView;
}

rcp<PLSRenderTargetWebGPU> PLSRenderContextWebGPUImpl::makeRenderTarget(
    wgpu::TextureFormat framebufferFormat,
    size_t width,
    size_t height)
{
    return rcp(new PLSRenderTargetWebGPU(m_device, framebufferFormat, width, height));
}

class TexelBufferWebGPU : public TexelBufferRing
{
public:
    TexelBufferWebGPU(wgpu::Device device,
                      wgpu::Queue queue,
                      Format format,
                      size_t widthInItems,
                      size_t height,
                      size_t texelsPerItem,
                      wgpu::TextureUsage extraUsageFlags = wgpu::TextureUsage::None) :
        TexelBufferRing(format, widthInItems, height, texelsPerItem), m_queue(queue)
    {
        wgpu::TextureDescriptor desc{
            .usage =
                wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst | extraUsageFlags,
            .dimension = wgpu::TextureDimension::e2D,
            .size = {static_cast<uint32_t>(widthInItems * texelsPerItem),
                     static_cast<uint32_t>(height)},
        };
        switch (format)
        {
            case Format::rgba8:
                desc.format = wgpu::TextureFormat::RGBA8Unorm;
                break;
            case Format::rgba32f:
                desc.format = wgpu::TextureFormat::RGBA32Float;
                break;
            case Format::rgba32ui:
                desc.format = wgpu::TextureFormat::RGBA32Uint;
                break;
        }

        for (int i = 0; i < kBufferRingSize; ++i)
        {
            m_textures[i] = device.CreateTexture(&desc);
            m_textureViews[i] = m_textures[i].CreateView();
        }
    }

    wgpu::TextureView submittedTextureView() const { return m_textureViews[submittedBufferIdx()]; }

protected:
    void submitTexels(int textureIdx, size_t updateWidthInTexels, size_t updateHeight) override
    {
        if (updateWidthInTexels > 0 && updateHeight > 0)
        {
            wgpu::ImageCopyTexture dest{
                .texture = m_textures[textureIdx],
            };
            wgpu::TextureDataLayout layout{
                .bytesPerRow = static_cast<uint32_t>(widthInTexels() * BytesPerPixel(m_format)),
            };
            wgpu::Extent3D extent{
                .width = static_cast<uint32_t>(updateWidthInTexels),
                .height = static_cast<uint32_t>(updateHeight),
            };
            m_queue.WriteTexture(&dest,
                                 shadowBuffer(),
                                 capacity() * itemSizeInBytes(),
                                 &layout,
                                 &extent);
        }
    }

private:
    const wgpu::Queue m_queue;
    wgpu::Texture m_textures[kBufferRingSize];
    wgpu::TextureView m_textureViews[kBufferRingSize];
};

std::unique_ptr<TexelBufferRing> PLSRenderContextWebGPUImpl::makeTexelBufferRing(
    TexelBufferRing::Format format,
    size_t widthInItems,
    size_t height,
    size_t texelsPerItem,
    int textureIdx,
    TexelBufferRing::Filter)
{
    return std::make_unique<TexelBufferWebGPU>(m_device,
                                               m_queue,
                                               format,
                                               widthInItems,
                                               height,
                                               texelsPerItem);
}

class BufferWebGPU : public BufferRing
{
public:
    BufferWebGPU(wgpu::Device device,
                 wgpu::Queue queue,
                 size_t capacity,
                 size_t itemSizeInBytes,
                 wgpu::BufferUsage usage) :
        BufferRing(capacity, itemSizeInBytes), m_queue(queue)
    {
        for (int i = 0; i < kBufferRingSize; ++i)
        {
            wgpu::BufferDescriptor desc{
                .usage = wgpu::BufferUsage::CopyDst | usage,
                .size = capacity * itemSizeInBytes,
            };
            m_buffers[i] = device.CreateBuffer(&desc);

            m_stagingBuffers[i] = malloc(capacity * itemSizeInBytes);
        }
    }

    ~BufferWebGPU()
    {
        for (int i = 0; i < kBufferRingSize; ++i)
        {
            free(m_stagingBuffers[i]);
        }
    }

    wgpu::Buffer submittedBuffer() const { return m_buffers[submittedBufferIdx()]; }

protected:
    void* onMapBuffer(int bufferIdx) override { return m_stagingBuffers[bufferIdx]; }

    void onUnmapAndSubmitBuffer(int bufferIdx, size_t bytesWritten) override
    {
        m_queue.WriteBuffer(m_buffers[bufferIdx], 0, m_stagingBuffers[bufferIdx], bytesWritten);
    }

private:
    const wgpu::Queue m_queue;
    wgpu::Buffer m_buffers[kBufferRingSize];
    void* m_stagingBuffers[kBufferRingSize];
};

std::unique_ptr<BufferRing> PLSRenderContextWebGPUImpl::makeVertexBufferRing(size_t capacity,
                                                                             size_t itemSizeInBytes)
{
    return std::make_unique<BufferWebGPU>(m_device,
                                          m_queue,
                                          capacity,
                                          itemSizeInBytes,
                                          wgpu::BufferUsage::Vertex);
}

std::unique_ptr<BufferRing> PLSRenderContextWebGPUImpl::makePixelUnpackBufferRing(
    size_t capacity,
    size_t itemSizeInBytes)
{
    return std::make_unique<BufferWebGPU>(m_device,
                                          m_queue,
                                          capacity,
                                          itemSizeInBytes,
                                          wgpu::BufferUsage::CopySrc);
}

std::unique_ptr<BufferRing> PLSRenderContextWebGPUImpl::makeUniformBufferRing(
    size_t capacity,
    size_t itemSizeInBytes)
{
    return std::make_unique<BufferWebGPU>(m_device,
                                          m_queue,
                                          capacity,
                                          itemSizeInBytes,
                                          wgpu::BufferUsage::Uniform);
}

void PLSRenderContextWebGPUImpl::resizeGradientTexture(size_t height)
{
    wgpu::TextureDescriptor desc{
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding |
                 wgpu::TextureUsage::CopyDst,
        .size = {kGradTextureWidth, static_cast<uint32_t>(height)},
        .format = wgpu::TextureFormat::RGBA8Unorm,
    };

    m_gradientTexture = m_device.CreateTexture(&desc);
    m_gradientTextureView = m_gradientTexture.CreateView();
}

void PLSRenderContextWebGPUImpl::resizeTessellationTexture(size_t height)
{
    wgpu::TextureDescriptor desc{
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .size = {kTessTextureWidth, static_cast<uint32_t>(height)},
        .format = wgpu::TextureFormat::RGBA32Uint,
    };

    m_tessVertexTexture = m_device.CreateTexture(&desc);
    m_tessVertexTextureView = m_tessVertexTexture.CreateView();
}

static wgpu::Buffer webgpu_buffer(const BufferRing* bufferRing)
{
    return static_cast<const BufferWebGPU*>(bufferRing)->submittedBuffer();
}

static wgpu::TextureView webgpu_texture_view(const TexelBufferRing* texelBufferRing)
{
    return static_cast<const TexelBufferWebGPU*>(texelBufferRing)->submittedTextureView();
}

void PLSRenderContextWebGPUImpl::flush(const PLSRenderContext::FlushDescriptor& desc)
{
    auto* renderTarget = static_cast<const PLSRenderTargetWebGPU*>(desc.renderTarget);
    wgpu::CommandEncoder encoder = m_device.CreateCommandEncoder();

    // Render the complex color ramps to the gradient texture.
    if (desc.complexGradSpanCount > 0)
    {
        wgpu::BindGroupEntry bindings[] = {
            {
                .binding = FLUSH_UNIFORM_BUFFER_IDX,
                .buffer = webgpu_buffer(flushUniformBufferRing()),
            },
        };

        wgpu::BindGroupDescriptor bindGroupDesc = {
            .layout = m_colorRampPipeline->bindGroupLayout(),
            .entryCount = std::size(bindings),
            .entries = bindings,
        };

        wgpu::BindGroup bindGroup = m_device.CreateBindGroup(&bindGroupDesc);

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

        wgpu::RenderPassEncoder gradPass = encoder.BeginRenderPass(&gradPassDesc);
        gradPass.SetViewport(0.f,
                             static_cast<double>(desc.complexGradRowsTop),
                             pls::kGradTextureWidth,
                             static_cast<float>(desc.complexGradRowsHeight),
                             0.0,
                             1.0);
        gradPass.SetPipeline(m_colorRampPipeline->renderPipeline());
        gradPass.SetVertexBuffer(0, webgpu_buffer(gradSpanBufferRing()));
        gradPass.SetBindGroup(0, bindGroup);
        gradPass.Draw(4, desc.complexGradSpanCount, 0, 0);
        gradPass.End();
    }

    // Copy the simple color ramps to the gradient texture.
    if (desc.simpleGradTexelsHeight > 0)
    {
        wgpu::ImageCopyBuffer srcBuffer = {
            .layout =
                {
                    .offset = 0,
                    .bytesPerRow = pls::kGradTextureWidth * 4,
                },
            .buffer = webgpu_buffer(simpleColorRampsBufferRing()),
        };
        wgpu::ImageCopyTexture dstTexture = {
            .texture = m_gradientTexture,
            .origin = {0, 0, 0},
        };
        wgpu::Extent3D copySize = {
            .width = desc.simpleGradTexelsWidth,
            .height = desc.simpleGradTexelsHeight,
        };
        encoder.CopyBufferToTexture(&srcBuffer, &dstTexture, &copySize);
    }

    // Tessellate all curves into vertices in the tessellation texture.
    if (desc.tessVertexSpanCount > 0)
    {
        wgpu::BindGroupEntry bindings[] = {
            {
                .binding = PATH_TEXTURE_IDX,
                .textureView = webgpu_texture_view(pathBufferRing()),
            },
            {
                .binding = CONTOUR_TEXTURE_IDX,
                .textureView = webgpu_texture_view(contourBufferRing()),
            },
            {
                .binding = FLUSH_UNIFORM_BUFFER_IDX,
                .buffer = webgpu_buffer(flushUniformBufferRing()),
            },
        };

        wgpu::BindGroupDescriptor bindGroupDesc = {
            .layout = m_tessellatePipeline->bindGroupLayout(),
            .entryCount = std::size(bindings),
            .entries = bindings,
        };

        wgpu::BindGroup bindGroup = m_device.CreateBindGroup(&bindGroupDesc);

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

        wgpu::RenderPassEncoder tessPass = encoder.BeginRenderPass(&tessPassDesc);
        tessPass.SetViewport(0.f,
                             0.f,
                             pls::kTessTextureWidth,
                             static_cast<float>(desc.tessDataHeight),
                             0.0,
                             1.0);
        tessPass.SetPipeline(m_tessellatePipeline->renderPipeline());
        tessPass.SetVertexBuffer(0, webgpu_buffer(tessSpanBufferRing()));
        tessPass.SetIndexBuffer(m_tessSpanIndexBuffer, wgpu::IndexFormat::Uint16);
        tessPass.SetBindGroup(0, bindGroup);
        tessPass.DrawIndexed(std::size(pls::kTessSpanIndices), desc.tessVertexSpanCount, 0);
        tessPass.End();
    }

    wgpu::BindGroupEntry bindings[] = {
        {
            .binding = TESS_VERTEX_TEXTURE_IDX,
            .textureView = m_tessVertexTextureView,
        },
        {
            .binding = PATH_TEXTURE_IDX,
            .textureView = webgpu_texture_view(pathBufferRing()),
        },
        {
            .binding = CONTOUR_TEXTURE_IDX,
            .textureView = webgpu_texture_view(contourBufferRing()),
        },
        {
            .binding = GRAD_TEXTURE_IDX,
            .textureView = m_gradientTextureView,
        },
        {
            .binding = IMAGE_TEXTURE_IDX,
            .textureView = m_nullImagePaintTextureView,
        },
        {
            .binding = FLUSH_UNIFORM_BUFFER_IDX,
            .buffer = webgpu_buffer(flushUniformBufferRing()),
        },
        {
            .binding = IMAGE_MESH_UNIFORM_BUFFER_IDX,
            .buffer = webgpu_buffer(imageMeshUniformBufferRing()),
        },
    };

    wgpu::BindGroupDescriptor bindGroupDesc = {
        .layout = m_drawBindGroupLayouts[0],
        .entryCount = std::size(bindings),
        .entries = bindings,
    };

    wgpu::BindGroup bindGroup = m_device.CreateBindGroup(&bindGroupDesc);

    wgpu::RenderPassColorAttachment plsAttachments[4]{
        {
            // framebuffer
            .view = renderTarget->m_targetTextureView,
            .storeOp = wgpu::StoreOp::Store,
        },
        {
            // coverage
            .view = renderTarget->m_coverageMemorylessTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Discard,
            .clearValue = {},
        },
        {
            // originalDstColor
            .view = renderTarget->m_originalDstColorMemorylessTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Discard,
            .clearValue = {},
        },
        {
            // clip
            .view = renderTarget->m_clipMemorylessTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Discard,
            .clearValue = {},
        },
    };

    if (desc.loadAction == LoadAction::clear)
    {
        float cc[4];
        UnpackColorToRGBA32F(desc.clearColor, cc);
        plsAttachments[0].loadOp = wgpu::LoadOp::Clear;
        plsAttachments[0].clearValue = wgpu::Color{cc[0], cc[1], cc[2], cc[3]};
    }
    else
    {
        plsAttachments[0].loadOp = wgpu::LoadOp::Load;
    }

    wgpu::RenderPassDescriptor passDesc = {
        .colorAttachmentCount = static_cast<size_t>(
            m_pixelLocalStorageType == PixelLocalStorageType::EXT_shader_pixel_local_storage ? 1
                                                                                             : 4),
        .colorAttachments = plsAttachments,
    };

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDesc);
    pass.SetViewport(0.f, 0.f, renderTarget->width(), renderTarget->height(), 0.0, 1.0);

    if (m_pixelLocalStorageType == PixelLocalStorageType::EXT_shader_pixel_local_storage)
    {
        enable_shader_pixel_local_storage_ext(pass, true);

        // Draw the load action for EXT_shader_pixel_local_storage.
        std::array<float, 4> clearColor;
        LoadStoreActionsEXT loadActions = pls::BuildLoadActionsEXT(desc, &clearColor);
        const LoadStoreEXTPipeline& loadPipeline =
            m_loadStoreEXTPipelines
                .try_emplace(loadActions, this, loadActions, renderTarget->framebufferFormat())
                .first->second;

        if (loadActions & LoadStoreActionsEXT::clearColor)
        {
            void* uniformData = m_loadStoreEXTUniforms->mapBuffer();
            memcpy(uniformData, clearColor.data(), sizeof(clearColor));
            m_loadStoreEXTUniforms->unmapAndSubmitBuffer(sizeof(clearColor));

            wgpu::BindGroupEntry uniformBindings[] = {
                {
                    .binding = 0,
                    .buffer = webgpu_buffer(m_loadStoreEXTUniforms.get()),
                },
            };

            wgpu::BindGroupDescriptor uniformBindGroupDesc = {
                .layout = loadPipeline.bindGroupLayout(),
                .entryCount = std::size(uniformBindings),
                .entries = uniformBindings,
            };

            wgpu::BindGroup uniformBindGroup = m_device.CreateBindGroup(&uniformBindGroupDesc);

            pass.SetBindGroup(0, uniformBindGroup);
        }

        pass.SetPipeline(loadPipeline.renderPipeline(renderTarget->framebufferFormat()));
        pass.Draw(4);
    }

    pass.SetBindGroup(0, bindGroup);
    pass.SetBindGroup(1, m_samplerBindGroup);
    pass.SetIndexBuffer(m_pathPatchIndexBuffer, wgpu::IndexFormat::Uint16);

    // Execute the DrawList.
    for (const Draw& draw : *desc.drawList)
    {
        if (draw.elementCount == 0)
        {
            continue;
        }

        DrawType drawType = draw.drawType;
        if (drawType == DrawType::imageMesh)
        {
            continue; // Unimplemented.
        }

        // Setup the pipeline for this specific drawType and shaderFeatures.
        const DrawPipeline& drawPipeline =
            m_drawPipelines
                .try_emplace(pls::ShaderUniqueKey(drawType, draw.shaderFeatures),
                             this,
                             drawType,
                             draw.shaderFeatures)
                .first->second;
        pass.SetPipeline(drawPipeline.renderPipeline(renderTarget->framebufferFormat()));

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
            {
                // Draw PLS patches that connect the tessellation vertices.
                pass.SetVertexBuffer(0, m_pathPatchVertexBuffer);
                pass.DrawIndexed(pls::PatchIndexCount(drawType),
                                 draw.elementCount,
                                 pls::PatchBaseIndex(drawType),
                                 0,
                                 draw.baseElement);
                break;
            }
            case DrawType::interiorTriangulation:
            {
                pass.SetVertexBuffer(0, webgpu_buffer(triangleBufferRing()));
                pass.Draw(draw.elementCount, 1, draw.baseElement);
                break;
            }
            case DrawType::imageMesh:
            {
                break;
            }
        }
    }

    if (m_pixelLocalStorageType == PixelLocalStorageType::EXT_shader_pixel_local_storage)
    {
        // Draw the store action for EXT_shader_pixel_local_storage.
        LoadStoreActionsEXT actions = LoadStoreActionsEXT::storeColor;
        auto it = m_loadStoreEXTPipelines.try_emplace(actions,
                                                      this,
                                                      actions,
                                                      renderTarget->framebufferFormat());
        LoadStoreEXTPipeline* storePipeline = &it.first->second;
        pass.SetPipeline(storePipeline->renderPipeline(renderTarget->framebufferFormat()));
        pass.Draw(4);

        enable_shader_pixel_local_storage_ext(pass, false);
    }

    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    m_queue.Submit(1, &commands);
}

std::unique_ptr<PLSRenderContext> PLSRenderContextWebGPUImpl::MakeContext(
    wgpu::Device device,
    wgpu::Queue queue,
    const pls::PlatformFeatures& baselinePlatformFeatures,
    PixelLocalStorageType pixelLocalStorageType)
{
    return std::make_unique<PLSRenderContext>(std::unique_ptr<PLSRenderContextWebGPUImpl>(
        new PLSRenderContextWebGPUImpl(device,
                                       queue,
                                       baselinePlatformFeatures,
                                       pixelLocalStorageType)));
}
} // namespace rive::pls

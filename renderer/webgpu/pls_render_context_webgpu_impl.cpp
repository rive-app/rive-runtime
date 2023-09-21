/*
 * Copyright 2023 Rive
 */

#include <sstream>
#include <string>

#include "../gl/gl_utils.hpp"
#include "../shaders/constants.glsl"
#include "rive/pls/webgpu/pls_render_context_webgpu_impl.hpp"

#include "../out/obj/generated/spirv/color_ramp.vert.h"
#include "../out/obj/generated/spirv/color_ramp.frag.h"
#include "../out/obj/generated/spirv/tessellate.vert.h"
#include "../out/obj/generated/spirv/tessellate.frag.h"
#include "../out/obj/generated/spirv/draw_path.vert.h"
#include "../out/obj/generated/spirv/draw_path.frag.h"
#include "../out/obj/generated/spirv/draw_interior_triangles.vert.h"
#include "../out/obj/generated/spirv/draw_interior_triangles.frag.h"

#ifdef RIVE_DAWN

#include <dawn/webgpu_cpp.h>

#else

#include <emscripten/html5_webgpu.h>
#include <emscripten.h>

EM_JS(int, create_shader_module, (int device, const char* source), {
    device = JsValStore.get(device);
    source = UTF8ToString(source);

    return JsValStore.add(device.createShaderModule({
        language : "glsl",
        code : source,
    }));
});

#endif

namespace rive::pls
{
wgpu::ShaderModule compile_spirv_shader_module(wgpu::Device device,
                                               const uint32_t* code,
                                               uint32_t codeSize)
{
    wgpu::ShaderModuleSPIRVDescriptor spirvDesc;
    spirvDesc.code = code;
    spirvDesc.codeSize = codeSize;
    wgpu::ShaderModuleDescriptor descriptor;
    descriptor.nextInChain = &spirvDesc;
    return device.CreateShaderModule(&descriptor);
}

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
            compile_spirv_shader_module(device, color_ramp_vert, std::size(color_ramp_vert));

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
            compile_spirv_shader_module(device, color_ramp_frag, std::size(color_ramp_frag));

        wgpu::ColorTargetState colorTargetState = {.format = wgpu::TextureFormat::RGBA8Unorm};

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
                    .frontFace = wgpu::FrontFace::CW,
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
    wgpu::RenderPipeline m_renderPipeline;
};

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
            compile_spirv_shader_module(device, tessellate_vert, std::size(tessellate_vert));

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
            compile_spirv_shader_module(device, tessellate_frag, std::size(tessellate_frag));

        wgpu::ColorTargetState colorTargetState = {.format = wgpu::TextureFormat::RGBA32Uint};

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
                    .frontFace = wgpu::FrontFace::CW,
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
    wgpu::RenderPipeline m_renderPipeline;
};

class PLSRenderContextWebGPUImpl::DrawPipeline
{
public:
    DrawPipeline(PLSRenderContextWebGPUImpl* context,
                 DrawType drawType,
                 ShaderFeatures shaderFeatures)
    {
        wgpu::ShaderModule vertexShader, fragmentShader;
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::outerCurvePatches:
                vertexShader = compile_spirv_shader_module(context->m_device,
                                                           draw_path_vert,
                                                           std::size(draw_path_vert));
                fragmentShader = compile_spirv_shader_module(context->m_device,
                                                             draw_path_frag,
                                                             std::size(draw_path_frag));
                break;
            case DrawType::interiorTriangulation:
                vertexShader = compile_spirv_shader_module(context->m_device,
                                                           draw_interior_triangles_vert,
                                                           std::size(draw_interior_triangles_vert));
                fragmentShader =
                    compile_spirv_shader_module(context->m_device,
                                                draw_interior_triangles_frag,
                                                std::size(draw_interior_triangles_frag));
                break;
            case DrawType::imageMesh:
                RIVE_UNREACHABLE(); // Unimplemented.
                break;
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

        wgpu::ColorTargetState colorTargets[] = {
            {.format = wgpu::TextureFormat::BGRA8Unorm},
            {.format = wgpu::TextureFormat::R32Uint},
            {.format = wgpu::TextureFormat::BGRA8Unorm},
            {.format = wgpu::TextureFormat::R32Uint},
        };

        wgpu::FragmentState fragmentState = {
            .module = fragmentShader,
            .entryPoint = "main",
            .targetCount = std::size(colorTargets),
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
                    .frontFace = wgpu::FrontFace::CW,
                    .cullMode = wgpu::CullMode::Back,
                },
            .fragment = &fragmentState,
        };

        m_renderPipeline = context->m_device.CreateRenderPipeline(&desc);
    }

    const wgpu::RenderPipeline renderPipeline() const { return m_renderPipeline; }

private:
    wgpu::RenderPipeline m_renderPipeline;
};

PLSRenderContextWebGPUImpl::PLSRenderContextWebGPUImpl(wgpu::Device device,
                                                       wgpu::Queue queue,
                                                       GLExtensions extensions) :
    m_device(device),
    m_queue(queue),
    m_colorRampPipeline(std::make_unique<ColorRampPipeline>(m_device)),
    m_tessellatePipeline(std::make_unique<TessellatePipeline>(m_device))
{
    m_platformFeatures.invertOffscreenY = true;

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
        .mipmapFilter = wgpu::MipmapFilterMode::Linear,
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

    for (auto drawType : {DrawType::midpointFanPatches, DrawType::interiorTriangulation})
    {
        ShaderFeatures allShaderFeatures = pls::AllShaderFeaturesForDrawType(drawType);
        uint32_t fullyFeaturedDrawPathKey = pls::ShaderUniqueKey(drawType, allShaderFeatures);
        m_drawPipelines.try_emplace(fullyFeaturedDrawPathKey, this, drawType, allShaderFeatures);
    }

#if 0
    // We have to manually implement load/store operations from a shader when using
    // EXT_shader_pixel_local_storage.
    m_plsLoadStoreModule =
        compile_shader_module(m_device, extensions, GL_VERTEX_SHADER, {glsl::pls_load_store_ext});
#endif

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
                                             wgpu::TextureFormat pixelFormat,
                                             size_t width,
                                             size_t height) :
    PLSRenderTarget(width, height), m_pixelFormat(pixelFormat)
{
    wgpu::TextureDescriptor desc = {
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TransientAttachment,
        .size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
    };

    desc.format = m_pixelFormat;
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
    wgpu::TextureFormat pixelFormat,
    size_t width,
    size_t height)
{
    return rcp(new PLSRenderTargetWebGPU(m_device, pixelFormat, width, height));
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
        m_queue(queue), TexelBufferRing(format, widthInItems, height, texelsPerItem)
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
        m_queue(queue), BufferRing(capacity, itemSizeInBytes)
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
        wgpu::RenderPassColorAttachment attachment = {
            .view = m_gradientTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {},
        };

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
        wgpu::RenderPassColorAttachment attachment{
            .view = m_tessVertexTextureView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = {},
        };

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

    wgpu::RenderPassDescriptor passDesc = {
        .colorAttachmentCount = 4,
        .colorAttachments = plsAttachments,
    };

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDesc);

    pass.SetViewport(0.f, 0.f, renderTarget->width(), renderTarget->height(), 0.0, 1.0);
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
        auto it = m_drawPipelines.find(
            pls::ShaderUniqueKey(drawType, pls::AllShaderFeaturesForDrawType(drawType)));
        assert(it != m_drawPipelines.end());
        DrawPipeline* drawPipeline = &it->second;
        pass.SetPipeline(drawPipeline->renderPipeline());

#if 0
        // Bind the appropriate image texture, if any.
        if (auto imageTextureMetal = static_cast<const PLSTextureMetalImpl*>(draw.imageTextureRef))
        {
            [encoder setFragmentTexture:imageTextureMetal->texture() atIndex:IMAGE_TEXTURE_IDX];
        }
        pass.SetBindGroup(0, bindGroup);
#endif

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

    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    m_queue.Submit(1, &commands);
}

std::unique_ptr<PLSRenderContext> PLSRenderContextWebGPUImpl::MakeContext(wgpu::Device device,
                                                                          wgpu::Queue queue)
{
    GLExtensions extensions{};
    auto plsContextImpl = std::unique_ptr<PLSRenderContextWebGPUImpl>(
        new PLSRenderContextWebGPUImpl(device, queue, extensions));
    return std::make_unique<PLSRenderContext>(std::move(plsContextImpl));
}
} // namespace rive::pls

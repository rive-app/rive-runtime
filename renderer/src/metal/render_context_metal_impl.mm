/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/metal/render_context_metal_impl.h"

#include "rive/decoders/astc_footprints.hpp"

#include "background_shader_compiler.h"
#include "rive/renderer/buffer_ring.hpp"
#include "rive/renderer/draw.hpp"
#include "rive/renderer/range_chunker.hpp"
#ifdef RIVE_CANVAS
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/ore/ore_context_metal.hpp"
#endif
#include "rive/renderer/texture.hpp"
#include "rive/renderer/rive_render_buffer.hpp"
#include "shaders/constants.glsl"
#include <algorithm>
#include <cmath>
#include <sstream>

#include "generated/shaders/color_ramp.glsl.exports.h"
#include "generated/shaders/tessellate.glsl.exports.h"

#if defined(RIVE_IOS_SIMULATOR)
#import <mach-o/arch.h>
#endif

namespace rive::gpu
{
#if defined(RIVE_IOS)
#include "generated/shaders/rive_pls_ios.metallib.c"
#elif defined(RIVE_IOS_SIMULATOR)
#include "generated/shaders/rive_pls_ios_simulator.metallib.c"
#elif defined(RIVE_XROS)
#include "generated/shaders/rive_renderer_xros.metallib.c"
#elif defined(RIVE_XROS_SIMULATOR)
#include "generated/shaders/rive_renderer_xros_simulator.metallib.c"
#elif defined(RIVE_APPLETVOS)
#include "generated/shaders/rive_renderer_appletvos.metallib.c"
#elif defined(RIVE_APPLETVOS_SIMULATOR)
#include "generated/shaders/rive_renderer_appletvsimulator.metallib.c"
#else
#include "generated/shaders/rive_pls_macosx.metallib.c"
#endif

static id<MTLRenderPipelineState> make_pipeline_state(
    id<MTLDevice> gpu, MTLRenderPipelineDescriptor* desc)
{
  NSError* err = nil;
  id<MTLRenderPipelineState> state = [gpu
      newRenderPipelineStateWithDescriptor:desc
                                     error:&err];
  if (err != nil || state == nil)
  {
    NSLog(
        @"RIVE: make_pipeline_state error %@",
        err != nil ? err.localizedDescription : @"<nil>");
  }
  return state;
}

static MTLStencilOperation metalStencilOp(gpu::StencilOp op)
{
  switch (op)
  {
    case gpu::StencilOp::keep:
      return MTLStencilOperationKeep;
    case gpu::StencilOp::replace:
      return MTLStencilOperationReplace;
    case gpu::StencilOp::zero:
      return MTLStencilOperationZero;
    case gpu::StencilOp::decrClamp:
      return MTLStencilOperationDecrementClamp;
    case gpu::StencilOp::incrWrap:
      return MTLStencilOperationIncrementWrap;
    case gpu::StencilOp::decrWrap:
      return MTLStencilOperationDecrementWrap;
  }
  RIVE_UNREACHABLE();
}

static MTLCompareFunction metalStencilCompare(gpu::StencilCompareOp op)
{
  switch (op)
  {
    case gpu::StencilCompareOp::less:
      return MTLCompareFunctionLess;
    case gpu::StencilCompareOp::equal:
      return MTLCompareFunctionEqual;
    case gpu::StencilCompareOp::lessOrEqual:
      return MTLCompareFunctionLessEqual;
    case gpu::StencilCompareOp::notEqual:
      return MTLCompareFunctionNotEqual;
    case gpu::StencilCompareOp::always:
      return MTLCompareFunctionAlways;
  }
  RIVE_UNREACHABLE();
}

static MTLStencilDescriptor* makeStencilDescriptor(
    const gpu::StencilFaceOps& ops, uint8_t compareMask, uint8_t writeMask)
{
  MTLStencilDescriptor* desc = [[MTLStencilDescriptor alloc] init];
  desc.stencilCompareFunction = metalStencilCompare(ops.compareOp);
  desc.stencilFailureOperation = metalStencilOp(ops.stencilFailOp);
  desc.depthFailureOperation = metalStencilOp(ops.depthFailOp);
  desc.depthStencilPassOperation = metalStencilOp(ops.depthStencilPassOp);
  desc.readMask = compareMask;
  desc.writeMask = writeMask;
  return desc;
}

// Metal's default front face is clockwise, which is also Rive's front face, so
// the cull faces map straight across.
static MTLCullMode metalCullMode(gpu::CullFace cullFace)
{
  switch (cullFace)
  {
    case gpu::CullFace::none:
      return MTLCullModeNone;
    case gpu::CullFace::clockwise:
      return MTLCullModeFront;
    case gpu::CullFace::counterclockwise:
      return MTLCullModeBack;
  }
  RIVE_UNREACHABLE();
}

// Depth/stencil attachment format for InterlockMode::depthStencil.
// Depth24Unorm_Stencil8 doesn't exist on Apple GPUs, and the packed depth
// values we emit are exact in float32, so this works everywhere.
constexpr static MTLPixelFormat
    kDepthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

static MTLSamplerAddressMode address_mode_for_image_wrap(ImageWrap wrap)
{
  switch (wrap)
  {
    case ImageWrap::clamp:
      return MTLSamplerAddressModeClampToEdge;
    case ImageWrap::repeat:
      return MTLSamplerAddressModeRepeat;
    case ImageWrap::mirror:
      return MTLSamplerAddressModeMirrorRepeat;
  }

  RIVE_UNREACHABLE();
}

static MTLSamplerMinMagFilter min_mag_filter_for_image_filter(
    ImageFilter option)
{
  switch (option)
  {
    case ImageFilter::bilinear:
      return MTLSamplerMinMagFilterLinear;
    case ImageFilter::nearest:
      return MTLSamplerMinMagFilterNearest;
  }

  RIVE_UNREACHABLE();
}

static MTLSamplerMipFilter mip_filter_for_image_filter(ImageFilter option)
{
  switch (option)
  {
    case ImageFilter::nearest:
    case ImageFilter::bilinear:
      return MTLSamplerMipFilterNearest;
  }

  RIVE_UNREACHABLE();
}

// Renders color ramps to the gradient texture.
class RenderContextMetalImpl::ColorRampPipeline
{
public:
  ColorRampPipeline(id<MTLDevice> gpu, id<MTLLibrary> plsLibrary)
  {
    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc]
        init];
    desc.vertexFunction = [plsLibrary
        newFunctionWithName:@GLSL_colorRampVertexMain];
    desc.fragmentFunction = [plsLibrary
        newFunctionWithName:@GLSL_colorRampFragmentMain];
    desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    m_pipelineState = make_pipeline_state(gpu, desc);
  }

  id<MTLRenderPipelineState> pipelineState() const { return m_pipelineState; }

private:
  id<MTLRenderPipelineState> m_pipelineState;
};

// Renders tessellated vertices to the tessellation texture.
class RenderContextMetalImpl::TessellatePipeline
{
public:
  TessellatePipeline(id<MTLDevice> gpu, id<MTLLibrary> plsLibrary)
  {
    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc]
        init];
    desc.vertexFunction = [plsLibrary
        newFunctionWithName:@GLSL_tessellateVertexMain];
    desc.fragmentFunction = [plsLibrary
        newFunctionWithName:@GLSL_tessellateFragmentMain];
    desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA32Uint;
    m_pipelineState = make_pipeline_state(gpu, desc);
  }

  id<MTLRenderPipelineState> pipelineState() const { return m_pipelineState; }

private:
  id<MTLRenderPipelineState> m_pipelineState;
};

// Renders feathered fills and strokes to the feather atlas.
class RenderContextMetalImpl::FeatherAtlasPipeline
{
public:
  FeatherAtlasPipeline(
      id<MTLDevice> gpu,
      id<MTLLibrary> plsLibrary,
      NSString* fragmentMain,
      MTLBlendOperation blendOperation)
  {
    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc]
        init];
    desc.vertexFunction = [plsLibrary
        newFunctionWithName:@GLSL_atlasVertexMain];
    desc.fragmentFunction = [plsLibrary newFunctionWithName:fragmentMain];
    desc.colorAttachments[0].pixelFormat = MTLPixelFormatR16Float;
    desc.colorAttachments[0].blendingEnabled = TRUE;
    desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
    desc.colorAttachments[0].rgbBlendOperation = blendOperation;
    desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
    desc.colorAttachments[0].alphaBlendOperation = blendOperation;
    desc.colorAttachments[0].writeMask = MTLColorWriteMaskAll;
    m_pipelineState = make_pipeline_state(gpu, desc);
  }

  id<MTLRenderPipelineState> pipelineState() const { return m_pipelineState; }

private:
  id<MTLRenderPipelineState> m_pipelineState;
};

// A compiled draw shader, from which DrawPipelines are built.
class RenderContextMetalImpl::DrawShader
{
public:
  // Precompiled functions are embedded in namespaces. Return the fully
  // qualified name of the desired function, including its namespace.
  static NSString* GetPrecompiledFunctionName(
      DrawType drawType,
      gpu::ShaderFeatures shaderFeatures,
      gpu::ShaderMiscFlags shaderMiscFlags,
      id<MTLLibrary> precompiledLibrary,
      const char* functionBaseName)
  {
    // Each feature corresponds to a specific index in the namespaceID.
    // These must stay in sync with generate_draw_combinations.py.
    char namespaceID[] = "000000000000";
    static_assert(
        sizeof(namespaceID) ==
        gpu::ShaderFeatureCount + 1 /*DRAW_INTERIOR_TRIANGLES*/ +
            1 /*FEATHER_ATLAS_BLIT*/ + 1 /*null terminator*/);
    for (size_t i = 0; i < gpu::ShaderFeatureCount; ++i)
    {
      const auto feature = ShaderFeatures(1 << i);
      if (enums::is_flag_set(shaderFeatures, feature))
      {
        namespaceID[i] = '1';
      }
      static_assert((int)ShaderFeatures::ENABLE_CLIPPING == 1 << 0);
      static_assert((int)ShaderFeatures::ENABLE_CLIP_RECT == 1 << 1);
      static_assert((int)ShaderFeatures::ENABLE_ADVANCED_BLEND == 1 << 2);
      static_assert((int)ShaderFeatures::ENABLE_FEATHER == 1 << 3);
      static_assert((int)ShaderFeatures::ENABLE_MODULATED_IMAGE == 1 << 4);
      static_assert((int)ShaderFeatures::ENABLE_EVEN_ODD == 1 << 5);
      static_assert((int)ShaderFeatures::ENABLE_NESTED_CLIPPING == 1 << 6);
      static_assert((int)ShaderFeatures::ENABLE_HSL_BLEND_MODES == 1 << 7);
      static_assert((int)ShaderFeatures::ENABLE_DITHER == 1 << 8);
      static_assert((int)ShaderFeatures::ENABLE_ANGULAR_GRADIENT == 1 << 9);
    }
    if (drawType == DrawType::interiorTriangulation)
    {
      namespaceID[gpu::ShaderFeatureCount] = '1';
    }
    else if (drawType == DrawType::featherAtlasBlit)
    {
      namespaceID[gpu::ShaderFeatureCount] = '1';
      namespaceID[gpu::ShaderFeatureCount + 1] = '1';
    }

    char namespacePrefix;
    switch (drawType)
    {
      case DrawType::midpointFanPatches:
      case DrawType::midpointFanCenterAAPatches:
      case DrawType::outerCurvePatches:
      case DrawType::interiorTriangulation:
      case DrawType::featherAtlasBlit:
        namespacePrefix = enums::is_flag_set(
                              shaderMiscFlags,
                              gpu::ShaderMiscFlags::clockwiseFill)
                              ? 'c'
                              : 'p';
        break;
      case DrawType::imageRect:
        RIVE_UNREACHABLE();
      case DrawType::imageMesh:
        namespacePrefix = 'm';
        break;
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
      case DrawType::renderPassResolve:
      case DrawType::depthStrokes:
      case DrawType::depthAAStrokes:
      case DrawType::depthAAOuterHairline:
        RIVE_UNREACHABLE();
    }

    return [NSString stringWithFormat:@"%c%s::%s",
                                      namespacePrefix,
                                      namespaceID,
                                      functionBaseName];
  }

  DrawShader(
      id<MTLLibrary> library,
      NSString* vertexFunctionName,
      NSString* fragmentFunctionName,
      gpu::DrawType drawType,
      gpu::InterlockMode interlockMode,
      gpu::ShaderMiscFlags shaderMiscFlags
#ifdef WITH_RIVE_TOOLS
      ,
      gpu::SynthesizedFailureType synthesizedFailureType
#endif
      )
      : m_drawType(drawType)
      , m_interlockMode(interlockMode)
      , m_shaderMiscFlags(shaderMiscFlags)
#ifdef WITH_RIVE_TOOLS
      , m_synthesizedFailureType(synthesizedFailureType)
#endif
  {
    if (library == nil)
    {
      // This shader failed to compile. Leave everything nil and let draws
      // fail.
      return;
    }
    m_vertexMain = [library newFunctionWithName:vertexFunctionName];
    m_fragmentMain = [library newFunctionWithName:fragmentFunctionName];
    if (m_vertexMain == nil || m_fragmentMain == nil)
    {
      NSLog(
          @"RIVE: missing shader function %@ / %@",
          vertexFunctionName,
          fragmentFunctionName);
      m_vertexMain = m_fragmentMain = nil;
    }
  }

  bool valid() const { return m_vertexMain != nil; }

  gpu::DrawType drawType() const { return m_drawType; }
  gpu::InterlockMode interlockMode() const { return m_interlockMode; }
  gpu::ShaderMiscFlags shaderMiscFlags() const { return m_shaderMiscFlags; }
  id<MTLFunction> vertexMain() const { return m_vertexMain; }
  id<MTLFunction> fragmentMain() const { return m_fragmentMain; }
#ifdef WITH_RIVE_TOOLS
  gpu::SynthesizedFailureType synthesizedFailureType() const
  {
    return m_synthesizedFailureType;
  }
#endif

private:
  const gpu::DrawType m_drawType;
  const gpu::InterlockMode m_interlockMode;
  const gpu::ShaderMiscFlags m_shaderMiscFlags;
#ifdef WITH_RIVE_TOOLS
  const gpu::SynthesizedFailureType m_synthesizedFailureType;
#endif
  id<MTLFunction> m_vertexMain = nil;
  id<MTLFunction> m_fragmentMain = nil;
};

// Renders paths to the main render target.
class RenderContextMetalImpl::DrawPipeline
{
public:
  DrawPipeline(
      id<MTLDevice> gpu,
      const DrawShader& shader,
      MTLPixelFormat pixelFormat,
      bool colorWriteEnabled)
  {
    if (!shader.valid())
    {
      return;
    }

#ifdef WITH_RIVE_TOOLS
    if (shader.synthesizedFailureType() ==
        SynthesizedFailureType::pipelineCreation)
    {
      NSLog(@"RIVE: Synthesizing pipeline creation failure...");
      return;
    }
#endif

    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc]
        init];
    desc.vertexFunction = shader.vertexMain();
    desc.fragmentFunction = shader.fragmentMain();

    auto* framebuffer = desc.colorAttachments[COLOR_PLANE_IDX];
    framebuffer.pixelFormat = pixelFormat;

    switch (shader.interlockMode())
    {
      case gpu::InterlockMode::rasterOrdering:
        // In rasterOrdering mode, the PLS planes are accessed as
        // color attachments.
        desc.colorAttachments[CLIP_PLANE_IDX]
            .pixelFormat = MTLPixelFormatR32Uint;
        desc.colorAttachments[SCRATCH_COLOR_PLANE_IDX]
            .pixelFormat = pixelFormat;
        desc.colorAttachments[COVERAGE_PLANE_IDX]
            .pixelFormat = MTLPixelFormatR32Uint;
        break;

      case gpu::InterlockMode::atomics:
        // In atomic mode, the PLS planes are accessed as device
        // buffers. We only use the "framebuffer" attachment
        // configured above.
        if (enums::is_flag_set(
                shader.shaderMiscFlags(),
                gpu::ShaderMiscFlags::fixedFunctionColorOutput))
        {
          // The shader expectes a "src-over" blend function in
          // order to to implement antialiasing and opacity.
          framebuffer.blendingEnabled = TRUE;
          framebuffer.sourceRGBBlendFactor = MTLBlendFactorOne;
          framebuffer
              .destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
          framebuffer.rgbBlendOperation = MTLBlendOperationAdd;
          framebuffer.sourceAlphaBlendFactor = MTLBlendFactorOne;
          framebuffer
              .destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
          framebuffer.alphaBlendOperation = MTLBlendOperationAdd;
          framebuffer.writeMask = MTLColorWriteMaskAll;
        }
        else if (shader.drawType() == gpu::DrawType::renderPassResolve)
        {
          // We're resolving from the offscreen color buffer to
          // the framebuffer attachment. Write out the final color
          // directly without any blend modes.
          framebuffer.blendingEnabled = FALSE;
          framebuffer.writeMask = MTLColorWriteMaskAll;
        }
        else
        {
          // This pipeline renders by storing to the offscreen
          // color buffer; disable writes to the framebuffer
          // attachment.
          framebuffer.blendingEnabled = FALSE;
          framebuffer.writeMask = MTLColorWriteMaskNone;
        }
        break;

      case gpu::InterlockMode::depthStencil:
        if (pixelFormat != MTLPixelFormatInvalid)
        {
          framebuffer.blendingEnabled = TRUE;
          framebuffer.sourceRGBBlendFactor = MTLBlendFactorOne;
          framebuffer
              .destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
          framebuffer.rgbBlendOperation = MTLBlendOperationAdd;
          framebuffer.sourceAlphaBlendFactor = MTLBlendFactorOne;
          framebuffer
              .destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
          framebuffer.alphaBlendOperation = MTLBlendOperationAdd;
          framebuffer.writeMask = colorWriteEnabled ? MTLColorWriteMaskAll
                                                    : MTLColorWriteMaskNone;
        }
        desc.depthAttachmentPixelFormat = kDepthStencilPixelFormat;
        desc.stencilAttachmentPixelFormat = kDepthStencilPixelFormat;
        break;

      case gpu::InterlockMode::clockwise:
      case gpu::InterlockMode::clockwiseAtomic:
        RIVE_UNREACHABLE();
    }
    m_pipelineState = make_pipeline_state(gpu, desc);
  }

  bool valid() const { return m_pipelineState != nil; }

  id<MTLRenderPipelineState> pipelineState() const
  {
    assert(valid());
    return m_pipelineState;
  }

private:
  id<MTLRenderPipelineState> m_pipelineState = nil;
};

#if defined(RIVE_IOS) || defined(RIVE_XROS) || defined(RIVE_APPLETVOS)
static bool is_apple_silicon(id<MTLDevice> gpu)
{
  if (@available(iOS 13, tvOS 13, visionOS 1, *))
  {
    return [gpu supportsFamily:MTLGPUFamilyApple4];
  }
  return false;
}
#endif

class BufferRingMetalImpl : public BufferRing
{
public:
  static std::unique_ptr<BufferRingMetalImpl> Make(
      id<MTLDevice> gpu, size_t capacityInBytes)
  {
    return capacityInBytes != 0
               ? std::make_unique<BufferRingMetalImpl>(gpu, capacityInBytes)
               : nullptr;
  }

  BufferRingMetalImpl(id<MTLDevice> gpu, size_t capacityInBytes)
      : BufferRing(capacityInBytes)
  {
    for (int i = 0; i < kBufferRingSize; ++i)
    {
      m_buffers[i] = [gpu newBufferWithLength:capacityInBytes
                                      options:MTLResourceStorageModeShared];
    }
  }

  id<MTLBuffer> submittedBuffer() const
  {
    return m_buffers[submittedBufferIdx()];
  }

protected:
  void* onMapBuffer(int bufferIdx, size_t mapSizeInBytes) override
  {
    return m_buffers[bufferIdx].contents;
  }

  void onUnmapAndSubmitBuffer(int bufferIdx, size_t mapSizeInBytes) override {}

private:
  id<MTLBuffer> m_buffers[kBufferRingSize];
};

std::unique_ptr<RenderContext> RenderContextMetalImpl::MakeContext(
    id<MTLDevice> gpu, const ContextOptions& contextOptions)
{
  auto renderContextImpl = std::unique_ptr<RenderContextMetalImpl>(
      new RenderContextMetalImpl(gpu, contextOptions));
  return std::make_unique<RenderContext>(std::move(renderContextImpl));
}

RenderContextMetalImpl::RenderContextMetalImpl(
    id<MTLDevice> gpu, const ContextOptions& contextOptions)
    : m_contextOptions(contextOptions)
    , m_gpu(gpu)
{
  // It appears, so far, that we don't need to use flat interpolation for path
  // IDs on any Apple device, and it's faster not to.
  m_platformFeatures.avoidFlatVaryings = true;
  m_platformFeatures.clipSpaceBottomUp = true;
  m_platformFeatures.framebufferBottomUp = false;
  if ([m_gpu supportsFamily:MTLGPUFamilyApple2] ||
      [m_gpu supportsFamily:MTLGPUFamilyMac2])
  {
    m_platformFeatures.maxTextureSize = 16384;
  }
  else
  {
    m_platformFeatures.maxTextureSize = 8192;
  }
#if defined(RIVE_IOS) || defined(RIVE_XROS) || defined(RIVE_APPLETVOS)
  m_metalFeatures.supportsProgrammableBlending = true;
  m_platformFeatures.supportsAtomicMode = false;
  if (!is_apple_silicon(m_gpu))
  {
    // The PowerVR GPU, at least on A10, has fp16 precision issues. We can't
    // use the the bottom 3 bits of the path and clip IDs in order for our
    // equality testing to work.
    m_platformFeatures.pathIDGranularity = 8;
  }
#elif defined(RIVE_IOS_SIMULATOR) || defined(RIVE_XROS_SIMULATOR) ||           \
    defined(RIVE_APPLETVOS_SIMULATOR)
  // The simulator does not support framebuffer reads. Fall back on atomic
  // mode.
  m_metalFeatures.supportsProgrammableBlending = false;
  m_platformFeatures.supportsAtomicMode = true;
#else
  m_metalFeatures
      .supportsProgrammableBlending = [m_gpu
                                          supportsFamily:MTLGPUFamilyApple1] &&
                                      !contextOptions.disableFramebufferReads;
  m_platformFeatures.supportsAtomicMode = true;
#endif
  // Programmable blending is exactly what rasterOrdering mode needs.
  m_platformFeatures
      .supportsRasterOrderingMode = m_metalFeatures
                                        .supportsProgrammableBlending;
  m_platformFeatures.atomicPLSInitNeedsDraw = true;

  m_platformFeatures.supportsClipScissor = true;

  m_platformFeatures.supportsPipelineDynamicState = true;

  // Texture compression support varies by Apple platform family.
#if defined(RIVE_IOS) || defined(RIVE_XROS) || defined(RIVE_APPLETVOS) ||      \
    defined(RIVE_IOS_SIMULATOR) || defined(RIVE_XROS_SIMULATOR) ||             \
    defined(RIVE_APPLETVOS_SIMULATOR)
  // iOS/tvOS/visionOS: ETC2 and ASTC are always supported.
  m_platformFeatures.supportsTextureCompressionETC2 = true;
  m_platformFeatures.supportsTextureCompressionASTC = true;
#else
  // macOS: BC is always supported; ASTC only on Apple Silicon.
  m_platformFeatures.supportsTextureCompressionBC = true;
  m_platformFeatures.supportsTextureCompressionASTC = [m_gpu
      supportsFamily:MTLGPUFamilyApple1];
#endif

#if defined(RIVE_IOS) || defined(RIVE_XROS) || defined(RIVE_XROS_SIMULATOR) || \
    defined(RIVE_APPLETVOS) || defined(RIVE_APPLETVOS_SIMULATOR)
  // iOS hardware always supports raster order groups.
  m_metalFeatures.pipelineBarrierType = PipelineBarrierType::rasterOrderGroup;
#elif defined(RIVE_IOS_SIMULATOR)
  const NXArchInfo* hostArchitecture = NXGetLocalArchInfo();
  if (strncmp(hostArchitecture->name, "arm64", 5) == 0)
  {
    // The simulator doesn't advertise support for raster order groups, but
    // they appear to work anyway on an Apple-Silicon-hosted simulator. Use
    // rasterOrderGroup in this case because it's much faster than
    // renderPassBreak. (On Intel/AMD this doesn't matter anyway because
    // renderPassBreaks are cheap and actually faster than
    // rasterOrderGroups.)
    m_metalFeatures.pipelineBarrierType = PipelineBarrierType::rasterOrderGroup;
  }
  else
  {
    m_metalFeatures.pipelineBarrierType = PipelineBarrierType::renderPassBreak;
  }
#else
  // Use real memory barriers if they're availabile.
  // "GPU devices in Apple3 through Apple9 families don’t support memory
  // barriers that include the MTLRenderStages.fragment or .tile stages in the
  // after argument..."
  if (([m_gpu supportsFamily:MTLGPUFamilyCommon2] ||
       [m_gpu supportsFamily:MTLGPUFamilyMac2]) &&
      ![m_gpu supportsFamily:MTLGPUFamilyApple3])
  {
    m_metalFeatures.pipelineBarrierType = PipelineBarrierType::memoryBarrier;
  }
  else if (m_gpu.rasterOrderGroupsSupported)
  {
    m_metalFeatures.pipelineBarrierType = PipelineBarrierType::rasterOrderGroup;
  }
  else
  {
    m_metalFeatures.pipelineBarrierType = PipelineBarrierType::renderPassBreak;
  }
#endif

  m_platformFeatures
      .supportsRasterOrderDstReads = !m_metalFeatures
                                          .depthStencilReadsDstFromTexture();

  for (int i = 0; i < rive::ImageSampler::MAX_SAMPLER_PERMUTATIONS; ++i)
  {
    auto wrapX = ImageSampler::GetWrapXOptionFromKey(i);
    auto wrapY = ImageSampler::GetWrapYOptionFromKey(i);
    auto filter = ImageSampler::GetFilterOptionFromKey(i);

    MTLSamplerDescriptor* samplerDescriptor = [MTLSamplerDescriptor new];
    samplerDescriptor.minFilter = min_mag_filter_for_image_filter(filter);
    samplerDescriptor.magFilter = min_mag_filter_for_image_filter(filter);
    samplerDescriptor.mipFilter = mip_filter_for_image_filter(filter);
    samplerDescriptor.sAddressMode = address_mode_for_image_wrap(wrapX);
    samplerDescriptor.tAddressMode = address_mode_for_image_wrap(wrapY);

    m_imageSamplers[i] = [gpu newSamplerStateWithDescriptor:samplerDescriptor];
  }

  m_backgroundShaderCompiler = std::make_unique<BackgroundShaderCompiler>(
      m_gpu, m_metalFeatures);

  // Load the precompiled shaders.
  dispatch_data_t metallibData = dispatch_data_create(
#if defined(RIVE_IOS)
      rive_pls_ios_metallib,
      rive_pls_ios_metallib_len,
#elif defined(RIVE_IOS_SIMULATOR)
      rive_pls_ios_simulator_metallib,
      rive_pls_ios_simulator_metallib_len,
#elif defined(RIVE_XROS)
      rive_renderer_xros_metallib,
      rive_renderer_xros_metallib_len,
#elif defined(RIVE_XROS_SIMULATOR)
      rive_renderer_xros_simulator_metallib,
      rive_renderer_xros_simulator_metallib_len,
#elif defined(RIVE_APPLETVOS)
      rive_renderer_appletvos_metallib,
      rive_renderer_appletvos_metallib_len,
#elif defined(RIVE_APPLETVOS_SIMULATOR)
      rive_renderer_appletvsimulator_metallib,
      rive_renderer_appletvsimulator_metallib_len,
#else
      rive_pls_macosx_metallib,
      rive_pls_macosx_metallib_len,
#endif
      nil,
      nil);
  NSError* err = nil;
  m_plsPrecompiledLibrary = [m_gpu newLibraryWithData:metallibData error:&err];
  if (err != nil || m_plsPrecompiledLibrary == nil)
  {
    NSLog(
        @"RIVE: Failed to load pls metallib error: %@",
        err != nil ? err.localizedDescription : @"<nil>");
    return;
  }

  m_colorRampPipeline = std::make_unique<ColorRampPipeline>(
      m_gpu, m_plsPrecompiledLibrary);

  MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
  desc.pixelFormat = MTLPixelFormatR16Float;
  desc.textureType = MTLTextureType1DArray;
  desc.width = gpu::GAUSSIAN_TABLE_SIZE;
  desc.mipmapLevelCount = 1;
  desc.arrayLength = GAUSSIAN_INTEGRAL_TEXTURE_1D_ARRAY_LENGTH;
  desc.usage = MTLTextureUsageShaderRead;
  m_gaussianIntegralTexture = [m_gpu newTextureWithDescriptor:desc];
  [m_gaussianIntegralTexture
      replaceRegion:MTLRegionMake2D(0, 0, gpu::GAUSSIAN_TABLE_SIZE, 1)
        mipmapLevel:0
              slice:FEATHER_FUNCTION_ARRAY_INDEX
          withBytes:gpu::g_gaussianIntegralTableF16
        bytesPerRow:sizeof(gpu::g_gaussianIntegralTableF16)
      bytesPerImage:sizeof(gpu::g_gaussianIntegralTableF16)];
  [m_gaussianIntegralTexture
      replaceRegion:MTLRegionMake2D(0, 0, gpu::GAUSSIAN_TABLE_SIZE, 1)
        mipmapLevel:0
              slice:FEATHER_INVERSE_FUNCTION_ARRAY_INDEX
          withBytes:gpu::g_inverseGaussianIntegralTableF16
        bytesPerRow:sizeof(gpu::g_gaussianIntegralTableF16)
      bytesPerImage:sizeof(gpu::g_gaussianIntegralTableF16)];

  m_tessPipeline = std::make_unique<TessellatePipeline>(
      m_gpu, m_plsPrecompiledLibrary);
  m_tessSpanIndexBuffer = [m_gpu
      newBufferWithBytes:gpu::kTessSpanIndices
                  length:sizeof(gpu::kTessSpanIndices)
                 options:MTLResourceStorageModeShared];

  // The precompiled static library has a fully-featured shader for each
  // drawType in "rasterOrdering" mode. We load these at initialization and
  // use them while waiting for the background compiler to generate more
  // specialized, higher performance shaders.
  if (m_platformFeatures.supportsRasterOrderingMode)
  {
    for (auto drawType :
         {DrawType::midpointFanPatches,
          DrawType::interiorTriangulation,
          DrawType::featherAtlasBlit,
          DrawType::imageMesh})
    {
      for (auto shaderMiscFlags :
           {gpu::ShaderMiscFlags::none, gpu::ShaderMiscFlags::clockwiseFill})
      {
        if (drawType == gpu::DrawType::featherAtlasBlit &&
            shaderMiscFlags != gpu::ShaderMiscFlags::none)
        {
          continue;
        }
        gpu::ShaderFeatures allShaderFeatures = gpu::ShaderFeaturesMaskFor(
            drawType, gpu::InterlockMode::rasterOrdering);
        uint32_t shaderKey = ShaderUniqueKey(
            drawType,
            allShaderFeatures,
            gpu::InterlockMode::rasterOrdering,
            shaderMiscFlags);
        const DrawShader& shader = *(
            m_drawShaders[shaderKey] = std::make_unique<DrawShader>(
                m_plsPrecompiledLibrary,
                DrawShader::GetPrecompiledFunctionName(
                    drawType,
                    allShaderFeatures & gpu::kVertexShaderFeaturesMask,
                    gpu::ShaderMiscFlags::none,
                    m_plsPrecompiledLibrary,
                    GLSL_drawVertexMain),
                DrawShader::GetPrecompiledFunctionName(
                    drawType,
                    allShaderFeatures,
                    shaderMiscFlags,
                    m_plsPrecompiledLibrary,
                    GLSL_drawFragmentMain),
                drawType,
                gpu::InterlockMode::rasterOrdering,
                shaderMiscFlags
#ifdef WITH_RIVE_TOOLS
                ,
                SynthesizedFailureType::none
#endif
                ));
        for (MTLPixelFormat pixelFormat :
             {MTLPixelFormatRGBA8Unorm, MTLPixelFormatBGRA8Unorm})
        {
          findOrCreateDrawPipeline(shaderKey, shader, pixelFormat, true);
        }
      }
    }
  }

  // Create vertex and index buffers for the different PLS patches.
  m_pathPatchVertexBuffer = [m_gpu
      newBufferWithLength:kPatchVertexBufferCount * sizeof(PatchVertex)
                  options:MTLResourceStorageModeShared];
  m_pathPatchIndexBuffer = [m_gpu
      newBufferWithLength:kPatchIndexBufferCount * sizeof(uint16_t)
                  options:MTLResourceStorageModeShared];
  GeneratePatchBufferData(
      reinterpret_cast<PatchVertex*>(m_pathPatchVertexBuffer.contents),
      reinterpret_cast<uint16_t*>(m_pathPatchIndexBuffer.contents));

  // Set up the imageRect rendering buffers. (gpu::InterlockMode::atomics
  // only.)
  m_imageRectVertexBuffer = [m_gpu
      newBufferWithBytes:gpu::kImageRectVertices
                  length:sizeof(gpu::kImageRectVertices)
                 options:MTLResourceStorageModeShared];
  m_imageRectIndexBuffer = [m_gpu
      newBufferWithBytes:gpu::kImageRectIndices
                  length:sizeof(gpu::kImageRectIndices)
                 options:MTLResourceStorageModeShared];
}

RenderContextMetalImpl::~RenderContextMetalImpl() {}

// If the GPU supports framebuffer reads (called "programmable blending" in the
// feature tables), PLS planes besides the main framebuffer can exist in
// ephemeral "memoryless" storage. This means their contents are never actually
// written to main memory, and they only exist in fast tiled memory.
static id<MTLTexture> make_pls_memoryless_texture(
    id<MTLDevice> gpu,
    MTLPixelFormat pixelFormat,
    uint32_t width,
    uint32_t height)
{
  MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
  desc.pixelFormat = pixelFormat;
  desc.width = width;
  desc.height = height;
  desc.usage = MTLTextureUsageRenderTarget;
  desc.textureType = MTLTextureType2D;
  desc.mipmapLevelCount = 1;
  desc.storageMode = MTLStorageModeMemoryless;
  return [gpu newTextureWithDescriptor:desc];
}

RenderTargetMetal::RenderTargetMetal(
    id<MTLDevice> gpu,
    MTLPixelFormat pixelFormat,
    uint32_t width,
    uint32_t height,
    const PlatformFeatures& platformFeatures)
    : RenderTarget(width, height)
    , m_gpu(gpu)
    , m_pixelFormat(pixelFormat)
    , m_supportsMemorylessStorage(platformFeatures.supportsRasterOrderingMode)
{
  m_targetTexture = nil; // Will be configured later by setTargetTexture().
  if (platformFeatures.supportsRasterOrderingMode)
  {
    m_coverageMemorylessTexture = make_pls_memoryless_texture(
        gpu, MTLPixelFormatR32Uint, width, height);
    m_clipMemorylessTexture = make_pls_memoryless_texture(
        gpu, MTLPixelFormatR32Uint, width, height);
    m_scratchColorMemorylessTexture = make_pls_memoryless_texture(
        gpu, m_pixelFormat, width, height);
  }
}

void RenderTargetMetal::setTargetTexture(id<MTLTexture> texture)
{
  assert(!texture || compatibleWith(texture));
  m_targetTexture = texture;
}

id<MTLTexture> RenderTargetMetal::dstColorTexture()
{
  if (m_dstColorTexture == nil)
  {
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = m_pixelFormat;
    desc.width = width();
    desc.height = height();
    desc.usage = MTLTextureUsageShaderRead;
    desc.textureType = MTLTextureType2D;
    desc.mipmapLevelCount = 1;
    desc.storageMode = MTLStorageModePrivate;
    m_dstColorTexture = [m_gpu newTextureWithDescriptor:desc];
  }
  return m_dstColorTexture;
}

id<MTLTexture> RenderTargetMetal::depthStencilTexture()
{
  if (m_depthStencilTexture == nil)
  {
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = kDepthStencilPixelFormat;
    desc.width = width();
    desc.height = height();
    desc.usage = MTLTextureUsageRenderTarget;
    desc.textureType = MTLTextureType2D;
    desc.mipmapLevelCount = 1;
    desc.storageMode = m_supportsMemorylessStorage ? MTLStorageModeMemoryless
                                                   : MTLStorageModePrivate;
    m_depthStencilTexture = [m_gpu newTextureWithDescriptor:desc];
  }
  return m_depthStencilTexture;
}

rcp<RenderTargetMetal> RenderContextMetalImpl::makeRenderTarget(
    MTLPixelFormat pixelFormat, uint32_t width, uint32_t height)
{
  return rcp(new RenderTargetMetal(
      m_gpu, pixelFormat, width, height, m_platformFeatures));
}

class RenderBufferMetalImpl
    : public LITE_RTTI_OVERRIDE(RiveRenderBuffer, RenderBufferMetalImpl)
{
public:
  RenderBufferMetalImpl(
      RenderBufferType renderBufferType,
      RenderBufferFlags renderBufferFlags,
      size_t sizeInBytes,
      id<MTLDevice> gpu)
      : lite_rtti_override(renderBufferType, renderBufferFlags, sizeInBytes)
      , m_gpu(gpu)
  {
    int bufferCount = enums::is_flag_set(
                          flags(),
                          RenderBufferFlags::mappedOnceAtInitialization)
                          ? 1
                          : gpu::kBufferRingSize;
    for (int i = 0; i < bufferCount; ++i)
    {
      m_buffers[i] = [gpu newBufferWithLength:sizeInBytes
                                      options:MTLResourceStorageModeShared];
    }
  }

  id<MTLBuffer> submittedBuffer() { return m_buffers[frontBufferIdx()]; }

protected:
  void* onMap() override
  {
    assert(m_buffers[backBufferIdx()] != nil);
    return m_buffers[backBufferIdx()].contents;
  }

  void onUnmap() override {}

private:
  id<MTLDevice> m_gpu;
  id<MTLBuffer> m_buffers[gpu::kBufferRingSize];
  int m_submittedBufferIdx = -1;
};

rcp<RenderBuffer> RenderContextMetalImpl::makeRenderBuffer(
    RenderBufferType type, RenderBufferFlags flags, size_t sizeInBytes)
{
  return make_rcp<RenderBufferMetalImpl>(type, flags, sizeInBytes, m_gpu);
}

class TextureMetalImpl : public Texture
{
public:
  TextureMetalImpl(
      id<MTLDevice> gpu,
      uint32_t width,
      uint32_t height,
      uint32_t mipLevelCount,
      const uint8_t imageData[],
      MTLPixelFormat pixelFormat = MTLPixelFormatRGBA8Unorm,
      uint8_t blockWidth = 1,
      uint8_t blockHeight = 1,
      uint32_t bytesPerBlock = 4,
      bool generateRemainingMips = false)
      : Texture(width, height)
      , m_mipsDirty(generateRemainingMips && mipLevelCount > 1)
  {
    // Create the texture.
    MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
    desc.pixelFormat = pixelFormat;
    desc.width = width;
    desc.height = height;
    desc.mipmapLevelCount = mipLevelCount;
    desc.usage = MTLTextureUsageShaderRead;
    desc.textureType = MTLTextureType2D;
    m_texture = [gpu newTextureWithDescriptor:desc];

    // Upload mip 0 only when the caller asks for auto-mipgen
    // (generateRemainingMips=true). Otherwise upload every level the
    // texture was created with from the caller-supplied tight blob.
    const uint32_t levelsToUpload = generateRemainingMips ? 1u : mipLevelCount;
    const uint8_t* src = imageData;
    for (uint32_t i = 0; i < levelsToUpload; ++i)
    {
      const uint32_t logW = std::max<uint32_t>(1u, width >> i);
      const uint32_t logH = std::max<uint32_t>(1u, height >> i);
      const uint32_t blocksX = (logW + blockWidth - 1) / blockWidth;
      const uint32_t blocksY = (logH + blockHeight - 1) / blockHeight;
      const NSUInteger bytesPerRow = static_cast<NSUInteger>(blocksX) *
                                     bytesPerBlock;
      const size_t levelBytes = static_cast<size_t>(bytesPerRow) * blocksY;
      MTLRegion region = MTLRegionMake2D(0, 0, logW, logH);
      [m_texture replaceRegion:region
                   mipmapLevel:i
                     withBytes:src
                   bytesPerRow:bytesPerRow];
      src += levelBytes;
    }
  }

  void ensureMipmaps(id<MTLCommandBuffer> commandBuffer) const
  {
    if (m_mipsDirty)
    {
      // Generate mipmaps.
      id<MTLBlitCommandEncoder> mipEncoder = [commandBuffer blitCommandEncoder];
      [mipEncoder generateMipmapsForTexture:m_texture];
      [mipEncoder endEncoding];
      m_mipsDirty = false;
    }
  }

  // Adopt a pre-created MTLTexture (for RenderCanvas).
  TextureMetalImpl(id<MTLTexture> texture, uint32_t width, uint32_t height)
      : Texture(width, height)
      , m_texture(texture)
      , m_mipsDirty(false)
  {}

  id<MTLTexture> texture() const { return m_texture; }
  void* nativeHandle() const override { return (__bridge void*)m_texture; }

private:
  id<MTLTexture> m_texture;
  mutable bool m_mipsDirty = true;
};

rcp<Texture> RenderContextMetalImpl::makeImageTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevelCount,
    GPUTextureFormat format,
    const uint8_t imageData[],
    uint8_t blockWidth,
    uint8_t blockHeight,
    [[maybe_unused]] bool srgb,
    bool generateRemainingMips)
{
  MTLPixelFormat pixelFormat = MTLPixelFormatRGBA8Unorm;
  uint32_t bytesPerBlock = 4;
  bool isCompressed = false;

  switch (format)
  {
    case GPUTextureFormat::rgba32:
      assert(blockWidth == 1 && blockHeight == 1);
      break;
#if !TARGET_OS_IPHONE
    case GPUTextureFormat::bc7:
      pixelFormat = MTLPixelFormatBC7_RGBAUnorm;
      bytesPerBlock = 16;
      isCompressed = true;
      break;
#endif
    case GPUTextureFormat::astc:
    {
      // MTLPixelFormat ASTC LDR enums are sequential in Vulkan/GL
      // footprint order, starting at MTLPixelFormatASTC_4x4_LDR.
      const int idx = rive::astcFootprintIndex(blockWidth, blockHeight);
      if (idx < 0)
      {
        assert(!"unsupported ASTC block footprint");
        return nullptr;
      }
      pixelFormat = static_cast<MTLPixelFormat>(
          MTLPixelFormatASTC_4x4_LDR + idx);
      bytesPerBlock = 16;
      isCompressed = true;
      break;
    }
    case GPUTextureFormat::etc2:
      // ETC2 RGBA8: 8 bytes EAC alpha + 8 bytes ETC2 RGB = 16/block.
      pixelFormat = MTLPixelFormatEAC_RGBA8;
      bytesPerBlock = 16;
      isCompressed = true;
      break;
    default:
      assert(!"unsupported format");
      return nullptr;
  }
  assert(
      !(generateRemainingMips && isCompressed) &&
      "generateMipmapsForTexture is undefined on compressed formats");

  return make_rcp<TextureMetalImpl>(
      m_gpu,
      width,
      height,
      mipLevelCount,
      imageData,
      pixelFormat,
      blockWidth,
      blockHeight,
      bytesPerBlock,
      generateRemainingMips);
}

rcp<Texture> RenderContextMetalImpl::adoptImageTexture(
    id<MTLTexture> texture, uint32_t width, uint32_t height)
{
  if (texture == nil || width == 0 || height == 0)
  {
    return nullptr;
  }
  return make_rcp<TextureMetalImpl>(texture, width, height);
}

#ifdef RIVE_CANVAS
void RenderContextMetalImpl::ensureCanvasBacking(gpu::RenderCanvas* canvas)
{
  if (canvas->isBacked())
  {
    return;
  }

  uint32_t width = canvas->width(), height = canvas->height();

  // An MTLTexture usable as both a render target and a shader-read image
  // for compositing into Rive draws.
  MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
  desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
  desc.width = width;
  desc.height = height;
  desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  desc.textureType = MTLTextureType2D;
  desc.mipmapLevelCount = 1;
  desc.storageMode = MTLStorageModePrivate;
  id<MTLTexture> mtlTexture = [m_gpu newTextureWithDescriptor:desc];

  auto renderTarget = makeRenderTarget(MTLPixelFormatRGBA8Unorm, width, height);
  renderTarget->setTargetTexture(mtlTexture);

  // The TextureMetalImpl adopt constructor takes a pre-created MTLTexture
  // without uploading data.
  canvas->setBacking(
      make_rcp<TextureMetalImpl>(mtlTexture, width, height),
      std::move(renderTarget));
}

std::unique_ptr<rive::ore::Context> RenderContextMetalImpl::makeOreContext()
{
  // A deferred session can request the ore context before the first render
  // texture lazily sets the command queue, so mint one here.
  if (m_commandQueue == nil)
  {
    m_commandQueue = [m_gpu newCommandQueue];
  }
  return rive::ore::ContextMetal::Make(m_gpu, m_commandQueue);
}
#endif

std::unique_ptr<BufferRing> RenderContextMetalImpl::makeUniformBufferRing(
    size_t capacityInBytes)
{
  return BufferRingMetalImpl::Make(m_gpu, capacityInBytes);
}

std::unique_ptr<BufferRing> RenderContextMetalImpl::makeStorageBufferRing(
    size_t capacityInBytes, gpu::StorageBufferStructure)
{
  return BufferRingMetalImpl::Make(m_gpu, capacityInBytes);
}

std::unique_ptr<BufferRing> RenderContextMetalImpl::makeVertexBufferRing(
    size_t capacityInBytes)
{
  return BufferRingMetalImpl::Make(m_gpu, capacityInBytes);
}

void RenderContextMetalImpl::resizeGradientTexture(
    uint32_t width, uint32_t height)
{
  if (width == 0 || height == 0)
  {
    m_gradientTexture = nil;
    return;
  }
  MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
  desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
  desc.width = width;
  desc.height = height;
  desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  desc.textureType = MTLTextureType2D;
  desc.mipmapLevelCount = 1;
  desc.storageMode = MTLStorageModePrivate;
  m_gradientTexture = [m_gpu newTextureWithDescriptor:desc];
}

void RenderContextMetalImpl::resizeTessellationTexture(
    uint32_t width, uint32_t height)
{
  if (width == 0 || height == 0)
  {
    m_tessVertexTexture = nil;
    return;
  }
  MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
  desc.pixelFormat = MTLPixelFormatRGBA32Uint;
  desc.width = width;
  desc.height = height;
  desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  desc.textureType = MTLTextureType2D;
  desc.mipmapLevelCount = 1;
  desc.storageMode = MTLStorageModePrivate;
  m_tessVertexTexture = [m_gpu newTextureWithDescriptor:desc];
}

void RenderContextMetalImpl::resizeFeatherAtlasTexture(
    uint32_t width, uint32_t height)
{
  if (width == 0 || height == 0)
  {
    m_featherAtlasTexture = nil;
    return;
  }

  MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
  desc.pixelFormat = MTLPixelFormatR16Float;
  desc.width = width;
  desc.height = height;
  desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  desc.textureType = MTLTextureType2D;
  desc.mipmapLevelCount = 1;
  desc.storageMode = MTLStorageModePrivate;
  m_featherAtlasTexture = [m_gpu newTextureWithDescriptor:desc];

  // Don't build atlas pipelines until we get an indication that they will be
  // used.
  assert(
      (m_featherAtlasFillPipeline == nil) ==
      (m_featherAtlasStrokePipeline == nil));
  if (m_featherAtlasFillPipeline == nil)
  {
    m_featherAtlasFillPipeline = std::make_unique<FeatherAtlasPipeline>(
        m_gpu,
        m_plsPrecompiledLibrary,
        @GLSL_atlasFillFragmentMain,
        MTLBlendOperationAdd);
    m_featherAtlasStrokePipeline = std::make_unique<FeatherAtlasPipeline>(
        m_gpu,
        m_plsPrecompiledLibrary,
        @GLSL_atlasStrokeFragmentMain,
        MTLBlendOperationMax);
  }
}

const RenderContextMetalImpl::DrawPipeline* RenderContextMetalImpl::
    findCompatibleDrawPipeline(
        gpu::DrawType drawType,
        gpu::ShaderFeatures shaderFeatures,
        const gpu::FlushDescriptor& desc,
        gpu::ShaderMiscFlags shaderMiscFlags,
        MTLPixelFormat pixelFormat,
        bool colorWriteEnabled)
{
  // Find a fully-featured superset of features whose pipeline we can fall
  // back on while waiting for it to compile.
  ShaderFeatures fullyFeaturedPipelineFeatures = gpu::UbershaderFeaturesMaskFor(
      shaderFeatures,
      drawType,
      desc.interlockMode,
      shaderMiscFlags,
      m_platformFeatures);

  if (m_contextOptions.shaderCompilationMode ==
      ShaderCompilationMode::onlyUbershaders)
  {
    // Force the shader features to be the full set if that's what was
    // requested.
    shaderFeatures = fullyFeaturedPipelineFeatures;
  }

#ifdef WITH_RIVE_TOOLS
  if (desc.synthesizedFailureType == SynthesizedFailureType::ubershaderLoad)
  {
    // Pretend that the requested shader is not ready yet and the ubershader
    // compilation failed
    return nil;
  }
#endif

  uint32_t shaderKey = gpu::ShaderUniqueKey(
      drawType, shaderFeatures, desc.interlockMode, shaderMiscFlags);
  auto shaderIter = m_drawShaders.find(shaderKey);
  if (shaderIter == m_drawShaders.end())
  {
    // The shader hasn't been scheduled for compiling yet. Schedule it to
    // compile in the background.
    m_backgroundShaderCompiler->pushJob({
        .drawType = drawType,
        .shaderFeatures = shaderFeatures,
        .interlockMode = desc.interlockMode,
        .shaderMiscFlags = shaderMiscFlags,
#ifdef WITH_RIVE_TOOLS
        .synthesizedFailureType = desc.synthesizedFailureType,
#endif
    });
    shaderIter = m_drawShaders.insert({shaderKey, nullptr}).first;
  }

  if (shaderIter->second == nullptr)
  {
    // The shader hasn't finished compiling yet. Fully-featured
    // "rasterOrdering" shaders should have already been pre-loaded from the
    // static library.
    assert(
        shaderFeatures != fullyFeaturedPipelineFeatures ||
        desc.interlockMode != gpu::InterlockMode::rasterOrdering);

    // Poll to see if the shader is actually done compiling, but only wait
    // if it's a fully-feature shader. Otherwise, we can fall back on the
    // fully-featured shader while we wait for compilation.
    BackgroundCompileJob job;
    bool shouldWaitForBackgroundCompilation =
        shaderFeatures == fullyFeaturedPipelineFeatures ||
        m_contextOptions.shaderCompilationMode !=
            ShaderCompilationMode::allowAsynchronous;
    while (m_backgroundShaderCompiler->popFinishedJob(
        &job, shouldWaitForBackgroundCompilation))
    {
      uint32_t jobKey = gpu::ShaderUniqueKey(
          job.drawType,
          job.shaderFeatures,
          job.interlockMode,
          job.shaderMiscFlags);
      // clipReset draws stencil_draw.glsl, whose entry points have
      // their own names.
      const bool isStencilDraw = job.drawType == DrawType::clipReset;
      m_drawShaders[jobKey] = std::make_unique<DrawShader>(
          job.compiledLibrary,
          isStencilDraw ? @GLSL_stencilVertexMain : @GLSL_drawVertexMain,
          isStencilDraw ? @GLSL_stencilFragmentMain : @GLSL_drawFragmentMain,
          job.drawType,
          job.interlockMode,
          job.shaderMiscFlags
#ifdef WITH_RIVE_TOOLS
          ,
          desc.synthesizedFailureType
#endif
      );
      if (jobKey == shaderKey)
      {
        // The shader we wanted was actually done compiling.
        break;
      }
    }
  }

  const DrawPipeline* pipeline = nullptr;
  if (shaderIter->second != nullptr)
  {
    pipeline = findOrCreateDrawPipeline(
        shaderKey, *shaderIter->second, pixelFormat, colorWriteEnabled);
  }

  if ((pipeline == nullptr || !pipeline->valid()) &&
      shaderFeatures != fullyFeaturedPipelineFeatures)
  {
    // The shader for this feature set hasn't finished compiling (or it
    // failed to compile). Use the uber-shader pipeline that has all
    // features enabled while we wait for it to finish.
    return findCompatibleDrawPipeline(
        drawType,
        fullyFeaturedPipelineFeatures,
        desc,
        shaderMiscFlags,
        pixelFormat,
        colorWriteEnabled);
  }

  return pipeline;
}

const RenderContextMetalImpl::DrawPipeline* RenderContextMetalImpl::
    findOrCreateDrawPipeline(
        uint32_t shaderKey,
        const DrawShader& shader,
        MTLPixelFormat pixelFormat,
        bool colorWriteEnabled)
{
  if (shader.interlockMode() == gpu::InterlockMode::depthStencil &&
      m_metalFeatures.depthStencilRendersColorToBuffer(enums::is_flag_set(
          shader.shaderMiscFlags(),
          gpu::ShaderMiscFlags::fixedFunctionColorOutput)))
  {
    pixelFormat = MTLPixelFormatInvalid;
    colorWriteEnabled = true;
  }
  assert(
      colorWriteEnabled ||
      shader.interlockMode() == gpu::InterlockMode::depthStencil);

  uint64_t key = shaderKey;
  key = math::add_bits_to_key(key, uint32_t(pixelFormat), 16);
  key = math::add_bits_to_key(key, colorWriteEnabled, 1);
  std::unique_ptr<DrawPipeline>& pipeline = m_drawPipelines[key];
  if (pipeline == nullptr)
  {
    pipeline = std::make_unique<DrawPipeline>(
        m_gpu, shader, pixelFormat, colorWriteEnabled);
  }
  return pipeline.get();
}

// Packs a face's stencil ops into a depth/stencil state key.
static uint64_t addStencilFaceOpsToKey(
    uint64_t key, const gpu::StencilFaceOps& ops)
{
  constexpr static uint32_t kStencilOpBitCount = 3;
  constexpr static uint32_t kStencilCompareOpBitCount = 3;
  key = math::add_bits_to_key(
      key, uint32_t(ops.stencilFailOp), kStencilOpBitCount);
  key = math::add_bits_to_key(
      key, uint32_t(ops.depthFailOp), kStencilOpBitCount);
  key = math::add_bits_to_key(
      key, uint32_t(ops.depthStencilPassOp), kStencilOpBitCount);
  key = math::add_bits_to_key(
      key, uint32_t(ops.compareOp), kStencilCompareOpBitCount);
  return key;
}

id<MTLDepthStencilState> RenderContextMetalImpl::findOrCreateDepthStencilState(
    const gpu::PipelineState& pipelineState)
{
  // The stencil reference is encoder state, so it stays out of the key.
  const gpu::StencilFaceOps& backOps = pipelineState.stencilDoubleSided
                                           ? pipelineState.stencilBackOps
                                           : pipelineState.stencilFrontOps;
  uint64_t key = 0;
  key = math::add_bits_to_key(key, pipelineState.depthTestEnabled, 1);
  key = math::add_bits_to_key(key, pipelineState.depthWriteEnabled, 1);
  key = math::add_bits_to_key(key, pipelineState.stencilTestEnabled, 1);
  key = math::add_bits_to_key(key, pipelineState.stencilCompareMask, 8);
  key = math::add_bits_to_key(key, pipelineState.stencilWriteMask, 8);
  key = addStencilFaceOpsToKey(key, pipelineState.stencilFrontOps);
  key = addStencilFaceOpsToKey(key, backOps);

  auto iter = m_depthStencilStates.find(key);
  if (iter != m_depthStencilStates.end())
  {
    return iter->second;
  }

  MTLDepthStencilDescriptor* desc = [[MTLDepthStencilDescriptor alloc] init];
  desc.depthCompareFunction = pipelineState.depthTestEnabled
                                  ? MTLCompareFunctionGreater
                                  : MTLCompareFunctionAlways;
  desc.depthWriteEnabled = pipelineState.depthWriteEnabled;
  if (pipelineState.stencilTestEnabled)
  {
    desc.frontFaceStencil = makeStencilDescriptor(
        pipelineState.stencilFrontOps,
        pipelineState.stencilCompareMask,
        pipelineState.stencilWriteMask);
    desc.backFaceStencil = makeStencilDescriptor(
        backOps,
        pipelineState.stencilCompareMask,
        pipelineState.stencilWriteMask);
  }
  id<MTLDepthStencilState> state = [m_gpu
      newDepthStencilStateWithDescriptor:desc];
  m_depthStencilStates[key] = state;
  return state;
}

void* RenderContextMetalImpl::makeCommandBuffer()
{
  if (m_commandQueue == nil)
  {
    return nullptr;
  }
  // __bridge_retained: transfers ARC ownership to the void* so it stays alive
  // until commitCommandBuffer() releases it.
  return (__bridge_retained void*)[m_commandQueue commandBuffer];
}

void RenderContextMetalImpl::commitCommandBuffer(void* commandBuffer)
{
  if (commandBuffer == nullptr)
  {
    return;
  }
  // __bridge_transfer: reclaims ARC ownership, balancing the
  // __bridge_retained in makeCommandBuffer().
  id<MTLCommandBuffer> mtlCmdBuffer = (__bridge_transfer id<MTLCommandBuffer>)
      commandBuffer;
  [mtlCmdBuffer commit];
}

// The buffer-ring lock acquired here is released asymmetrically: postFlush()
// schedules a GPU completion handler that unlocks it once rendering finishes.
// Clang's -Wthread-safety analysis cannot follow a lock handed off into that
// block, so this method is exempt from the analysis.
void RenderContextMetalImpl::prepareToFlush(
    uint64_t nextFrameNumber,
    uint64_t safeFrameNumber) RIVE_NO_THREAD_SAFETY_ANALYSIS
{
  // Wait until the GPU finishes rendering flush "N + 1 - kBufferRingSize".
  // This ensures it is safe for the CPU to begin modifying the next buffers
  // in our rings.
  m_bufferRingIdx = (m_bufferRingIdx + 1) % kBufferRingSize;
  m_bufferRingLocks[m_bufferRingIdx].lock();
}

static id<MTLBuffer> mtl_buffer(const BufferRing* bufferRing)
{
  assert(bufferRing != nullptr);
  return static_cast<const BufferRingMetalImpl*>(bufferRing)->submittedBuffer();
}

static MTLViewport make_viewport(
    uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
  return {
      static_cast<double>(x),
      static_cast<double>(y),
      static_cast<double>(width),
      static_cast<double>(height),
      0,
      1,
  };
}

static MTLScissorRect make_scissor(const AABBu16& scissor)
{
  return {
      static_cast<NSUInteger>(scissor.left),
      static_cast<NSUInteger>(scissor.top),
      static_cast<NSUInteger>(scissor.width()),
      static_cast<NSUInteger>(scissor.height()),
  };
}

id<MTLRenderCommandEncoder> RenderContextMetalImpl::makeRenderPassForDraws(
    const gpu::FlushDescriptor& flushDesc,
    MTLRenderPassDescriptor* passDesc,
    id<MTLCommandBuffer> commandBuffer,
    gpu::ShaderMiscFlags baselineShaderMiscFlags)
{
  auto* renderTarget = static_cast<RenderTargetMetal*>(flushDesc.renderTarget);

  id<MTLRenderCommandEncoder> encoder = [commandBuffer
      renderCommandEncoderWithDescriptor:passDesc];

  [encoder
      setViewport:make_viewport(
                      0, 0, renderTarget->width(), renderTarget->height())];
  [encoder setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                    offset:flushDesc.flushUniformDataOffsetInBytes
                   atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
  [encoder setFragmentBuffer:mtl_buffer(flushUniformBufferRing())
                      offset:flushDesc.flushUniformDataOffsetInBytes
                     atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
  [encoder setVertexTexture:m_tessVertexTexture
                    atIndex:TESS_VERTEX_TEXTURE_IDX];
  [encoder setVertexTexture:m_gaussianIntegralTexture
                    atIndex:GAUSSIAN_INTEGRAL_TEXTURE_IDX];
  [encoder setFragmentTexture:m_gradientTexture atIndex:GRAD_TEXTURE_IDX];
  [encoder setFragmentTexture:m_gaussianIntegralTexture
                      atIndex:GAUSSIAN_INTEGRAL_TEXTURE_IDX];
  [encoder setFragmentTexture:m_featherAtlasTexture
                      atIndex:FEATHER_ATLAS_TEXTURE_IDX];
  if (flushDesc.pathCount > 0)
  {
    [encoder setVertexBuffer:mtl_buffer(pathBufferRing())
                      offset:flushDesc.firstPath * sizeof(gpu::PathData)
                     atIndex:METAL_BUFFER_IDX(PATH_BUFFER_IDX)];
    if (flushDesc.interlockMode == gpu::InterlockMode::atomics)
    {
      [encoder setFragmentBuffer:mtl_buffer(pathBufferRing())
                          offset:flushDesc.firstPath * sizeof(gpu::PathData)
                         atIndex:METAL_BUFFER_IDX(PATH_BUFFER_IDX)];
      [encoder
          setFragmentBuffer:mtl_buffer(paintAuxBufferRing())
                     offset:flushDesc.firstPaintAux * sizeof(gpu::PaintAuxData)
                    atIndex:METAL_BUFFER_IDX(PAINT_AUX_BUFFER_IDX)];
    }
    else
    {
      [encoder
          setVertexBuffer:mtl_buffer(paintAuxBufferRing())
                   offset:flushDesc.firstPaintAux * sizeof(gpu::PaintAuxData)
                  atIndex:METAL_BUFFER_IDX(PAINT_AUX_BUFFER_IDX)];
    }
  }
  if (flushDesc.contourCount > 0)
  {
    [encoder setVertexBuffer:mtl_buffer(contourBufferRing())
                      offset:flushDesc.firstContour * sizeof(gpu::ContourData)
                     atIndex:METAL_BUFFER_IDX(CONTOUR_BUFFER_IDX)];
  }
  if (flushDesc.interlockMode == gpu::InterlockMode::depthStencil &&
      m_metalFeatures.depthStencilRendersColorToBuffer(
          flushDesc.fixedFunctionColorOutput))
  {
    // Without framebuffer fetch, depthStencil draws blend in the shader
    // and write packed pixels to the color buffer, in the target's byte
    // order, instead of a color attachment.
    [encoder
        setFragmentBuffer:renderTarget->colorAtomicBuffer()
                   offset:0
                  atIndex:METAL_BUFFER_IDX(
                              COLOR_PLANE_IDX + DEFAULT_BINDINGS_SET_SIZE)];
    const uint32_t targetIsBGRA = renderTarget->pixelFormat() ==
                                      MTLPixelFormatBGRA8Unorm ||
                                  renderTarget->pixelFormat() ==
                                      MTLPixelFormatBGRA8Unorm_sRGB;
    [encoder
        setFragmentBytes:&targetIsBGRA
                  length:sizeof(targetIsBGRA)
                 atIndex:METAL_BUFFER_IDX(DRAW_ARGUMENTS_UNIFORM_BUFFER_IDX)];
  }
  if (flushDesc.interlockMode == gpu::InterlockMode::depthStencil &&
      m_metalFeatures.depthStencilReadsDstFromTexture() &&
      !flushDesc.fixedFunctionColorOutput)
  {
    [encoder setFragmentTexture:renderTarget->dstColorTexture()
                        atIndex:DST_COLOR_TEXTURE_IDX];
  }
  if (flushDesc.interlockMode == gpu::InterlockMode::atomics)
  {
    // In atomic mode, the PLS planes are buffers that we need to bind
    // separately. Since the PLS plane indices collide with other buffer
    // bindings, offset the binding indices of these buffers by
    // DEFAULT_BINDINGS_SET_SIZE.
    if (!enums::is_flag_set(
            baselineShaderMiscFlags,
            gpu::ShaderMiscFlags::fixedFunctionColorOutput))
    {
      [encoder
          setFragmentBuffer:renderTarget->colorAtomicBuffer()
                     offset:0
                    atIndex:METAL_BUFFER_IDX(
                                COLOR_PLANE_IDX + DEFAULT_BINDINGS_SET_SIZE)];
    }
    [encoder setFragmentBuffer:renderTarget->clipAtomicBuffer()
                        offset:0
                       atIndex:METAL_BUFFER_IDX(
                                   CLIP_PLANE_IDX + DEFAULT_BINDINGS_SET_SIZE)];
    [encoder
        setFragmentBuffer:renderTarget->coverageAtomicBuffer()
                   offset:0
                  atIndex:METAL_BUFFER_IDX(
                              COVERAGE_PLANE_IDX + DEFAULT_BINDINGS_SET_SIZE)];
  }
  return encoder;
}

void RenderContextMetalImpl::copyTargetToColorBuffer(
    id<MTLCommandBuffer> commandBuffer,
    RenderTargetMetal* renderTarget,
    const IAABB& bounds)
{
  auto origin = MTLOriginMake(bounds.left, bounds.top, 0);
  auto size = MTLSizeMake(bounds.width(), bounds.height(), 1);
  id<MTLBlitCommandEncoder> copyEncoder = [commandBuffer blitCommandEncoder];
  [copyEncoder copyFromTexture:renderTarget->targetTexture()
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:origin
                    sourceSize:size
                      toBuffer:renderTarget->colorAtomicBuffer()
             destinationOffset:(origin.y * renderTarget->width() + origin.x) *
                               sizeof(uint32_t)
        destinationBytesPerRow:renderTarget->width() * sizeof(uint32_t)
      destinationBytesPerImage:renderTarget->height() * renderTarget->width() *
                               sizeof(uint32_t)];
  [copyEncoder endEncoding];
}

void RenderContextMetalImpl::fillColorBuffer(
    id<MTLCommandBuffer> commandBuffer,
    RenderTargetMetal* renderTarget,
    const IAABB& bounds,
    uint8_t value)
{
  const NSUInteger rowBytes = renderTarget->width() * sizeof(uint32_t);
  id<MTLBlitCommandEncoder> fillEncoder = [commandBuffer blitCommandEncoder];
  [fillEncoder
      fillBuffer:renderTarget->colorAtomicBuffer()
           range:NSMakeRange(bounds.top * rowBytes, bounds.height() * rowBytes)
           value:value];
  [fillEncoder endEncoding];
}

void RenderContextMetalImpl::copyColorBufferToTarget(
    id<MTLCommandBuffer> commandBuffer,
    RenderTargetMetal* renderTarget,
    const IAABB& bounds)
{
  auto origin = MTLOriginMake(bounds.left, bounds.top, 0);
  auto size = MTLSizeMake(bounds.width(), bounds.height(), 1);
  id<MTLBlitCommandEncoder> copyEncoder = [commandBuffer blitCommandEncoder];
  [copyEncoder copyFromBuffer:renderTarget->colorAtomicBuffer()
                 sourceOffset:(origin.y * renderTarget->width() + origin.x) *
                              sizeof(uint32_t)
            sourceBytesPerRow:renderTarget->width() * sizeof(uint32_t)
          sourceBytesPerImage:renderTarget->height() * renderTarget->width() *
                              sizeof(uint32_t)
                   sourceSize:size
                    toTexture:renderTarget->targetTexture()
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:origin];
  [copyEncoder endEncoding];
}

id<MTLRenderCommandEncoder> RenderContextMetalImpl::copyDstReadsAndRestart(
    id<MTLRenderCommandEncoder> encoder,
    const gpu::DrawBatch& batch,
    const gpu::FlushDescriptor& desc,
    MTLRenderPassDescriptor* pass,
    id<MTLCommandBuffer> commandBuffer,
    gpu::ShaderMiscFlags baselineMiscFlags)
{
  [encoder endEncoding];

  auto* renderTarget = static_cast<RenderTargetMetal*>(desc.renderTarget);
  id<MTLBlitCommandEncoder> copyEncoder = [commandBuffer blitCommandEncoder];
  for (const Draw* draw = batch.dstReadList; draw != nullptr;
       draw = draw->nextDstRead())
  {
    IAABB bounds = desc.renderTargetUpdateBounds.intersect(draw->pixelBounds());
    if (bounds.empty())
    {
      continue;
    }
    [copyEncoder copyFromTexture:renderTarget->targetTexture()
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(bounds.left, bounds.top, 0)
                      sourceSize:MTLSizeMake(bounds.width(), bounds.height(), 1)
                       toTexture:renderTarget->dstColorTexture()
                destinationSlice:0
                destinationLevel:0
               destinationOrigin:MTLOriginMake(bounds.left, bounds.top, 0)];
  }
  [copyEncoder endEncoding];

  pass.colorAttachments[COLOR_PLANE_IDX].loadAction = MTLLoadActionLoad;
  pass.depthAttachment.loadAction = MTLLoadActionLoad;
  pass.stencilAttachment.loadAction = MTLLoadActionLoad;
  return makeRenderPassForDraws(desc, pass, commandBuffer, baselineMiscFlags);
}

id<MTLRenderCommandEncoder> RenderContextMetalImpl::issuePipelineBarrier(
    id<MTLRenderCommandEncoder> encoder,
    const gpu::FlushDescriptor& desc,
    MTLRenderPassDescriptor* pass,
    id<MTLCommandBuffer> commandBuffer,
    gpu::ShaderMiscFlags baselineMiscFlags)
{
  switch (m_metalFeatures.pipelineBarrierType)
  {
    case PipelineBarrierType::memoryBarrier:
#if defined(RIVE_MACOSX)
      if (@available(macOS 10.14, *))
      {
        [encoder memoryBarrierWithScope:MTLBarrierScopeBuffers |
                                        MTLBarrierScopeRenderTargets
                            afterStages:MTLRenderStageFragment
                           beforeStages:MTLRenderStageFragment];
        return encoder;
      }
#endif
      // pipelineBarrierType shouldn't be "memoryBarrier" in this case.
      RIVE_UNREACHABLE();

    case PipelineBarrierType::rasterOrderGroup:
      return encoder;

    case PipelineBarrierType::renderPassBreak:
      // On very old hardware that can't support barriers, we just take a
      // sledge hammer and break the entire render pass between overlapping
      // draws.
      // TODO: Is there a lighter way to achieve this?
      [encoder endEncoding];
      pass.colorAttachments[COLOR_PLANE_IDX].loadAction = MTLLoadActionLoad;
      return makeRenderPassForDraws(
          desc, pass, commandBuffer, baselineMiscFlags);
  }
  RIVE_UNREACHABLE();
}

void RenderContextMetalImpl::flush(const FlushDescriptor& desc)
{
  assert(desc.interlockMode != gpu::InterlockMode::clockwise);
  assert(desc.interlockMode != gpu::InterlockMode::clockwiseAtomic);

  auto* renderTarget = static_cast<RenderTargetMetal*>(desc.renderTarget);
  id<MTLCommandBuffer> commandBuffer = (__bridge id<MTLCommandBuffer>)
                                           desc.externalCommandBuffer;

  // Render the color ramps to the gradient texture.
  if (desc.gradSpanCount > 0)
  {
    // We failed to load the precompiled library and therefore do not have
    // the abililty to draw anything.
    if (!m_colorRampPipeline)
    {
      return;
    }
    // We are removing the abort in the case this doesn't build. So give up
    // drawing if we still don't have a pipeline here.
    auto pipelineState = m_colorRampPipeline->pipelineState();
    if (!pipelineState)
    {
      return;
    }
    MTLRenderPassDescriptor*
        gradPass = [MTLRenderPassDescriptor renderPassDescriptor];
    gradPass.renderTargetWidth = kGradTextureWidth;
    gradPass.renderTargetHeight = desc.gradDataHeight;
    gradPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    gradPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    gradPass.colorAttachments[0].texture = m_gradientTexture;

    id<MTLRenderCommandEncoder> gradEncoder = [commandBuffer
        renderCommandEncoderWithDescriptor:gradPass];
    [gradEncoder setViewport:make_viewport(
                                 0,
                                 0,
                                 kGradTextureWidth,
                                 static_cast<float>(desc.gradDataHeight))];
    [gradEncoder setRenderPipelineState:pipelineState];
    [gradEncoder setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                          offset:desc.flushUniformDataOffsetInBytes
                         atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
    [gradEncoder setVertexBuffer:mtl_buffer(gradSpanBufferRing())
                          offset:desc.firstGradSpan * sizeof(gpu::GradientSpan)
                         atIndex:0];
    [gradEncoder setCullMode:MTLCullModeBack];
    [gradEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                    vertexStart:0
                    vertexCount:gpu::GRAD_SPAN_TRI_STRIP_VERTEX_COUNT
                  instanceCount:desc.gradSpanCount];
    [gradEncoder endEncoding];
  }

  // Tessellate all curves into vertices in the tessellation texture.
  if (desc.tessVertexSpanCount > 0)
  {
    // We failed to load the precompiled library and therefore do not have
    // the abililty to draw anything.
    if (!m_tessPipeline)
    {
      return;
    }
    // We are removing the abort in the case this doesn't build. So give up
    // drawing if we still don't have a pipeline here.
    auto pipelineState = m_tessPipeline->pipelineState();
    if (!pipelineState)
    {
      return;
    }

    MTLRenderPassDescriptor*
        tessPass = [MTLRenderPassDescriptor renderPassDescriptor];
    tessPass.renderTargetWidth = kTessTextureWidth;
    tessPass.renderTargetHeight = desc.tessDataHeight;
    tessPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    tessPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    tessPass.colorAttachments[0].texture = m_tessVertexTexture;

    id<MTLRenderCommandEncoder> tessEncoder = [commandBuffer
        renderCommandEncoderWithDescriptor:tessPass];
    [tessEncoder setViewport:make_viewport(
                                 0, 0, kTessTextureWidth, desc.tessDataHeight)];
    [tessEncoder setRenderPipelineState:pipelineState];
    [tessEncoder setVertexTexture:m_gaussianIntegralTexture
                          atIndex:GAUSSIAN_INTEGRAL_TEXTURE_IDX];
    [tessEncoder setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                          offset:desc.flushUniformDataOffsetInBytes
                         atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
    [tessEncoder
        setVertexBuffer:mtl_buffer(tessSpanBufferRing())
                 offset:desc.firstTessVertexSpan * sizeof(gpu::TessVertexSpan)
                atIndex:0];
    assert(desc.pathCount > 0);
    [tessEncoder setVertexBuffer:mtl_buffer(pathBufferRing())
                          offset:desc.firstPath * sizeof(gpu::PathData)
                         atIndex:METAL_BUFFER_IDX(PATH_BUFFER_IDX)];
    assert(desc.contourCount > 0);
    [tessEncoder setVertexBuffer:mtl_buffer(contourBufferRing())
                          offset:desc.firstContour * sizeof(gpu::ContourData)
                         atIndex:METAL_BUFFER_IDX(CONTOUR_BUFFER_IDX)];
    [tessEncoder setCullMode:MTLCullModeBack];
    [tessEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:std::size(gpu::kTessSpanIndices)
                             indexType:MTLIndexTypeUInt16
                           indexBuffer:m_tessSpanIndexBuffer
                     indexBufferOffset:0
                         instanceCount:desc.tessVertexSpanCount];
    [tessEncoder endEncoding];
  }

  // Render the feather atlas if we have any offscreen feathers.
  if ((desc.featherAtlasFillBatchCount | desc.featherAtlasStrokeBatchCount) !=
      0)
  {
    // We failed to load the precompiled library and therefore do not have
    // the abililty to draw anything.
    if (!m_featherAtlasStrokePipeline || !m_featherAtlasFillPipeline)
    {
      return;
    }
    // We are removing the abort in the case this doesn't build. So give up
    // drawing if we still don't have a pipeline here.
    auto atlasFillPipelineState = m_featherAtlasFillPipeline->pipelineState();
    if (!atlasFillPipelineState)
    {
      return;
    }

    auto atlasStrokePipelineState = m_featherAtlasStrokePipeline
                                        ->pipelineState();
    if (!atlasStrokePipelineState)
    {
      return;
    }

    MTLRenderPassDescriptor*
        atlasPass = [MTLRenderPassDescriptor renderPassDescriptor];
    atlasPass.renderTargetWidth = desc.featherAtlasContentWidth;
    atlasPass.renderTargetHeight = desc.featherAtlasContentHeight;
    atlasPass.colorAttachments[0].loadAction = MTLLoadActionClear;
    atlasPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    atlasPass.colorAttachments[0].texture = m_featherAtlasTexture;
    atlasPass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);

    id<MTLRenderCommandEncoder> atlasEncoder = [commandBuffer
        renderCommandEncoderWithDescriptor:atlasPass];
    [atlasEncoder setViewport:make_viewport(
                                  0,
                                  0,
                                  desc.featherAtlasContentWidth,
                                  desc.featherAtlasContentHeight)];
    [atlasEncoder setVertexBuffer:mtl_buffer(flushUniformBufferRing())
                           offset:desc.flushUniformDataOffsetInBytes
                          atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
    [atlasEncoder setFragmentBuffer:mtl_buffer(flushUniformBufferRing())
                             offset:desc.flushUniformDataOffsetInBytes
                            atIndex:METAL_BUFFER_IDX(FLUSH_UNIFORM_BUFFER_IDX)];
    [atlasEncoder setVertexTexture:m_tessVertexTexture
                           atIndex:TESS_VERTEX_TEXTURE_IDX];
    [atlasEncoder setVertexTexture:m_gaussianIntegralTexture
                           atIndex:GAUSSIAN_INTEGRAL_TEXTURE_IDX];
    [atlasEncoder setFragmentTexture:m_gradientTexture
                             atIndex:GRAD_TEXTURE_IDX];
    [atlasEncoder setFragmentTexture:m_gaussianIntegralTexture
                             atIndex:GAUSSIAN_INTEGRAL_TEXTURE_IDX];
    if (desc.pathCount > 0)
    {
      [atlasEncoder setVertexBuffer:mtl_buffer(pathBufferRing())
                             offset:desc.firstPath * sizeof(gpu::PathData)
                            atIndex:METAL_BUFFER_IDX(PATH_BUFFER_IDX)];
      [atlasEncoder
          setVertexBuffer:mtl_buffer(paintAuxBufferRing())
                   offset:desc.firstPaintAux * sizeof(gpu::PaintAuxData)
                  atIndex:METAL_BUFFER_IDX(PAINT_AUX_BUFFER_IDX)];
    }
    if (desc.contourCount > 0)
    {
      [atlasEncoder setVertexBuffer:mtl_buffer(contourBufferRing())
                             offset:desc.firstContour * sizeof(gpu::ContourData)
                            atIndex:METAL_BUFFER_IDX(CONTOUR_BUFFER_IDX)];
    }
    [atlasEncoder setVertexBuffer:m_pathPatchVertexBuffer offset:0 atIndex:0];

    if (desc.featherAtlasFillBatchCount != 0)
    {
      [atlasEncoder setCullMode:MTLCullModeNone];
      [atlasEncoder setRenderPipelineState:atlasFillPipelineState];
      for (size_t i = 0; i < desc.featherAtlasFillBatchCount; ++i)
      {
        const gpu::AtlasDrawBatch& fillBatch = desc.featherAtlasFillBatches[i];
        [atlasEncoder setScissorRect:make_scissor(fillBatch.scissor)];
        [atlasEncoder
            setVertexBytes:&fillBatch.basePatch
                    length:sizeof(uint32_t)
                   atIndex:METAL_BUFFER_IDX(DRAW_ARGUMENTS_UNIFORM_BUFFER_IDX)];
        [atlasEncoder
            drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                       indexCount:gpu::kMidpointFanCenterAAPatchIndexCount
                        indexType:MTLIndexTypeUInt16
                      indexBuffer:m_pathPatchIndexBuffer
                indexBufferOffset:gpu::kMidpointFanCenterAAPatchBaseIndex *
                                  sizeof(uint16_t)
                    instanceCount:fillBatch.patchCount];
      }
    }

    if (desc.featherAtlasStrokeBatchCount != 0)
    {
      [atlasEncoder setCullMode:MTLCullModeBack];
      [atlasEncoder setRenderPipelineState:atlasStrokePipelineState];
      for (size_t i = 0; i < desc.featherAtlasStrokeBatchCount; ++i)
      {
        const gpu::AtlasDrawBatch&
            strokeBatch = desc.featherAtlasStrokeBatches[i];
        [atlasEncoder setScissorRect:make_scissor(strokeBatch.scissor)];
        [atlasEncoder
            setVertexBytes:&strokeBatch.basePatch
                    length:sizeof(uint32_t)
                   atIndex:METAL_BUFFER_IDX(DRAW_ARGUMENTS_UNIFORM_BUFFER_IDX)];
        [atlasEncoder
            drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                       indexCount:gpu::kMidpointFanPatchBorderIndexCount
                        indexType:MTLIndexTypeUInt16
                      indexBuffer:m_pathPatchIndexBuffer
                indexBufferOffset:gpu::kMidpointFanPatchBaseIndex *
                                  sizeof(uint16_t)
                    instanceCount:strokeBatch.patchCount];
      }
    }

    [atlasEncoder endEncoding];
  }

  // Generate mipmaps if needed.
  for (const DrawBatch& batch : *desc.drawList)
  {
    if (auto imageTextureMetal = static_cast<const TextureMetalImpl*>(
            batch.imageTexture))
    {
      imageTextureMetal->ensureMipmaps(commandBuffer);
    }
  }

  // Set up a render pass to do the final rendering using (some form of) pixel
  // local storage.
  MTLRenderPassDescriptor*
      pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.renderTargetWidth = desc.renderTargetUpdateBounds.right;
  pass.renderTargetHeight = desc.renderTargetUpdateBounds.bottom;
  pass.colorAttachments[COLOR_PLANE_IDX].texture = renderTarget
                                                       ->targetTexture();
  switch (desc.colorLoadAction)
  {
    case gpu::LoadAction::clear:
    {
      float cc[4];
      UnpackColorToRGBA32FPremul(desc.colorClearValue, cc);
      pass.colorAttachments[COLOR_PLANE_IDX].loadAction = MTLLoadActionClear;
      pass.colorAttachments[COLOR_PLANE_IDX].clearColor = MTLClearColorMake(
          cc[0], cc[1], cc[2], cc[3]);
      break;
    }
    case gpu::LoadAction::preserveRenderTarget:
      pass.colorAttachments[COLOR_PLANE_IDX].loadAction = MTLLoadActionLoad;
      break;
    case gpu::LoadAction::dontCare:
      pass.colorAttachments[COLOR_PLANE_IDX].loadAction = MTLLoadActionDontCare;
      break;
  }
  pass.colorAttachments[COLOR_PLANE_IDX].storeAction = MTLStoreActionStore;

  // Without framebuffer fetch, depthStencil draws write color to the
  // raster-ordered color buffer and the pass has no color attachment at all.
  const bool rendersColorToBuffer = desc.interlockMode ==
                                        gpu::InterlockMode::depthStencil &&
                                    m_metalFeatures
                                        .depthStencilRendersColorToBuffer(
                                            desc.fixedFunctionColorOutput);
  if (desc.interlockMode == gpu::InterlockMode::depthStencil)
  {
    id<MTLTexture> depthStencilTexture = renderTarget->depthStencilTexture();
    pass.depthAttachment.texture = depthStencilTexture;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.clearDepth = desc.depthClearValue;
    // Breaking the render pass for a dst copy has to pick depth and stencil
    // back up where they left off.
    const MTLStoreAction depthStencilStoreAction = desc.firstDstBlendBarrier !=
                                                           nullptr
                                                       ? MTLStoreActionStore
                                                       : MTLStoreActionDontCare;
    pass.depthAttachment.storeAction = depthStencilStoreAction;
    pass.stencilAttachment.texture = depthStencilTexture;
    pass.stencilAttachment.loadAction = MTLLoadActionClear;
    pass.stencilAttachment.clearStencil = desc.stencilClearValue;
    pass.stencilAttachment.storeAction = depthStencilStoreAction;

    if (rendersColorToBuffer && !desc.renderTargetUpdateBounds.empty())
    {
      if (desc.colorLoadAction == gpu::LoadAction::clear)
      {
        float cc[4];
        UnpackColorToRGBA32FPremul(desc.colorClearValue, cc);
        uint8_t clearBytes[4];
        for (int i = 0; i < 4; ++i)
        {
          clearBytes[i] = static_cast<uint8_t>(
              std::lround(std::clamp(cc[i], 0.f, 1.f) * 255.f));
        }
        const bool clearIsUniformByte = (renderTarget->pixelFormat() ==
                                             MTLPixelFormatRGBA8Unorm ||
                                         renderTarget->pixelFormat() ==
                                             MTLPixelFormatBGRA8Unorm) &&
                                        clearBytes[0] == clearBytes[1] &&
                                        clearBytes[0] == clearBytes[2] &&
                                        clearBytes[0] == clearBytes[3];
        if (clearIsUniformByte)
        {
          fillColorBuffer(
              commandBuffer,
              renderTarget,
              desc.renderTargetUpdateBounds,
              clearBytes[0]);
        }
        else
        {
          // Blits can't fill a buffer with a 32-bit value, so clear the
          // render target with a draw-less pass and copy that in.
          MTLRenderPassDescriptor*
              clearPass = [MTLRenderPassDescriptor renderPassDescriptor];
          clearPass.colorAttachments[0].texture = renderTarget->targetTexture();
          clearPass.colorAttachments[0].loadAction = MTLLoadActionClear;
          clearPass.colorAttachments[0]
              .clearColor = pass.colorAttachments[COLOR_PLANE_IDX].clearColor;
          clearPass.colorAttachments[0].storeAction = MTLStoreActionStore;
          [[commandBuffer renderCommandEncoderWithDescriptor:clearPass]
              endEncoding];
          copyTargetToColorBuffer(
              commandBuffer, renderTarget, desc.renderTargetUpdateBounds);
        }
      }
      else if (desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget)
      {
        copyTargetToColorBuffer(
            commandBuffer, renderTarget, desc.renderTargetUpdateBounds);
      }
    }
    if (rendersColorToBuffer)
    {
      pass.colorAttachments[COLOR_PLANE_IDX].texture = nil;
      pass.colorAttachments[COLOR_PLANE_IDX].loadAction = MTLLoadActionDontCare;
      pass.colorAttachments[COLOR_PLANE_IDX]
          .storeAction = MTLStoreActionDontCare;
    }
  }

  auto baselineShaderMiscFlags = gpu::ShaderMiscFlags::none;
  if (desc.interlockMode == gpu::InterlockMode::rasterOrdering)
  {
    // In rasterOrdering mode, the PLS planes are accessed as color
    // attachments.
    pass.colorAttachments[CLIP_PLANE_IDX]
        .texture = renderTarget->m_clipMemorylessTexture;
    pass.colorAttachments[CLIP_PLANE_IDX].loadAction = MTLLoadActionClear;
    pass.colorAttachments[CLIP_PLANE_IDX].clearColor = MTLClearColorMake(
        0, 0, 0, 0);
    pass.colorAttachments[CLIP_PLANE_IDX].storeAction = MTLStoreActionDontCare;

    pass.colorAttachments[SCRATCH_COLOR_PLANE_IDX]
        .texture = renderTarget->m_scratchColorMemorylessTexture;
    pass.colorAttachments[SCRATCH_COLOR_PLANE_IDX]
        .loadAction = MTLLoadActionDontCare;
    pass.colorAttachments[SCRATCH_COLOR_PLANE_IDX]
        .storeAction = MTLStoreActionDontCare;

    pass.colorAttachments[COVERAGE_PLANE_IDX]
        .texture = renderTarget->m_coverageMemorylessTexture;
    pass.colorAttachments[COVERAGE_PLANE_IDX].loadAction = MTLLoadActionClear;
    pass.colorAttachments[COVERAGE_PLANE_IDX].clearColor = MTLClearColorMake(
        desc.coverageClearValue, 0, 0, 0);
    pass.colorAttachments[COVERAGE_PLANE_IDX]
        .storeAction = MTLStoreActionDontCare;
  }
  else if (
      desc.interlockMode == gpu::InterlockMode::atomics &&
      desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget &&
      !desc.fixedFunctionColorOutput)
  {
    // Since we need to preserve the renderTarget during load, and since
    // we're rendering to an offscreen color buffer, we have to literally
    // copy the renderTarget into the color buffer.
    copyTargetToColorBuffer(
        commandBuffer, renderTarget, desc.renderTargetUpdateBounds);
  }

  // Execute the DrawList.

  // Start the current scissor rect inside out to guarantee that the first
  // rectangle we get doesn't match it.
  const auto
      fullRenderTargetScissorRect = desc.renderTargetUpdateBounds
                                        .lossless_numeric_cast<uint16_t>();
  auto currentScissorRect = AABBu16{0xffff, 0xffff, 0, 0};

  id<MTLRenderCommandEncoder> encoder = makeRenderPassForDraws(
      desc, pass, commandBuffer, baselineShaderMiscFlags);
  for (const DrawBatch& batch : *desc.drawList)
  {
    // Setup the pipeline for this specific drawType and shaderFeatures.
    gpu::ShaderFeatures shaderFeatures;
    if (desc.interlockMode == gpu::InterlockMode::atomics)
    {
      // The combined shader features might have more flags set than are
      // actually relevant for this draw type, so filter them out.
      shaderFeatures = desc.combinedShaderFeatures &
                       gpu::ShaderFeaturesMaskFor(
                           batch.drawType, desc.interlockMode);
    }
    else
    {
      shaderFeatures = batch.shaderFeatures;
    }

    gpu::ShaderMiscFlags batchMiscFlags = baselineShaderMiscFlags |
                                          batch.shaderMiscFlags;
    if (rendersColorToBuffer)
    {
      // The target isn't attached when an advanced blend forces the color
      // buffer, so srcOver draws have to blend in the shader too.
      batchMiscFlags &= ~gpu::ShaderMiscFlags::fixedFunctionColorOutput;
    }
    if (desc.interlockMode == gpu::InterlockMode::atomics &&
        !enums::is_flag_set(
            batchMiscFlags, gpu::ShaderMiscFlags::fixedFunctionColorOutput))
    {
      if (batch.drawType == gpu::DrawType::renderPassResolve)
      {
        // Atomic mode can always do a coalesced resolve when rendering
        // to an offscreen color buffer.
        assert(desc.interlockMode == gpu::InterlockMode::atomics);
        batchMiscFlags |= gpu::ShaderMiscFlags::coalescedResolveAndTransfer;
      }
      else if (batch.drawType == gpu::DrawType::renderPassInitialize)
      {
        assert(desc.interlockMode == gpu::InterlockMode::atomics);
        if (desc.colorLoadAction == gpu::LoadAction::clear)
        {
          batchMiscFlags |= gpu::ShaderMiscFlags::storeColorClear;
        }
        else if (
            desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget &&
            renderTarget->pixelFormat() == MTLPixelFormatBGRA8Unorm)
        {
          // We already copied the renderTarget to our color buffer,
          // but since the target is BGRA, we also need to swizzle it
          // to RGBA before it's ready for PLS.
          batchMiscFlags |= gpu::ShaderMiscFlags::swizzleColorBGRAToRGBA;
        }
      }
    }
    gpu::PipelineState pipelineState;
    if (desc.interlockMode == gpu::InterlockMode::depthStencil)
    {
      gpu::get_pipeline_state(batch, desc, m_platformFeatures, &pipelineState);
    }
    const DrawPipeline* drawPipeline = findCompatibleDrawPipeline(
        batch.drawType,
        shaderFeatures,
        desc,
        batchMiscFlags,
        renderTarget->pixelFormat(),
        pipelineState.colorWriteEnabled);
    if (drawPipeline == nullptr || !drawPipeline->valid())
    {
      // The shader for this draw AND the uber-shader both failed to
      // compile. This should virtually never happen, and can only happen
      // on non-Apple Silicon, where we don't use precompiled shaders.
      // Skip the draw.
      continue;
    }

    // Issue any barriers if needed. (These come first because some of them
    // replace the encoder, which drops every bit of state set below.)
    if (enums::any_flag_set(
            batch.barriers,
            BarrierFlags::plsAtomic | BarrierFlags::plsAtomicPreResolve))
    {
      assert(desc.interlockMode == gpu::InterlockMode::atomics);
      id<MTLRenderCommandEncoder> barrierEncoder = issuePipelineBarrier(
          encoder, desc, pass, commandBuffer, baselineShaderMiscFlags);
      if (barrierEncoder != encoder)
      {
        // A new encoder starts without a scissor rect.
        encoder = barrierEncoder;
        currentScissorRect = AABBu16{0xffff, 0xffff, 0, 0};
      }
    }
    else if (enums::is_flag_set(batch.barriers, BarrierFlags::dstBlend))
    {
      assert(
          desc.interlockMode == gpu::InterlockMode::depthStencil &&
          m_metalFeatures.depthStencilReadsDstFromTexture());
      encoder = copyDstReadsAndRestart(
          encoder, batch, desc, pass, commandBuffer, baselineShaderMiscFlags);
      currentScissorRect = AABBu16{0xffff, 0xffff, 0, 0};
    }

    {
      auto desiredScissorRect = batch.scissorRect.has_value()
                                    ? fullRenderTargetScissorRect
                                          .intersectOrEmpty(
                                              batch.scissorRect.value())
                                    : fullRenderTargetScissorRect;

      if (desiredScissorRect != currentScissorRect)
      {
        currentScissorRect = desiredScissorRect;
        [encoder setScissorRect:make_scissor(currentScissorRect)];
      }
    }

    // depthStencil keeps depth/stencil, cull and the stencil reference as
    // encoder state, and bakes only the color write mask into the
    // pipeline.
    MTLCullMode depthStencilCullMode = MTLCullModeNone;
    if (desc.interlockMode == gpu::InterlockMode::depthStencil)
    {
      depthStencilCullMode = metalCullMode(pipelineState.cullFace);
      if (!gpu::drawTypeHasPipelineDynamicState(batch.drawType))
      {
        // (Dynamic-state draw types set this per pass below.)
        [encoder
            setDepthStencilState:findOrCreateDepthStencilState(pipelineState)];
        [encoder setStencilReferenceValue:pipelineState.stencilReference];
      }
    }

    id<MTLRenderPipelineState> drawPipelineState = drawPipeline
                                                       ->pipelineState();

    // Bind the appropriate image texture, if any.
    if (auto imageTextureMetal = static_cast<const TextureMetalImpl*>(
            batch.imageTexture))
    {
      [encoder setFragmentTexture:imageTextureMetal->texture()
                          atIndex:IMAGE_TEXTURE_IDX];

      [encoder
          setFragmentSamplerState:m_imageSamplers[batch.imageSampler.asKey()]
                          atIndex:IMAGE_TEXTURE_IDX];
    }
    else
    {
      [encoder
          setFragmentSamplerState:m_imageSamplers[BilinearClampImageSamplerKey]
                          atIndex:IMAGE_TEXTURE_IDX];
    }

    DrawType drawType = batch.drawType;
    if (desc.wireframe)
    {
      // Wireframe is a debugging aid. The initialize/resolve are
      // fullscreen operations, so leave them solid even in wireframe
      // mode.
      [encoder
          setTriangleFillMode:drawType != gpu::DrawType::renderPassInitialize &&
                                      drawType !=
                                          gpu::DrawType::renderPassResolve
                                  ? MTLTriangleFillModeLines
                                  : MTLTriangleFillModeFill];
    }
    switch (drawType)
    {
      case DrawType::midpointFanPatches:
      case DrawType::midpointFanCenterAAPatches:
      case DrawType::outerCurvePatches:
      {
        // Draw PLS patches that connect the tessellation vertices.
        [encoder setRenderPipelineState:drawPipelineState];
        [encoder setVertexBuffer:m_pathPatchVertexBuffer offset:0 atIndex:0];
        [encoder setCullMode:MTLCullModeBack];
        // Don't use baseInstance in order to run on Apple GPU Family 2.
        // TODO: Use baseInstance instead once we deprecate Apple2.
        uint32_t baseInstance = batch.baseElement();
        [encoder
            setVertexBytes:&baseInstance
                    length:sizeof(uint32_t)
                   atIndex:METAL_BUFFER_IDX(DRAW_ARGUMENTS_UNIFORM_BUFFER_IDX)];
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:batch.indexCountPerInstance
                             indexType:MTLIndexTypeUInt16
                           indexBuffer:m_pathPatchIndexBuffer
                     indexBufferOffset:batch.baseIndex * sizeof(uint16_t)
                         instanceCount:batch.elementCount()];
        break;
      }
      case DrawType::interiorTriangulation:
      case DrawType::featherAtlasBlit:
      {
        [encoder setRenderPipelineState:drawPipelineState];
        [encoder setVertexBuffer:mtl_buffer(triangleBufferRing())
                          offset:0
                         atIndex:0];
        assert(
            desc.interlockMode != gpu::InterlockMode::depthStencil ||
            depthStencilCullMode == MTLCullModeBack);
        [encoder setCullMode:MTLCullModeBack];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:batch.baseElement()
                    vertexCount:batch.elementCount()];
        break;
      }
      case DrawType::imageRect:
      {
        [encoder setRenderPipelineState:drawPipelineState];
        [encoder
            setVertexBuffer:mtl_buffer(imageRectInstanceBufferRing())
                     offset:batch.baseElement() * sizeof(gpu::ImageRectInstance)
                    atIndex:2];
        [encoder setCullMode:MTLCullModeNone];
        assert(desc.interlockMode == gpu::InterlockMode::atomics);
        [encoder setVertexBuffer:m_imageRectVertexBuffer offset:0 atIndex:0];
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:batch.indexCountPerInstance
                             indexType:MTLIndexTypeUInt16
                           indexBuffer:m_imageRectIndexBuffer
                     indexBufferOffset:batch.baseIndex * sizeof(uint16_t)
                         instanceCount:batch.elementCount()];
        break;
      }
      case DrawType::imageMesh:
      {
        [encoder setRenderPipelineState:drawPipelineState];
        [encoder
            setVertexBuffer:mtl_buffer(imageMeshInstanceBufferRing())
                     offset:batch.baseElement() * sizeof(gpu::ImageMeshInstance)
                    atIndex:2];
        assert(
            desc.interlockMode != gpu::InterlockMode::depthStencil ||
            depthStencilCullMode == MTLCullModeNone);
        [encoder setCullMode:MTLCullModeNone];
        LITE_RTTI_CAST_OR_BREAK(
            vertexBuffer, RenderBufferMetalImpl*, batch.vertexBuffer);
        LITE_RTTI_CAST_OR_BREAK(
            uvBuffer, RenderBufferMetalImpl*, batch.uvBuffer);
        LITE_RTTI_CAST_OR_BREAK(
            indexBuffer, RenderBufferMetalImpl*, batch.indexBuffer);
        [encoder setVertexBuffer:vertexBuffer->submittedBuffer()
                          offset:0
                         atIndex:0];
        [encoder setVertexBuffer:uvBuffer->submittedBuffer()
                          offset:0
                         atIndex:1];
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:batch.indexCountPerInstance
                             indexType:MTLIndexTypeUInt16
                           indexBuffer:indexBuffer->submittedBuffer()
                     indexBufferOffset:batch.baseIndex * sizeof(uint16_t)
                         instanceCount:batch.elementCount()];
        break;
      }
      case DrawType::renderPassInitialize:
      case DrawType::renderPassResolve:
      {
        assert(desc.interlockMode == gpu::InterlockMode::atomics);
        [encoder setRenderPipelineState:drawPipelineState];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                    vertexStart:0
                    vertexCount:4];
        break;
      }
      case DrawType::stencilMidpointFanBorrowedCoverage:
      case DrawType::stencilMidpointFans:
      case DrawType::stencilMidpointFanReset:
      case DrawType::stencilMidpointFanWinding:
      case DrawType::stencilMidpointFanCover:
      case DrawType::stencilOuterCubicBorrowedCoverage:
      case DrawType::stencilOuterCubicReset:
      case DrawType::stencilOuterCubicWinding:
      case DrawType::stencilOuterCubicCover:
      case DrawType::stencilOuterCubics:
      case DrawType::depthStrokes:
      case DrawType::depthAAStrokes:
      case DrawType::depthAAOuterHairline:
      {
        assert(desc.interlockMode == gpu::InterlockMode::depthStencil);
        [encoder setRenderPipelineState:drawPipelineState];
        [encoder setCullMode:depthStencilCullMode];
        drawDepthStencilIndexRanges(
            encoder, batch, gpu::DSIndexRangeChunker::drawsForBatch(batch));
        break;
      }
      case DrawType::stencilDynamicMidpointFans:
      case DrawType::stencilDynamicOuterCubics:
      {
        assert(desc.interlockMode == gpu::InterlockMode::depthStencil);
        [encoder setRenderPipelineState:drawPipelineState];
        const gpu::DSDynamicFillPipelineStates passStates(
            desc, batch, batchMiscFlags, m_platformFeatures);
        for (const auto& range : batch.elementRanges())
        {
          for (const gpu::PipelineState& passState : passStates)
          {
            [encoder
                setDepthStencilState:findOrCreateDepthStencilState(passState)];
            [encoder setStencilReferenceValue:passState.stencilReference];
            [encoder setCullMode:metalCullMode(passState.cullFace)];
            drawDepthStencilIndexRanges(
                encoder,
                batch,
                gpu::DSIndexRangeChunker::drawsForDynamicFillSubpass(
                    batch, range, passState));
          }
        }
        break;
      }
      case DrawType::clipReset:
      {
        assert(desc.interlockMode == gpu::InterlockMode::depthStencil);
        [encoder setRenderPipelineState:drawPipelineState];
        [encoder setVertexBuffer:mtl_buffer(triangleBufferRing())
                          offset:0
                         atIndex:0];
        [encoder setCullMode:depthStencilCullMode];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:batch.baseElement()
                    vertexCount:batch.elementCount()];
        break;
      }
    }
  }
  [encoder endEncoding];

  if (rendersColorToBuffer && !desc.renderTargetUpdateBounds.empty())
  {
    // The frame went to the color buffer; copy it out to the target.
    copyColorBufferToTarget(
        commandBuffer, renderTarget, desc.renderTargetUpdateBounds);
  }
}

void RenderContextMetalImpl::drawDepthStencilIndexRanges(
    id<MTLRenderCommandEncoder> encoder,
    const DrawBatch& batch,
    const gpu::DSIndexRangeChunker& chunker)
{
  for (auto [chunkIndexCount, chunkBaseVertex] : chunker)
  {
    // Metal has no base vertex on Apple GPU Family 2, so the shader adds
    // this uniform to the vertex ID instead.
    [encoder
        setVertexBytes:&chunkBaseVertex
                length:sizeof(chunkBaseVertex)
               atIndex:METAL_BUFFER_IDX(DRAW_ARGUMENTS_UNIFORM_BUFFER_IDX)];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:chunkIndexCount
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:m_pathPatchIndexBuffer
                 indexBufferOffset:batch.baseIndex * sizeof(uint16_t)
                     instanceCount:1];
  }
}

void RenderContextMetalImpl::postFlush(
    const RenderContext::FlushResources& flushResources)
{
  // Schedule a callback that will unlock the buffers used by this flush,
  // after the GPU has finished rendering with them. This unblocks the CPU
  // from reusing them in a future flush.
  id<MTLCommandBuffer> commandBuffer = (__bridge id<MTLCommandBuffer>)
                                           flushResources.externalCommandBuffer;
  std::mutex& thisFlushLock = m_bufferRingLocks[m_bufferRingIdx];
  [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
    assert(!thisFlushLock.try_lock()); // The mutex should already be locked.
    thisFlushLock.unlock();
  }];
}
} // namespace rive::gpu

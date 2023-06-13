/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/pls/buffer_ring.hpp"
#include "rive/pls/pls.hpp"
#include "rive/pls/pls_render_target.hpp"
#include "rive/shapes/paint/color.hpp"
#include <functional>
#include <unordered_map>

namespace rive::pls
{
class GradientLibrary;
class PLSGradient;
class PLSPaint;
class PLSPath;

// Used as a key for complex gradients.
class GradientContentKey
{
public:
    inline GradientContentKey(rcp<const PLSGradient> gradient);
    inline GradientContentKey(GradientContentKey&& other);
    bool operator==(const GradientContentKey&) const;
    const PLSGradient* gradient() const { return m_gradient.get(); }

private:
    rcp<const PLSGradient> m_gradient;
};

// Hashes all stops and all colors in a complex gradient.
class DeepHashGradient
{
public:
    size_t operator()(const GradientContentKey&) const;
};

// Top-level, API agnostic rendering context for PLSRenderer. This class manages all the GPU
// buffers, context state, and other resources required for Rive's pixel local storage path
// rendering algorithm.
//
// Main algorithm:
// https://docs.google.com/document/d/19Uk9eyFxav6dNSYsI2ZyiX9zHU1YOaJsMB2sdDFVz6s/edit
//
// Batching multiple unique paths:
// https://docs.google.com/document/d/1DLrQimS5pbNaJJ2sAW5oSOsH6_glwDPo73-mtG5_zns/edit
//
// Batching strokes as well:
// https://docs.google.com/document/d/1CRKihkFjbd1bwT08ErMCP4fwSR7D4gnHvgdw_esY9GM/edit
//
// Intended usage pattern of this class:
//
//   context->beginFrame(...);
//   for (path : paths)
//   {
//       context->pushPath(...);
//       for (contour : path.contours)
//       {
//           context->pushContour(...);
//           for (cubic : contour.cubics)
//           {
//               context->pushCubic(...);
//           }
//       }
//   }
//   context->flush();
//
// Furthermore, the subclass isues the actual rendering commands during onFlush(). Rendering is done
// in two steps:
//
//  1. Tessellate all curves into positions and normals in the "tessellation texture".
//  2. Render the tessellated curves using Rive's pixel local storage path rendering algorithm.
//
class PLSRenderContext
{
public:
    virtual ~PLSRenderContext();

    // Specifies what to do with the render target at the beginning of a flush.
    enum class LoadAction : bool
    {
        clear,
        preserveRenderTarget
    };

    // Options for controlling how and where a frame is rendered.
    struct FrameDescriptor
    {
        rcp<const PLSRenderTarget> renderTarget;
        LoadAction loadAction = LoadAction::clear;
        ColorInt clearColor = 0;

        // Testing flags.
        bool wireframe = false;
        bool fillsDisabled = false;
        bool strokesDisabled = false;
    };

    // Called at the beginning of a frame and establishes where and how it will be rendered.
    //
    // All rendering related calls must be made between beginFrame() and flush().
    void beginFrame(FrameDescriptor&&);

    const FrameDescriptor& frameDescriptor() const
    {
        assert(m_didBeginFrame);
        return m_frameDescriptor;
    }

    // Generates a unique clip ID that is guaranteed to not exist in the current clip buffer.
    //
    // Returns 0 if a unique ID could not be generated, at which point the caller must flush before
    // continuing.
    uint32_t generateClipID();

    // Get/set a "clip content ID" that uniquely identifies the current contents of the clip buffer.
    // This ID is reset to 0 on every flush.
    void setClipContentID(uint32_t clipID)
    {
        assert(m_didBeginFrame);
        m_clipContentID = clipID;
    }
    uint32_t getClipContentID()
    {
        assert(m_didBeginFrame);
        return m_clipContentID;
    }

    // Reserves space for 'pathCount', 'contourCount', 'curveCount', and 'tessVertexCount' records
    // in their respective GPU buffers, prior to calling pushPath(), pushContour(), pushCubic().
    //
    // Returns false if there was too much data to accomodate, at which point the caller mush flush
    // before continuing.
    [[nodiscard]] bool reservePathData(size_t pathCount,
                                       size_t contourCount,
                                       size_t curveCount,
                                       uint32_t tessVertexCount);

    // Adds the given paint to the GPU data library and fills out 'PaintData' with the information
    // required by the GPU to access it.
    //
    // Returns false if there wasn't room in the library, at which point the caller mush flush
    // before continuing.
    [[nodiscard]] bool pushPaint(const PLSPaint*, PaintData*);

    // Pushes a record to the GPU for the given path, which will be referenced by future calls to
    // pushContour() and pushCubic().
    void pushPath(const Mat2D&,
                  float strokeRadius,
                  FillRule,
                  PaintType,
                  uint32_t clipID,
                  PLSBlendMode,
                  const PaintData&);

    // Pushes a contour record to the GPU for the given contour, which references the most-recently
    // pushed path and will be referenced by future calls to pushCubic().
    //
    // The first curve of the contour will be pre-padded with "paddingVertexCount" tessellation
    // vertices, colocated at T=0. This allows the caller to align wedge boundaries with contour
    // boundaries by making the number of tessellation vertices in the contour an exact multiple of
    // kWedgeSize.
    void pushContour(Vec2D midpoint, bool closed, uint32_t paddingVertexCount);

    // Appends a cubic curve and join to the most-recently pushed contour, and reserves the
    // appropriate number of tessellated vertices in the tessellation texture.
    //
    // An instance consists of a cubic curve with "parametricSegmentCount + polarSegmentCount"
    // segments, followed by a join with "joinSegmentCount" segments, for a grand total of
    // "parametricSegmentCount + polarSegmentCount + joinSegmentCount - 1" vertices.
    //
    // If a cubic has already been pushed to the current contour, pts[0] must be equal to the former
    // cubic's pts[3].
    //
    // "joinTangent" is the ending tangent of the join that follows the cubic.
    void pushCubic(const Vec2D pts[4],
                   Vec2D joinTangent,
                   uint32_t joinTypeFlag,
                   uint32_t parametricSegmentCount,
                   uint32_t polarSegmentCount,
                   uint32_t joinSegmentCount);

    enum class FlushType : bool
    {
        // The flush was kicked off mid-frame because GPU buffers ran out of room. The flush
        // follwing this one will load the color buffer instead of clearing it, and color data may
        // not make it all the way to the main framebuffer.
        intermediate,

        // This is the final flush of the frame. The flush follwing this one will always clear color
        // buffer, and the main framebuffer is guaranteed to be updated.
        endOfFrame
    };

    // Renders all paths that have been pushed since the last call to flush(). Rendering is done in
    // two steps:
    //
    //  1. Tessellate all cubics into positions and normals in the "tessellation texture".
    //  2. Render the tessellated curves using Rive's pixel local storage path rendering algorithm.
    //
    void flush(FlushType = FlushType::endOfFrame);

    // Reallocates all GPU resources to the basline minimum allocation size.
    void resetGPUResources();

    // Shrinks GPU resource allocations to the maximum per-flush limits seen since the most recent
    // previous call to shrinkGPUResourcesToFit(). This method is intended to be called at a fixed
    // temporal interval. (GPU resource allocations automatically grow based on usage, but can only
    // shrink if the application calls this method.)
    void shrinkGPUResourcesToFit();

protected:
    PLSRenderContext(const PlatformFeatures&);

    virtual std::unique_ptr<BufferRingImpl> makeVertexBufferRing(size_t capacity,
                                                                 size_t itemSizeInBytes) = 0;

    virtual std::unique_ptr<TexelBufferRing> makeTexelBufferRing(TexelBufferRing::Format,
                                                                 size_t widthInItems,
                                                                 size_t height,
                                                                 size_t texelsPerItem,
                                                                 int textureIdx,
                                                                 TexelBufferRing::Filter) = 0;

    virtual std::unique_ptr<BufferRingImpl> makeUniformBufferRing(size_t capacity,
                                                                  size_t sizeInBytes) = 0;

    virtual void allocateTessellationTexture(size_t height) = 0;

    const BufferRingImpl* colorRampUniforms() const
    {
        return static_cast<const BufferRingImpl*>(m_colorRampUniforms.impl());
    }
    const BufferRingImpl* tessellateUniforms()
    {
        return static_cast<const BufferRingImpl*>(m_tessellateUniforms.impl());
    }
    const BufferRingImpl* drawUniforms()
    {
        return static_cast<const BufferRingImpl*>(m_drawUniforms.impl());
    }
    const BufferRingImpl* drawParametersBufferRing()
    {
        return static_cast<const BufferRingImpl*>(m_drawParametersBuffer.impl());
    }
    const TexelBufferRing* pathBufferRing()
    {
        return static_cast<const TexelBufferRing*>(m_pathBuffer.impl());
    }
    const TexelBufferRing* contourBufferRing()
    {
        return static_cast<const TexelBufferRing*>(m_contourBuffer.impl());
    }
    const TexelBufferRing* gradTexelBufferRing() const
    {
        return static_cast<const TexelBufferRing*>(m_gradTexelBuffer.impl());
    }
    const BufferRingImpl* gradSpanBufferRing() const { return m_gradSpanBuffer.impl(); }
    const BufferRingImpl* tessSpanBufferRing() { return m_tessSpanBuffer.impl(); }

    size_t gradTextureRowsForSimpleRamps() const { return m_gradTextureRowsForSimpleRamps; }

    virtual void onBeginFrame() {}

    // Indicates how much blendMode support will be needed in the "uber" draw shader.
    enum class BlendTier : uint8_t
    {
        srcOver,     // Every draw uses srcOver.
        advanced,    // Draws use srcOver *and* advanced blend modes, excluding HSL modes.
        advancedHSL, // Draws use srcOver *and* advanced blend modes *and* advanced HSL modes.
    };

    // Used by ShaderFeatures to generate keys and source code.
    enum class SourceType : bool
    {
        vertexOnly,
        wholeProgram
    };

    // Indicates which "uber shader" features to enable in the draw shader.
    struct ShaderFeatures
    {
        enum PreprocessorDefines : uint64_t
        {
            ENABLE_ADVANCED_BLEND = 1 << 0,
            ENABLE_PATH_CLIPPING = 1 << 1,
            ENABLE_EVEN_ODD = 1 << 2,
            ENABLE_HSL_BLEND_MODES = 1 << 3,
        };

        // Returns a bitmask of which preprocessor macros must be defined in order to support the
        // current feature set.
        uint64_t getPreprocessorDefines(SourceType) const;

        struct
        {
            BlendTier blendTier = BlendTier::srcOver;
            bool enablePathClipping = false;
        } programFeatures;

        struct
        {
            bool enableEvenOdd = false;
        } fragmentFeatures;
    };

    virtual void onFlush(FlushType,
                         LoadAction,
                         size_t gradSpanCount,
                         size_t gradSpansHeight,
                         size_t tessVertexSpanCount,
                         size_t tessDataHeight,
                         size_t tessVertexCount,
                         const ShaderFeatures&) = 0;

    const PlatformFeatures m_platformFeatures;
    const size_t m_maxPathID;

private:
    static BlendTier BlendTierForBlendMode(PLSBlendMode);

    // Allocates a horizontal span of texels in the gradient texture and schedules either a texture
    // upload or draw that fills it with the given gradient's color ramp.
    [[nodiscard]] bool pushGradient(const PLSGradient*, PaintData*);

    // Capacities of all our GPU resource allocations.
    struct GPUResourceLimits
    {
        size_t maxPathID;
        size_t maxContourID;
        size_t maxSimpleGradients;
        size_t maxComplexGradients;
        size_t maxComplexGradientSpans;
        size_t maxTessellationSpans;
        size_t maxTessellationVertices;
        size_t maxDrawParameters;

        // "*this = max(*this, other)"
        void accumulateMax(const GPUResourceLimits& other)
        {
            maxPathID = std::max(maxPathID, other.maxPathID);
            maxContourID = std::max(maxContourID, other.maxContourID);
            maxSimpleGradients = std::max(maxSimpleGradients, other.maxSimpleGradients);
            maxComplexGradients = std::max(maxComplexGradients, other.maxComplexGradients);
            maxComplexGradientSpans =
                std::max(maxComplexGradientSpans, other.maxComplexGradientSpans);
            maxTessellationSpans = std::max(maxTessellationSpans, other.maxTessellationSpans);
            maxTessellationVertices =
                std::max(maxTessellationVertices, other.maxTessellationVertices);
            maxDrawParameters = std::max(maxDrawParameters, other.maxDrawParameters);
            static_assert(sizeof(*this) == sizeof(size_t) * 8); // Make sure we got every field.
        }

        // Scale each limit > threshold by a factor of "scaleFactor".
        GPUResourceLimits makeScaledIfLarger(const GPUResourceLimits& threshold,
                                             double scaleFactor) const
        {
            GPUResourceLimits scaled = *this;
            if (maxPathID > threshold.maxPathID)
                scaled.maxPathID = static_cast<double>(maxPathID) * scaleFactor;
            if (maxContourID > threshold.maxContourID)
                scaled.maxContourID = static_cast<double>(maxContourID) * scaleFactor;
            if (maxSimpleGradients > threshold.maxSimpleGradients)
                scaled.maxSimpleGradients = static_cast<double>(maxSimpleGradients) * scaleFactor;
            if (maxComplexGradients > threshold.maxComplexGradients)
                scaled.maxComplexGradients = static_cast<double>(maxComplexGradients) * scaleFactor;
            if (maxComplexGradientSpans > threshold.maxComplexGradientSpans)
                scaled.maxComplexGradientSpans =
                    static_cast<double>(maxComplexGradientSpans) * scaleFactor;
            if (maxTessellationSpans > threshold.maxTessellationSpans)
                scaled.maxTessellationSpans =
                    static_cast<double>(maxTessellationSpans) * scaleFactor;
            if (maxTessellationVertices > threshold.maxTessellationVertices)
                scaled.maxTessellationVertices =
                    static_cast<double>(maxTessellationVertices) * scaleFactor;
            if (maxDrawParameters > threshold.maxDrawParameters)
                scaled.maxDrawParameters = static_cast<double>(maxDrawParameters) * scaleFactor;
            static_assert(sizeof(*this) == sizeof(size_t) * 8); // Make sure we got every field.
            return scaled;
        }

        // Scale all limits by a factor of "scaleFactor".
        GPUResourceLimits makeScaled(double scaleFactor) const
        {
            return makeScaledIfLarger(GPUResourceLimits{}, scaleFactor);
        }
    };

    // Reallocate any GPU resource whose size in 'targetUsage' is larger than its size in
    // 'm_currentResourceLimits'.
    //
    // The new allocation size will be "targetUsage.<resourceLimit> * scaleFactor".
    void growExceededGPUResources(const GPUResourceLimits& targetUsage, double scaleFactor);

    // Reallocate resources to the size specified in 'targets'.
    // Only reallocate the resources for which 'shouldReallocate' returns true.
    void allocateGPUResources(
        const GPUResourceLimits& targets,
        std::function<bool(size_t targetSize, size_t currentSize)> shouldReallocate);

    GPUResourceLimits m_currentResourceLimits = {};
    GPUResourceLimits m_currentFrameResourceUsage = {};
    GPUResourceLimits m_maxRecentResourceUsage = {};

    // Here we cache the contents of the uniform buffers that only have one item (and are therefore
    // constant throughout an entire flush). We use these cached values to determine whether the
    // buffers need to be updated at the beginning of a flush.
    ColorRampUniforms m_cachedColorRampUniformData{};
    TessellateUniforms m_cachedTessUniformData{};
    DrawUniforms m_cachedDrawUniformData{0, 0, 0, m_platformFeatures};

    // Simple gradients only have 2 texels, so we write them to mapped texture memory from the CPU
    // instead of rendering them.
    struct TwoTexelRamp
    {
        void set(const ColorInt colors[2])
        {
            UnpackColorToRGBA8(colors[0], colorData);
            UnpackColorToRGBA8(colors[1], colorData + 4);
        }
        uint8_t colorData[8];
    };

    BufferRing<PathData> m_pathBuffer;
    BufferRing<ContourData> m_contourBuffer;
    BufferRing<TwoTexelRamp> m_gradTexelBuffer; // Simple gradients get written by the CPU.
    BufferRing<GradientSpan> m_gradSpanBuffer;  // Complex gradients get rendered by the GPU.
    BufferRing<TessVertexSpan> m_tessSpanBuffer;
    BufferRing<DrawParameters> m_drawParametersBuffer;
    BufferRing<ColorRampUniforms> m_colorRampUniforms;
    BufferRing<TessellateUniforms> m_tessellateUniforms;
    BufferRing<DrawUniforms> m_drawUniforms;

    // How many rows of the gradient texture are dedicated to simple (two-texel) ramps?
    // This is also the y-coordinate at which the complex color ramps begin.
    size_t m_gradTextureRowsForSimpleRamps = 0;

    // Per-frame state.
    FrameDescriptor m_frameDescriptor;
    bool m_isFirstFlushOfFrame = true;
    RIVE_DEBUG_CODE(bool m_didBeginFrame = false;)

    // Clipping state.
    uint32_t m_lastGeneratedClipID = 0;
    uint32_t m_clipContentID = 0;

    // Most recent path and contour state.
    bool m_currentPathIsStroked = false;
    uint32_t m_currentPathID = 0;
    uint32_t m_currentContourID = 0;
    uint32_t m_currentContourIDWithFlags = 0;
    uint32_t m_currentContourPaddingVertexCount = 0; // Padding vertices to add to the first curve.
    size_t m_tessVertexCount = 0;

    // Simple gradients have one stop at t=0 and one stop at t=1. They're implemented with 2 texels.
    std::unordered_map<uint64_t, uint32_t> m_simpleGradients; // [color0, color1] -> rampTexelsIdx

    // Complex gradients have stop(s) between t=0 and t=1. In theory they should be scaled to a ramp
    // where every stop lands exactly on a pixel center, but for now we just always scale them to
    // the entire gradient texture width.
    std::unordered_map<GradientContentKey, uint32_t, DeepHashGradient>
        m_complexGradients; // [colors[0..n], stops[0..n]] -> rowIdx

    // Combined set of "uber shader" features used by all currently-pushed paths.
    ShaderFeatures m_shaderFeatures;
};
} // namespace rive::pls

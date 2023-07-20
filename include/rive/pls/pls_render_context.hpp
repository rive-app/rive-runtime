/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/pls/buffer_ring.hpp"
#include "rive/pls/pls.hpp"
#include "rive/pls/pls_render_target.hpp"
#include "rive/pls/trivial_block_allocator.hpp"
#include "rive/shapes/paint/color.hpp"
#include <functional>
#include <unordered_map>

namespace rive
{
class GrInnerFanTriangulator;
class RawPath;
} // namespace rive

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

    // Utility for calculating the total tessellation vertex count of a batch of paths, and finding
    // how much padding to insert between paths.
    class TessVertexCounter
    {
    public:
        TessVertexCounter(PLSRenderContext* context) :
            m_initialTessVertexCount(context->m_tessVertexCount),
            m_runningTessVertexCount(context->m_tessVertexCount)
        {
            // Don't create a path calculator while the context is in the middle of pushing a path.
            assert(m_runningTessVertexCount == context->m_expectedTessVertexCountAtEndOfPath);
        }

        // Returns the required number of padding vertices to insert before the path.
        template <size_t PatchSize> [[nodiscard]] size_t countPath(size_t pathTessVertexCount)
        {
            size_t padding = PaddingToAlignUp<PatchSize>(m_runningTessVertexCount);
            m_runningTessVertexCount += padding + pathTessVertexCount;
            return padding;
        }

        size_t totalVertexCount() const
        {
            return m_runningTessVertexCount - m_initialTessVertexCount;
        }

    private:
        size_t m_initialTessVertexCount;
        size_t m_runningTessVertexCount;
    };

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
    //
    // The first curve of the path will be pre-padded with 'paddingVertexCount' tessellation
    // vertices, colocated at T=0. The caller must use this argument to align the beginning of the
    // path on a boundary of the patch size. (See PLSRenderContext::TessVertexCounter.)
    void pushPath(PatchType,
                  const Mat2D&,
                  float strokeRadius,
                  FillRule,
                  PaintType,
                  uint32_t clipID,
                  PLSBlendMode,
                  const PaintData&,
                  uint32_t tessVertexCount,
                  uint32_t paddingVertexCount);

    // Pushes a contour record to the GPU for the given contour, which references the most-recently
    // pushed path and will be referenced by future calls to pushCubic().
    //
    // The first curve of the contour will be pre-padded with 'paddingVertexCount' tessellation
    // vertices, colocated at T=0. The caller must use this argument to align the end of the contour
    // on a boundary of the patch size. (See pls::PaddingToAlignUp().)
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
                   uint32_t additionalPLSFlags,
                   uint32_t parametricSegmentCount,
                   uint32_t polarSegmentCount,
                   uint32_t joinSegmentCount);

    // Pushes triangles to be drawn using the data records from the most recent calls to pushPath()
    // and pushPaint().
    void pushInteriorTriangulation(GrInnerFanTriangulator*,
                                   PaintType,
                                   uint32_t clipID,
                                   PLSBlendMode);

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

    // Returns the context's TrivialBlockAllocator, which is automatically reset at the end of every
    // flush.
    TrivialBlockAllocator* trivialPerFlushAllocator()
    {
        assert(m_didBeginFrame);
        return &m_trivialPerFlushAllocator;
    }

    // Allocates a trivially destructible object that will be automatically deleted at the end of
    // the current flush.
    template <typename T, typename... Args> T* make(Args&&... args)
    {
        assert(m_didBeginFrame);
        return m_trivialPerFlushAllocator.make<T>(std::forward<Args>(args)...);
    }

    // Simple linked list whose nodes are allocated on a context's TrivialBlockAllocator.
    template <typename T> class PerFlushLinkedList
    {
    public:
        void reset() { m_tail = m_head = nullptr; }

        bool empty() const;
        T& tail() const;
        template <typename... Args> void emplace_back(PLSRenderContext* context, Args... args);

        struct Node
        {
            template <typename... Args> Node(Args... args) : data(std::forward<Args>(args)...) {}
            T data;
            Node* next = nullptr;
        };

        class Iter
        {
        public:
            Iter(Node* current) : m_current(current) {}
            bool operator!=(const Iter& other) const { return m_current != other.m_current; }
            void operator++() { m_current = m_current->next; }
            T& operator*() { return m_current->data; }

        private:
            Node* m_current;
        };
        Iter begin() { return {m_head}; }
        Iter end() { return {nullptr}; }

    private:
        Node* m_head = nullptr;
        Node* m_tail = nullptr;
    };

protected:
    PLSRenderContext(const PlatformFeatures&);

    virtual std::unique_ptr<BufferRingImpl> makeVertexBufferRing(size_t capacity,
                                                                 size_t itemSizeInBytes) = 0;

    enum class Renderable : bool
    {
        no,
        yes
    };

    virtual std::unique_ptr<TexelBufferRing> makeTexelBufferRing(TexelBufferRing::Format,
                                                                 Renderable,
                                                                 size_t widthInItems,
                                                                 size_t height,
                                                                 size_t texelsPerItem,
                                                                 int textureIdx,
                                                                 TexelBufferRing::Filter) = 0;

    virtual std::unique_ptr<BufferRingImpl> makeUniformBufferRing(size_t capacity,
                                                                  size_t sizeInBytes) = 0;

    virtual void allocateTessellationTexture(size_t height) = 0;

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
    const BufferRingImpl* triangleBufferRing() { return m_triangleBuffer.impl(); }
    const BufferRingImpl* uniformBufferRing() const { return m_uniformBuffer.impl(); }

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
        enum PreprocessorDefines : uint32_t
        {
            ENABLE_ADVANCED_BLEND = 1 << 0,
            ENABLE_PATH_CLIPPING = 1 << 1,
            ENABLE_EVEN_ODD = 1 << 2,
            ENABLE_HSL_BLEND_MODES = 1 << 3,
        };

        // Returns a bitmask of which preprocessor macros must be defined in order to support the
        // current feature set.
        uint32_t getPreprocessorDefines(SourceType) const;

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
                         bool needsClipBuffer) = 0;

    const PlatformFeatures m_platformFeatures;
    const size_t m_maxPathID;

    enum class DrawType : uint8_t
    {
        midpointFanPatches, // Standard paths and/or strokes.
        outerCurvePatches,  // Just the outer curves of a path; the interior will be triangulated.
        interiorTriangulation
    };

    constexpr static uint32_t PatchSegmentSpan(DrawType drawType)
    {
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
                return kMidpointFanPatchSegmentSpan;
            case DrawType::outerCurvePatches:
                return kOuterCurvePatchSegmentSpan;
            default:
                RIVE_UNREACHABLE();
        }
    }

    constexpr static uint32_t PatchIndexCount(DrawType drawType)
    {
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
                return kMidpointFanPatchIndexCount;
            case DrawType::outerCurvePatches:
                return kOuterCurvePatchIndexCount;
            default:
                RIVE_UNREACHABLE();
        }
    }

    constexpr static uintptr_t PatchBaseIndex(DrawType drawType)
    {
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
                return kMidpointFanPatchBaseIndex;
            case DrawType::outerCurvePatches:
                return kOuterCurvePatchBaseIndex;
            default:
                RIVE_UNREACHABLE();
        }
    }

    static uint32_t ShaderUniqueKey(SourceType sourceType,
                                    DrawType drawType,
                                    const ShaderFeatures& shaderFeatures)
    {
        return (shaderFeatures.getPreprocessorDefines(sourceType) << 1) |
               (drawType == DrawType::interiorTriangulation);
    }

    // Linked list of draws to be issued by the subclass during onFlush().
    struct Draw
    {
        Draw(DrawType drawType_, uint32_t baseVertexOrInstance_) :
            drawType(drawType_), baseVertexOrInstance(baseVertexOrInstance_)
        {}
        const DrawType drawType;
        uint32_t baseVertexOrInstance;
        uint32_t vertexOrInstanceCount = 0; // Calculated during PLSRenderContext::flush().
        ShaderFeatures shaderFeatures;
        GrInnerFanTriangulator* triangulator = nullptr; // Used by "interiorTriangulation" draws.
    };

    PerFlushLinkedList<Draw> m_drawList;
    size_t m_drawListCount = 0;

    // GrTriangulator provides an upper bound on the number of vertices it will emit. Triangulations
    // are not writen out until the last minute, during flush(), and this variable provides an upper
    // bound on the number of vertices that will be written.
    size_t m_maxTriangleVertexCount = 0;

private:
    static BlendTier BlendTierForBlendMode(PLSBlendMode);

    // Allocates a horizontal span of texels in the gradient texture and schedules either a texture
    // upload or draw that fills it with the given gradient's color ramp.
    [[nodiscard]] bool pushGradient(const PLSGradient*, PaintData*);

    // Either appends a draw to m_drawList or merges into m_lastDraw.
    // Updates the draw's ShaderFeatures according to the passed parameters.
    void pushDraw(DrawType, size_t baseVertex, FillRule, PaintType, uint32_t clipID, PLSBlendMode);

    // Writes padding vertices to the tessellation texture, with an invalid contour ID that is
    // guaranteed to not be the same ID as any neighbors.
    void pushPaddingVertices(uint32_t count);

    // Allocates a (potentially wrapped) span in the tessellation texture and pushes an instance to
    // render it. If the span does wraps, pushes multiple instances to render each horizontal
    // segment.
    RIVE_ALWAYS_INLINE void pushTessellationSpans(const Vec2D pts[4],
                                                  Vec2D joinTangent,
                                                  uint32_t totalVertexCount,
                                                  uint32_t parametricSegmentCount,
                                                  uint32_t polarSegmentCount,
                                                  uint32_t joinSegmentCount,
                                                  uint32_t contourIDWithFlags);

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
        size_t maxTriangleVertices;

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
            maxTriangleVertices = std::max(maxTriangleVertices, other.maxTriangleVertices);
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
            if (maxTriangleVertices > threshold.maxTriangleVertices)
                scaled.maxTriangleVertices = static_cast<double>(maxTriangleVertices) * scaleFactor;
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

    // Here we cache the contents of the uniform buffer. We use this cached value to determine
    // whether the buffer needs to be updated at the beginning of a flush.
    FlushUniforms m_cachedUniformData{0, 0, 0, 0, 0, m_platformFeatures};

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
    BufferRing<TriangleVertex> m_triangleBuffer;
    BufferRing<FlushUniforms> m_uniformBuffer;

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
    uint32_t m_currentContourPaddingVertexCount = 0; // Padding vertices to add to the first curve.
    uint32_t m_tessVertexCount = 0;
    RIVE_DEBUG_CODE(uint32_t m_expectedTessVertexCountAtNextReserve = 0;)
    RIVE_DEBUG_CODE(uint32_t m_expectedTessVertexCountAtEndOfPath = 0;)

    // Simple gradients have one stop at t=0 and one stop at t=1. They're implemented with 2 texels.
    std::unordered_map<uint64_t, uint32_t> m_simpleGradients; // [color0, color1] -> rampTexelsIdx

    // Complex gradients have stop(s) between t=0 and t=1. In theory they should be scaled to a ramp
    // where every stop lands exactly on a pixel center, but for now we just always scale them to
    // the entire gradient texture width.
    std::unordered_map<GradientContentKey, uint32_t, DeepHashGradient>
        m_complexGradients; // [colors[0..n], stops[0..n]] -> rowIdx

    // Simple allocator for trivially-destructible data that needs to persist until the current
    // flush has completed. Any object created with this allocator is automatically deleted during
    // the next call to flush().
    constexpr static size_t kPerFlushAllocatorInitialBlockSize = 1024 * 1024; // 1 MiB.
    TrivialBlockAllocator m_trivialPerFlushAllocator{kPerFlushAllocatorInitialBlockSize};
};

template <typename T> bool PLSRenderContext::PerFlushLinkedList<T>::empty() const
{
    assert(!!m_head == !!m_tail);
    return m_tail == nullptr;
}

template <typename T> T& PLSRenderContext::PerFlushLinkedList<T>::tail() const
{
    assert(!empty());
    return m_tail->data;
}

template <typename T>
template <typename... Args>
void PLSRenderContext::PerFlushLinkedList<T>::emplace_back(PLSRenderContext* context, Args... args)
{
    Node* node = context->make<Node>(std::forward<Args>(args)...);
    assert(!!m_head == !!m_tail);
    if (m_head == nullptr)
    {
        m_head = node;
    }
    else
    {
        m_tail->next = node;
    }
    m_tail = node;
}

} // namespace rive::pls

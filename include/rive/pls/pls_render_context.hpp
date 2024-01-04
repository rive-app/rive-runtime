/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/pls/pls.hpp"
#include "rive/pls/pls_factory.hpp"
#include "rive/pls/pls_render_target.hpp"
#include "rive/pls/trivial_block_allocator.hpp"
#include "rive/shapes/paint/color.hpp"
#include <unordered_map>

class PushRetrofittedTrianglesGMDraw;

namespace rive
{
class RawPath;
} // namespace rive

namespace rive::pls
{
class GradientLibrary;
class PLSDraw;
class PLSGradient;
class PLSPaint;
class PLSPath;
class PLSRenderContextImpl;

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
class PLSRenderContext : public PLSFactory
{
public:
    PLSRenderContext(std::unique_ptr<PLSRenderContextImpl>);
    ~PLSRenderContext();

    // Manually releases all references in the draw list and then resets it back to empty.
    void resetPLSDraws();

    PLSRenderContextImpl* impl() { return m_impl.get(); }
    template <typename T> T* static_impl_cast() { return static_cast<T*>(m_impl.get()); }

    // Returns the number of flushes that have been executed over the entire lifetime of this class,
    // intermediate or otherwise.
    uint64_t getFlushCount() const { return m_flushCount; }

    // Options for controlling how and where a frame is rendered.
    struct FrameDescriptor
    {
        rcp<const PLSRenderTarget> renderTarget;
        LoadAction loadAction = LoadAction::clear;
        ColorInt clearColor = 0;

        // Enables an experimental pixel local storage mode that makes use of atomics and barriers
        // instead of raster ordering in order to synchronize overlapping fragments.
        bool enableExperimentalAtomicMode = false;

        // Testing flags.
        bool wireframe = false;
        bool fillsDisabled = false;
        bool strokesDisabled = false;

        // Only used for metal backend.
        void* backendSpecificData = nullptr;
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

    bool pushDrawBatch(PLSDraw* draws[], size_t drawCount);

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
    TrivialBlockAllocator* perFrameAllocator()
    {
        assert(m_didBeginFrame);
        return &m_perFrameAllocator;
    }

    // Allocates a trivially destructible object that will be automatically deleted at the end of
    // the current flush.
    template <typename T, typename... Args> T* make(Args&&... args)
    {
        assert(m_didBeginFrame);
        return m_perFrameAllocator.make<T>(std::forward<Args>(args)...);
    }

    // Allocators for intermediate path processing buffers.
    TrivialArrayAllocator<uint8_t>& numChopsAllocator() { return m_numChopsAllocator; }
    TrivialArrayAllocator<Vec2D>& chopVerticesAllocator() { return m_chopVerticesAllocator; }
    TrivialArrayAllocator<std::array<Vec2D, 2>>& tangentPairsAllocator()
    {
        return m_tangentPairsAllocator;
    }
    TrivialArrayAllocator<uint32_t, alignof(float4)>& polarSegmentCountsAllocator()
    {
        return m_polarSegmentCountsAllocator;
    }
    TrivialArrayAllocator<uint32_t, alignof(float4)>& parametricSegmentCountsAllocator()
    {
        return m_parametricSegmentCountsAllocator;
    }

    // Simple linked list whose nodes are allocated on a context's TrivialBlockAllocator.
    template <typename T> class PerFlushLinkedList
    {
    public:
        size_t count() const;
        bool empty() const { return count() == 0; }
        T& tail() const;
        template <typename... Args> T& emplace_back(PLSRenderContext* context, Args... args);
        void reset();

        struct Node
        {
            template <typename... Args> Node(Args... args) : data(std::forward<Args>(args)...) {}
            T data;
            Node* next = nullptr;
        };

        template <typename U> class Iter
        {
        public:
            Iter(Node* current) : m_current(current) {}
            bool operator!=(const Iter& other) const { return m_current != other.m_current; }
            void operator++() { m_current = m_current->next; }
            U& operator*() { return m_current->data; }

        private:
            Node* m_current;
        };
        Iter<T> begin() { return {m_head}; }
        Iter<T> end() { return {nullptr}; }
        Iter<const T> begin() const { return {m_head}; }
        Iter<const T> end() const { return {nullptr}; }

    private:
        Node* m_head = nullptr;
        Node* m_tail = nullptr;
        size_t m_count = 0;
    };

    // Linked list of draws to be issued by the impl during onFlush().
    struct Draw
    {
        Draw(DrawType drawType_, uint32_t baseElement_) :
            drawType(drawType_), baseElement(baseElement_)
        {}

        const DrawType drawType;
        uint32_t baseElement;      // Base vertex, index, or instance.
        uint32_t elementCount = 0; // Vertex, index, or instance count. Calculated during flush().
        ShaderFeatures shaderFeatures = ShaderFeatures::NONE;
        const PLSTexture* imageTexture = nullptr;

        // DrawType::imageRect and DrawType::imageMesh.
        size_t imageDrawDataOffset;

        // DrawType::imageMesh.
        const RenderBuffer* vertexBuffer;
        const RenderBuffer* uvBuffer;
        const RenderBuffer* indexBuffer;
    };

    // Backend-specific PLSFactory implementation.
    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override;
    rcp<RenderImage> decodeImage(Span<const uint8_t>) override;

private:
    friend class PLSDraw;
    friend class PLSPathDraw;
    friend class MidpointFanPathDraw;
    friend class InteriorTriangulationDraw;
    friend class ImageRectDraw;
    friend class ImageMeshDraw;
    friend class ::PushRetrofittedTrianglesGMDraw; // For testing.

    // Resets the STL containers so they don't have unbounded growth.
    void resetContainers();

    // Running counts of objects that need to be allocated in our various GPU buffers.
    struct ResourceCounters
    {
        using VecType = simd::gvec<size_t, 8>;

        VecType toVec() const
        {
            static_assert(sizeof(VecType) == sizeof(*this));
            VecType vec;
            RIVE_INLINE_MEMCPY(&vec, this, sizeof(VecType));
            return vec;
        }

        ResourceCounters() = default;
        ResourceCounters(const VecType& vec)
        {
            static_assert(sizeof(*this) == sizeof(VecType));
            RIVE_INLINE_MEMCPY(this, &vec, sizeof(*this));
        }

        size_t midpointFanTessVertexCount = 0;
        size_t outerCubicTessVertexCount = 0;
        size_t pathCount = 0;
        size_t contourCount = 0;
        size_t tessellatedSegmentCount = 0; // lines, curves, standalone joins, emulated caps, etc.
        size_t maxTriangleVertexCount = 0;
        size_t imageDrawCount = 0; // imageRect or imageMesh.
        size_t complexGradientSpanCount = 0;
    };

    // Allocates a horizontal span of texels in the gradient texture and schedules either a texture
    // upload or a draw that fills it with the given gradient's color ramp.
    //
    // Fills out a PaintData record that tells the shader how to access the new gradient.
    //
    // Returns false if the gradient texture is out of space, at which point the caller must flush
    // before continuing.
    [[nodiscard]] bool allocateGradient(const PLSGradient*, ResourceCounters*, PaintData*);

    // Defines the exact size of each of our GPU resources. Computed during flush(), based on
    // ResourceCounters.
    struct ResourceAllocationSizes
    {
        using VecType = simd::gvec<size_t, 16>;

        VecType toVec() const
        {
            static_assert(sizeof(VecType) == sizeof(*this));
            VecType vec;
            RIVE_INLINE_MEMCPY(&vec, this, sizeof(VecType));
            return vec;
        }

        ResourceAllocationSizes() = default;
        ResourceAllocationSizes(const VecType& vec)
        {
            static_assert(sizeof(*this) == sizeof(VecType));
            RIVE_INLINE_MEMCPY(this, &vec, sizeof(*this));
        }

        size_t pathBufferCount = 0;
        size_t contourBufferCount = 0;
        size_t simpleGradientBufferCount = 0;
        size_t complexGradSpanBufferCount = 0;
        size_t tessSpanBufferCount = 0;
        size_t triangleVertexBufferCount = 0;
        size_t imageDrawUniformBufferCount = 0;
        size_t pathTextureHeight = 0;
        size_t contourTextureHeight = 0;
        size_t gradTextureHeight = 0;
        size_t tessTextureHeight = 0;
        size_t pad[5];
    };

    // Reallocates GPU resources and updates m_currentResourceAllocations.
    // If forceRealloc is true, every GPU resource is allocated, even if the size would not change.
    void setResourceSizes(ResourceAllocationSizes, bool forceRealloc = false);

    void mapResourceBuffers();
    void unmapResourceBuffers();

    // Writes padding vertices to the tessellation texture, with an invalid contour ID that is
    // guaranteed to not be the same ID as any neighbors.
    void pushPaddingVertices(uint32_t tessLocation, uint32_t count);

    // Pushes a record to the GPU for the given path, which will be referenced by future calls
    // to pushContour() and pushCubic().
    void pushPath(PatchType,
                  const Mat2D&,
                  float strokeRadius,
                  FillRule,
                  PaintType,
                  const PaintData&,
                  const PLSTexture* imageTexture,
                  uint32_t clipID,
                  const pls::ClipRectInverseMatrix*, // Null if there is no clipRect.
                  BlendMode,
                  uint32_t tessVertexCount);

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
                   uint32_t additionalContourFlags,
                   uint32_t parametricSegmentCount,
                   uint32_t polarSegmentCount,
                   uint32_t joinSegmentCount);

    // Pushes triangles to be drawn using the data records from the most recent calls to pushPath()
    // and pushPaint().
    void pushInteriorTriangulation(GrInnerFanTriangulator*,
                                   PaintType,
                                   const PaintData&,
                                   const PLSTexture* imageTexture,
                                   uint32_t clipID,
                                   bool hasClipRect,
                                   BlendMode);

    // Pushes an imageRect to the draw list.
    // This should only be used when we don't have bindless textures in atomic mode. Otherwise,
    // images should be drawn as rectangular paths with an image paint.
    void pushImageRect(const Mat2D&,
                       float opacity,
                       const PLSTexture* imageTexture,
                       uint32_t clipID,
                       const pls::ClipRectInverseMatrix*, // Null if there is no clipRect.
                       BlendMode);

    // Pushes an imageMesh to the draw list.
    void pushImageMesh(const Mat2D&,
                       float opacity,
                       const PLSTexture* imageTexture,
                       const RenderBuffer* vertexBuffer,
                       const RenderBuffer* uvBuffer,
                       const RenderBuffer* indexBuffer,
                       uint32_t indexCount,
                       uint32_t clipID,
                       const pls::ClipRectInverseMatrix*, // Null if there is no clipRect.
                       BlendMode);

    // Either appends a draw to m_drawList or merges into m_lastDraw.
    // Updates the draw's ShaderFeatures according to the passed parameters.
    void pushPathDraw(DrawType,
                      size_t baseVertex,
                      FillRule,
                      PaintType,
                      const PaintData&,
                      const PLSTexture* imageTexture,
                      uint32_t clipID,
                      bool hasClipRect,
                      BlendMode);
    void pushDraw(DrawType,
                  size_t baseVertex,
                  PaintType,
                  const PLSTexture* imageTexture,
                  uint32_t clipID,
                  bool hasClipRect,
                  BlendMode);

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

    // Same as pushTessellationSpans(), but also pushes a reflection of the span, rendered right to
    // left, that emits a mirrored version of the patch with negative coverage. (See
    // TessVertexSpan.)
    RIVE_ALWAYS_INLINE void pushMirroredTessellationSpans(const Vec2D pts[4],
                                                          Vec2D joinTangent,
                                                          uint32_t totalVertexCount,
                                                          uint32_t parametricSegmentCount,
                                                          uint32_t polarSegmentCount,
                                                          uint32_t joinSegmentCount,
                                                          uint32_t contourIDWithFlags);

    const std::unique_ptr<PLSRenderContextImpl> m_impl;
    const size_t m_maxPathID;

    // The number of flushes that have been executed over the entire lifetime of this class,
    // intermediate or otherwise.
    size_t m_flushCount = 0;

    ResourceAllocationSizes m_currentResourceAllocations;
    ResourceAllocationSizes m_maxRecentResourceRequirements;

    // Per-frame state.
    FrameDescriptor m_frameDescriptor;
    bool m_isFirstFlushOfFrame = true;
    RIVE_DEBUG_CODE(bool m_didBeginFrame = false;)

    // Clipping state.
    uint32_t m_lastGeneratedClipID = 0;
    uint32_t m_clipContentID = 0;

    // Running counts of objects in the current flush that need to be allocated in GPU buffers.
    ResourceCounters m_currentFlushCounters;

    // Simple gradients have one stop at t=0 and one stop at t=1. They're implemented with 2 texels.
    std::unordered_map<uint64_t, uint32_t> m_simpleGradients; // [color0, color1] -> rampTexelsIdx
    std::vector<pls::TwoTexelRamp> m_pendingSimpleGradientWrites;

    // Complex gradients have stop(s) between t=0 and t=1. In theory they should be scaled to a ramp
    // where every stop lands exactly on a pixel center, but for now we just always scale them to
    // the entire gradient texture width.
    std::unordered_map<GradientContentKey, uint32_t, DeepHashGradient>
        m_complexGradients; // [colors[0..n], stops[0..n]] -> rowIdx
    std::vector<const PLSGradient*> m_pendingComplexColorRampDraws;

    FlushUniforms m_currentFlushUniforms;

    std::vector<PLSDraw*> m_plsDraws;

    // Most recent path and contour state.
    bool m_currentPathIsStroked = false;
    bool m_currentPathNeedsMirroredContours = false;
    uint32_t m_currentPathID = 0;
    uint32_t m_currentContourID = 0;
    uint32_t m_currentContourPaddingVertexCount = 0; // Padding vertices to add to the first curve.
    uint32_t m_midpointFanTessVertexIdx = 0;
    uint32_t m_outerCubicTessVertexIdx = 0;
    uint32_t m_pathTessLocation = 0;
    uint32_t m_pathMirroredTessLocation = 0; // Used for back-face culling and mirrored patches.
    RIVE_DEBUG_CODE(uint32_t m_expectedPathTessLocationAtEndOfPath = 0;)
    RIVE_DEBUG_CODE(uint32_t m_expectedPathMirroredTessLocationAtEndOfPath = 0;)
    RIVE_DEBUG_CODE(uint32_t m_pathCurveCount = 0;)

    WriteOnlyMappedMemory<pls::PathData> m_pathData;
    WriteOnlyMappedMemory<pls::ContourData> m_contourData;
    // Simple gradients get written by the CPU.
    WriteOnlyMappedMemory<pls::TwoTexelRamp> m_simpleColorRampsData;
    // Complex gradients get rendered by the GPU.
    WriteOnlyMappedMemory<pls::GradientSpan> m_gradSpanData;
    WriteOnlyMappedMemory<pls::TessVertexSpan> m_tessSpanData;
    WriteOnlyMappedMemory<pls::TriangleVertex> m_triangleVertexData;
    WriteOnlyMappedMemory<pls::ImageDrawUniforms> m_imageDrawUniformData;

    PerFlushLinkedList<Draw> m_drawList;

    std::unique_ptr<pls::ExperimentalAtomicModeData> m_atomicModeData;

    // Simple allocator for trivially-destructible data that needs to persist until the current
    // flush has completed. Any object created with this allocator is automatically deleted during
    // the next call to flush().
    constexpr static size_t kPerFlushAllocatorInitialBlockSize = 1024 * 1024; // 1 MiB.
    TrivialBlockAllocator m_perFrameAllocator{kPerFlushAllocatorInitialBlockSize};

    // Allocators for intermediate path processing buffers.
    constexpr static size_t kIntermediateDataInitialStrokes = 8192;     // * 84 == 688 KiB.
    constexpr static size_t kIntermediateDataInitialFillCurves = 32768; // * 4 == 128 KiB.
    TrivialArrayAllocator<uint8_t> m_numChopsAllocator{kIntermediateDataInitialStrokes *
                                                       4}; // 4 byte per stroke curve.
    TrivialArrayAllocator<Vec2D> m_chopVerticesAllocator{kIntermediateDataInitialStrokes *
                                                         4}; // 32 bytes per stroke curve.
    TrivialArrayAllocator<std::array<Vec2D, 2>> m_tangentPairsAllocator{
        kIntermediateDataInitialStrokes * 2}; // 32 bytes per stroke curve.
    TrivialArrayAllocator<uint32_t, alignof(float4)> m_polarSegmentCountsAllocator{
        kIntermediateDataInitialStrokes * 4}; // 16 bytes per stroke curve.
    TrivialArrayAllocator<uint32_t, alignof(float4)> m_parametricSegmentCountsAllocator{
        kIntermediateDataInitialFillCurves}; // 4 bytes per fill curve.
};

template <typename T> size_t PLSRenderContext::PerFlushLinkedList<T>::count() const
{
    assert(!!m_head == !!m_tail);
    assert(!!m_tail == !!m_count);
    return m_count;
}

template <typename T> T& PLSRenderContext::PerFlushLinkedList<T>::tail() const
{
    assert(!empty());
    return m_tail->data;
}

template <typename T>
template <typename... Args>
T& PLSRenderContext::PerFlushLinkedList<T>::emplace_back(PLSRenderContext* context, Args... args)
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
    ++m_count;
    return m_tail->data;
}

template <typename T> void PLSRenderContext::PerFlushLinkedList<T>::reset()
{
    m_tail = nullptr;
    m_head = nullptr;
    m_count = 0;
}
} // namespace rive::pls

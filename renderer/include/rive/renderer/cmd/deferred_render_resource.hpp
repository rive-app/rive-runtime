/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer.hpp"
#include "rive/command_path.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/mat2d.hpp"
#include "utils/lite_rtti.hpp"
#include "rive/renderer/cmd/render_command_buffer.hpp"
#include "rive/renderer/cmd/render_commands.hpp"
#include "rive/renderer/cmd/id_allocator.hpp"
#include "rive/renderer/cmd/live_recorder_registry.hpp"
#include <cstdio>
#include <memory>
#include <mutex>

// Deferred 2D resources: lite_rtti subclasses of the real Factory types that
// carry a dense creation id and record their mutations into a shared
// RenderCommandBuffer. The render side recreates the real resource from the
// stream and replays the mutations in order.
namespace rive::cmd
{

// The destroy lands in stream order after the draws that referenced the id,
// making reuse safe. A release after the session died no-ops.
inline void releaseDeferred(RenderCommandBuffer* commands,
                            IdAllocator<RenderHandle>* allocator,
                            ResourceKind kind,
                            RenderHandle id,
                            uint32_t generation)
{
    std::lock_guard<std::mutex> lock(recorderRegistryMutex());
    if (liveRecorders().count(commands) == 0)
    {
        return; // the session died first, nothing to record into
    }
    // Destructors run on any thread, so queue rather than append; the
    // recording thread drains at the frame boundary.
    commands->queueDestroy(static_cast<uint8_t>(kind),
                           id,
                           generation,
                           allocator);
}

// Common deferred resource state: the dense creation id other streams
// reference it by, and the destroy recorded at destruction.
class DeferredResourceBase
{
public:
    DeferredResourceBase(ResourceKind kind,
                         RenderHandle id,
                         uint32_t generation,
                         RenderCommandBuffer* buffer,
                         IdAllocator<RenderHandle>* allocator) :
        m_id(id),
        m_buffer(buffer),
        m_generation(generation),
        m_allocator(allocator),
        m_kind(kind)
    {}
    RenderHandle id() const { return m_id; }

protected:
    ~DeferredResourceBase()
    {
        releaseDeferred(m_buffer, m_allocator, m_kind, m_id, m_generation);
    }

    RenderHandle m_id;
    RenderCommandBuffer* m_buffer;

    ResourceKind kind() const { return m_kind; }

private:
    uint32_t m_generation;
    IdAllocator<RenderHandle>* m_allocator;
    ResourceKind m_kind;
};

// Adds the drawn-version pinning: draws pin the version they recorded
// against, so a mutation of a resource drawn this frame bumps to a new
// version; the first mutation of a new frame reuses the live replay object.
class VersionedDeferredResource : public DeferredResourceBase
{
public:
    using DeferredResourceBase::DeferredResourceBase;

    uint32_t version() const { return m_version; }
    void markDrawn() { m_drawnFrame = m_buffer->frameId(); }

protected:
    void bump()
    {
        if (m_drawnFrame != m_buffer->frameId())
        {
            return; // not drawn this frame: mutate the live object in place
        }
        m_version++;
        m_drawnFrame = kNeverDrawn;
        m_buffer->append(
            static_cast<uint8_t>(RenderCmd::resourceNewVersion),
            ResourceVersionPOD{static_cast<uint8_t>(kind()), m_id, m_version});
    }

private:
    uint32_t m_version = 0;
    static constexpr uint32_t kNeverDrawn = ~0u;
    uint32_t m_drawnFrame = kNeverDrawn;
};

// RenderShader is only ever a gradient, so a deferred shader is just an id.
class DeferredRenderShader
    : public LITE_RTTI_OVERRIDE(RenderShader, DeferredRenderShader),
      public DeferredResourceBase
{
public:
    DeferredRenderShader(RenderHandle id,
                         uint32_t generation,
                         RenderCommandBuffer* commands,
                         IdAllocator<RenderHandle>* allocator) :
        DeferredResourceBase(ResourceKind::shader,
                             id,
                             generation,
                             commands,
                             allocator)
    {}
};

class DeferredRenderPaint
    : public LITE_RTTI_OVERRIDE(RenderPaint, DeferredRenderPaint),
      public VersionedDeferredResource
{
public:
    DeferredRenderPaint(RenderHandle id,
                        uint32_t generation,
                        RenderCommandBuffer* buffer,
                        IdAllocator<RenderHandle>* allocator) :
        VersionedDeferredResource(ResourceKind::paint,
                                  id,
                                  generation,
                                  buffer,
                                  allocator)
    {}

    void style(RenderPaintStyle v) override
    {
        if (absorbed(m_state.style, static_cast<uint8_t>(v)))
        {
            return;
        }
        emitU8(RenderCmd::paintStyle, m_state.style);
    }
    void color(ColorInt v) override
    {
        if (absorbed(m_state.color, v) && m_colorKnown)
        {
            return;
        }
        m_colorKnown = true;
        bump();
        m_buffer->append(t(RenderCmd::paintColor),
                         PaintColorPOD{m_id, m_state.color});
    }
    void thickness(float v) override
    {
        if (absorbed(m_state.thickness, v))
        {
            return;
        }
        emitFloat(RenderCmd::paintThickness, m_state.thickness);
    }
    void join(StrokeJoin v) override
    {
        if (absorbed(m_state.join, static_cast<uint8_t>(v)))
        {
            return;
        }
        emitU8(RenderCmd::paintJoin, m_state.join);
    }
    void cap(StrokeCap v) override
    {
        if (absorbed(m_state.cap, static_cast<uint8_t>(v)))
        {
            return;
        }
        emitU8(RenderCmd::paintCap, m_state.cap);
    }
    void feather(float v) override
    {
        if (absorbed(m_state.feather, v))
        {
            return;
        }
        emitFloat(RenderCmd::paintFeather, m_state.feather);
    }
    void blendMode(BlendMode v) override
    {
        if (absorbed(m_state.blendMode, static_cast<uint8_t>(v)))
        {
            return;
        }
        emitU8(RenderCmd::paintBlendMode, m_state.blendMode);
    }
    void shader(
        rcp<RenderShader> s) override; // defined below (needs the helper)
    void invalidateStroke() override
    {
        // Stroked shapes invalidate every frame their path moves, and the
        // consumer only rebuilds the stroke when it draws, so repeats before
        // the next draw say nothing new. It carries no state either, so it
        // must not drag a version bump and a state rewrite behind it.
        if (m_strokeInvalidated)
        {
            return;
        }
        m_strokeInvalidated = true;
        m_buffer->append(t(RenderCmd::paintInvalidateStroke), ResIdPOD{m_id});
    }

    void markDrawn()
    {
        VersionedDeferredResource::markDrawn();
        m_strokeInvalidated = false;
    }

private:
    // Must be the backend's defaults, because a property nothing ever changes
    // away from them is never written and the replay object keeps its own.
    struct State
    {
        ColorInt color = 0xFF000000;
        float thickness = 1;
        float feather = 0;
        uint8_t style = 1;     // fill; a fresh paint is unstroked until told
        uint8_t join = 0;      // miter
        uint8_t cap = 0;       // butt
        uint8_t blendMode = 3; // srcOver
    };

    static uint8_t t(RenderCmd c) { return static_cast<uint8_t>(c); }
    // Absorbs the new value into the shadow; true when the consumer already
    // has it and the append can be skipped.
    template <typename T> bool absorbed(T& field, T v)
    {
        if (field == v)
        {
            return true;
        }
        field = v;
        return false;
    }
    void emitU8(RenderCmd c, uint8_t v)
    {
        bump();
        m_buffer->append(t(c), PaintU8POD{m_id, v});
    }
    void emitFloat(RenderCmd c, float v)
    {
        bump();
        m_buffer->append(t(c), PaintFloatPOD{m_id, v});
    }

    State m_state;
    // Held as an rcp so an animated gradient outlives the setter: comparing
    // ids alone would alias a recycled one.
    rcp<RenderShader> m_shader;
    bool m_colorKnown = true;
    bool m_strokeInvalidated = false;
};

class DeferredRenderPath
    : public LITE_RTTI_OVERRIDE(RenderPath, DeferredRenderPath),
      public VersionedDeferredResource
{
public:
    DeferredRenderPath(RenderHandle id,
                       uint32_t generation,
                       RenderCommandBuffer* buffer,
                       IdAllocator<RenderHandle>* allocator) :
        VersionedDeferredResource(ResourceKind::path,
                                  id,
                                  generation,
                                  buffer,
                                  allocator)
    {}

    void rewind() override
    {
        bump();
        m_scratch.rewind(); // discards any pending per-verb geometry
#ifdef WITH_RIVE_PATH_QUERY
        if (m_query != nullptr)
        {
            m_query->rewind();
        }
#endif
        m_buffer->append(t(RenderCmd::pathRewind), ResIdPOD{m_id});
    }
    void fillRule(FillRule v) override
    {
        // ShapePaint re-sets the fill rule before every fill draw; a no-op
        // set must not bump a drawn path to a new version.
        if (m_haveFillRule && v == m_fillRule)
        {
            return;
        }
        m_haveFillRule = true;
        m_fillRule = v;
        bump();
        m_buffer->append(t(RenderCmd::pathFillRule),
                         PathFillRulePOD{m_id, static_cast<uint8_t>(v)});
    }
    FillRule fillRule() const { return m_fillRule; }

    // Where a host RawPath re-enters us, since RawPath::addTo fans out to
    // these. Accumulate into a scratch RawPath flushed on next use, dropping
    // zero length segments like RiveRenderPath so replay draws the same thing.
    void moveTo(float x, float y) override
    {
        m_scratch.moveTo(x, y);
        queryMirror([=](RawPath& q) { q.moveTo(x, y); });
    }
    void lineTo(float x, float y) override
    {
        appendLine(m_scratch, {x, y});
        queryMirror([=](RawPath& q) { appendLine(q, {x, y}); });
    }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override
    {
        appendCubic(m_scratch, {ox, oy}, {ix, iy}, {x, y});
        queryMirror(
            [=](RawPath& q) { appendCubic(q, {ox, oy}, {ix, iy}, {x, y}); });
    }
    void close() override
    {
        m_scratch.close();
        queryMirror([](RawPath& q) { q.close(); });
    }

    void addRenderPath(const RenderPath* path, const Mat2D& m) override
    {
        bump();
        flushScratch();
        flushScratchOf(path); // src geometry must be complete in the stream
        RenderHandle src = idOfPath(path);
        m_buffer->append(t(RenderCmd::pathAddRenderPath),
                         PathAddPathPOD{m_id,
                                        src,
                                        m.xx(),
                                        m.xy(),
                                        m.yx(),
                                        m.yy(),
                                        m.tx(),
                                        m.ty()});
#ifdef WITH_RIVE_PATH_QUERY
        if (m_query != nullptr)
        {
            auto* d = lite_rtti_cast<DeferredRenderPath*>(
                const_cast<RenderPath*>(path));
            if (const RawPath* srcQuery = d ? d->queryRawPath() : nullptr)
            {
                if (d == this)
                {
                    // Adding a path into itself reads the vector it appends.
                    RawPath copy(*srcQuery);
                    m_query->addPath(copy, &m);
                }
                else
                {
                    m_query->addPath(*srcQuery, &m);
                }
            }
        }
#endif
    }
    void addRawPath(const RawPath& path) override
    {
        flushScratch(); // preserve order: pending per-verb before this bulk add
        recordAddRawPath(path);
        queryMirror([&](RawPath& q) { q.addPath(path); });
    }

#ifdef WITH_RIVE_PATH_QUERY
    // Opt in per path: hosts that hit test or measure what they record call
    // this at creation; file loads never do, so their paths stay twin free.
    // A seed refreshes rather than appends, effect paths re-hand the same
    // live pointer.
    void retainQueryGeometry(const RawPath* seed = nullptr)
    {
        if (m_query == nullptr)
        {
            m_query = std::make_unique<RawPath>();
        }
        if (seed != nullptr)
        {
            m_query->rewind();
            m_query->addPath(*seed);
        }
    }
    const RawPath* queryRawPath() const { return m_query.get(); }
#endif

    // Emit any pending per verb geometry as one addRawPath. Public so the
    // renderer can flush before a draw or clip.
    void flushScratch()
    {
        if (m_scratch.empty())
        {
            return;
        }
        recordAddRawPath(m_scratch);
        m_scratch.rewind();
    }
    static void flushScratchOf(const RenderPath* p)
    {
        if (auto* d =
                lite_rtti_cast<DeferredRenderPath*>(const_cast<RenderPath*>(p)))
        {
            d->flushScratch();
        }
    }

    // kInvalid when the path is not one of ours.
    static RenderHandle idOfPath(const RenderPath* p)
    {
        auto* d =
            lite_rtti_cast<DeferredRenderPath*>(const_cast<RenderPath*>(p));
        return d ? d->id() : kInvalidRenderHandle;
    }

private:
    static uint8_t t(RenderCmd c) { return static_cast<uint8_t>(c); }

    // Start the contour even when the segment itself is empty, matching
    // RiveRenderPath.
    static void appendLine(RawPath& path, Vec2D p1)
    {
        path.injectImplicitMoveIfNeeded();
        if (path.points().back() != p1)
        {
            path.line(p1);
        }
    }
    static void appendCubic(RawPath& path, Vec2D p1, Vec2D p2, Vec2D p3)
    {
        path.injectImplicitMoveIfNeeded();
        if (path.points().back() != p1 || p1 != p2 || p2 != p3)
        {
            path.cubic(p1, p2, p3);
        }
    }
    // Replay seeds a bumped path version from the outgoing one, so appends
    // land on prior geometry and a rewind clears the seed via its own
    // recorded command.
    void recordAddRawPath(const RawPath& path)
    {
        bump();
        auto verbs = path.verbs();
        auto points = path.points();
        uint64_t verbsOff = m_buffer->appendBlob(
            verbs.data(),
            static_cast<uint32_t>(verbs.size() * sizeof(PathVerb)));
        uint64_t pointsOff = m_buffer->appendBlob(
            points.data(),
            static_cast<uint32_t>(points.size() * sizeof(Vec2D)));
        m_buffer->append(t(RenderCmd::pathAddRawPath),
                         PathRawPOD{verbsOff,
                                    pointsOff,
                                    m_id,
                                    static_cast<uint32_t>(verbs.size()),
                                    static_cast<uint32_t>(points.size())});
    }

#ifdef WITH_RIVE_PATH_QUERY
    template <typename F> void queryMirror(F&& f)
    {
        if (m_query != nullptr)
        {
            f(*m_query);
        }
    }
    std::unique_ptr<RawPath> m_query;
#else
    template <typename F> void queryMirror(F&&) {}
#endif

    RawPath m_scratch; // pending CommandPath per-verb geometry
    FillRule m_fillRule = FillRule::nonZero;
    bool m_haveFillRule = false;
};

inline void DeferredRenderPaint::shader(rcp<RenderShader> s)
{
    if (m_shader.get() == s.get())
    {
        return;
    }
    m_shader = std::move(s);
    RenderHandle id = kInvalidRenderHandle; // null clears the shader
    if (auto* d = lite_rtti_cast<DeferredRenderShader*>(m_shader.get()))
    {
        id = d->id();
    }
    // Backends are free to disturb the solid color when the shader moves
    // (RiveRenderPaint does), so stop trusting the shadowed one.
    m_colorKnown = false;
    bump();
    m_buffer->append(t(RenderCmd::paintShader), PaintShaderPOD{m_id, id});
}

// Carries dims decoded at record time so the artboard can read them during
// advance; the real GPU image is uploaded on the render side.
class DeferredRenderImage
    : public LITE_RTTI_OVERRIDE(RenderImage, DeferredRenderImage),
      public DeferredResourceBase
{
public:
    DeferredRenderImage(RenderHandle id,
                        uint32_t generation,
                        int width,
                        int height,
                        RenderCommandBuffer* commands,
                        IdAllocator<RenderHandle>* allocator) :
        DeferredResourceBase(ResourceKind::image,
                             id,
                             generation,
                             commands,
                             allocator)
    {
        m_Width = width;
        m_Height = height;
    }
};

// map() hands out a scratch buffer; unmap() records its bytes as a bufferData
// command replayed on the render side.
class DeferredRenderBuffer
    : public LITE_RTTI_OVERRIDE(RenderBuffer, DeferredRenderBuffer),
      public VersionedDeferredResource
{
public:
    DeferredRenderBuffer(RenderHandle id,
                         uint32_t generation,
                         RenderBufferType type,
                         RenderBufferFlags flags,
                         size_t sizeInBytes,
                         RenderCommandBuffer* buffer,
                         IdAllocator<RenderHandle>* allocator) :
        LITE_RTTI_OVERRIDE(RenderBuffer,
                           DeferredRenderBuffer)(type, flags, sizeInBytes),
        VersionedDeferredResource(ResourceKind::buffer,
                                  id,
                                  generation,
                                  buffer,
                                  allocator)
    {}

protected:
    void* onMap() override
    {
        m_scratch.resize(sizeInBytes());
        return m_scratch.data();
    }
    void onUnmap() override
    {
        bump();
        uint64_t off =
            m_buffer->appendBlob(m_scratch.data(),
                                 static_cast<uint32_t>(m_scratch.size()));
        m_buffer->append(
            static_cast<uint8_t>(RenderCmd::bufferData),
            BufferDataPOD{off, m_id, static_cast<uint32_t>(m_scratch.size())});
    }

private:
    std::vector<uint8_t> m_scratch;
};

} // namespace rive::cmd

/*
 * Copyright 2026 Rive
 */

// Measurement harness for the draw-time serialization work. Deliberately
// source identical between the pre-refactor and post-refactor trees so the
// two sides can be compared opcode for opcode and microsecond for
// microsecond. Emits machine readable rows; nothing here asserts on a
// threshold.
//
// Hidden tag; run explicitly with test.sh -m "[deferred_measure]".
//
// Environment:
//   RIVE_MEASURE_FRAMES   total frames per riv           (default 3000)
//   RIVE_MEASURE_WARMUP   frames treated as transient    (default 300)
//   RIVE_MEASURE_RIVS     comma separated riv names, or "all" for the
//                         whole zzzgold corpus           (default: a
//                         6 riv short list)
//   RIVE_MEASURE_SESSIONS concurrent sessions for the resident table
//                         sizing case                    (default 8)

#include "rive/renderer/cmd/deferred_render_factory.hpp"
#include "rive/renderer/cmd/deferred_render_resource.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/cmd/render_commands.hpp"
#include "rive/renderer/cmd/render_replay.hpp"
#include "rive_file_reader.hpp"
#include "rive/scene.hpp"
#include "utils/factory_utils.hpp"
#include "utils/no_op_renderer.hpp"

#include <catch.hpp>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

using namespace rive;
using namespace rive::cmd;

namespace
{
// ---- configuration ----

int envInt(const char* name, int fallback)
{
    const char* v = getenv(name);
    return v != nullptr && *v != '\0' ? atoi(v) : fallback;
}

const char* kRivDir = "../../../../zzzgold/rivs/";

std::vector<std::string> corpus()
{
    const char* v = getenv("RIVE_MEASURE_RIVS");
    std::string spec = v != nullptr && *v != '\0' ? v : "";
    std::vector<std::string> names;
    if (spec == "all")
    {
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(kRivDir, ec))
        {
            std::string n = e.path().filename().string();
            if (n.size() > 4 && n.compare(n.size() - 4, 4, ".riv") == 0)
            {
                names.push_back(n);
            }
        }
        std::sort(names.begin(), names.end());
        return names;
    }
    if (spec.empty())
    {
        return {"Halloween_v3.riv",
                "UI_Swipe_left_to_delete.riv",
                "Tom_Morello.riv",
                "Knight_square_2.riv",
                "falling.riv",
                "popsicle_loader.riv"};
    }
    size_t start = 0;
    while (start <= spec.size())
    {
        size_t comma = spec.find(',', start);
        if (comma == std::string::npos)
        {
            comma = spec.size();
        }
        if (comma > start)
        {
            names.push_back(spec.substr(start, comma - start));
        }
        start = comma + 1;
    }
    return names;
}

// ---- sink ----

// No-op resources: replay mutates them freely and the factory counts
// creations, so consumer side object churn per frame is observable.
class MPath : public RenderPath
{
public:
    void rewind() override {}
    void fillRule(FillRule) override {}
    void addPath(CommandPath*, const Mat2D&) override {}
    void addRenderPath(const RenderPath*, const Mat2D&) override {}
    void addRawPath(const RawPath&) override {}
    void moveTo(float, float) override {}
    void lineTo(float, float) override {}
    void cubicTo(float, float, float, float, float, float) override {}
    void close() override {}
};

class MPaint : public RenderPaint
{
public:
    void color(unsigned int) override {}
    void style(RenderPaintStyle) override {}
    void thickness(float) override {}
    void join(StrokeJoin) override {}
    void cap(StrokeCap) override {}
    void blendMode(BlendMode) override {}
    void shader(rcp<RenderShader>) override {}
    void invalidateStroke() override {}
    void feather(float) override {}
};

class MShader : public RenderShader
{};
class MImage : public RenderImage
{};

class MFactory : public Factory
{
public:
    int paths = 0, paints = 0, shaders = 0, buffers = 0, images = 0;

    rcp<RenderPath> makeRenderPath(RawPath&, FillRule) override
    {
        paths++;
        return make_rcp<MPath>();
    }
    rcp<RenderPath> makeEmptyRenderPath() override
    {
        paths++;
        return make_rcp<MPath>();
    }
    rcp<RenderPaint> makeRenderPaint() override
    {
        paints++;
        return make_rcp<MPaint>();
    }
    rcp<RenderShader> makeLinearGradient(float,
                                         float,
                                         float,
                                         float,
                                         const ColorInt[],
                                         const float[],
                                         size_t) override
    {
        shaders++;
        return make_rcp<MShader>();
    }
    rcp<RenderShader> makeRadialGradient(float,
                                         float,
                                         float,
                                         const ColorInt[],
                                         const float[],
                                         size_t) override
    {
        shaders++;
        return make_rcp<MShader>();
    }
    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType t,
                                       RenderBufferFlags f,
                                       size_t s) override
    {
        buffers++;
        return make_rcp<DataRenderBuffer>(t, f, s);
    }
    rcp<RenderImage> decodeImage(Span<const uint8_t>) override
    {
        images++;
        return make_rcp<MImage>();
    }
};

class MSink : public DeferredFrameSink
{
public:
    MFactory f;
    Factory* factory() override { return &f; }
    ore::Context* oreContext() override { return nullptr; }
    // The harness measures one session against one screen, and a no-op
    // renderer has nothing to dispatch per target anyway.
    Renderer* beginScreenFrame(uint64_t target) override
    {
        REQUIRE(target == 0);
        return &m_renderer;
    }

private:
    NoOpRenderer m_renderer;
};

// ---- retained geometry, present only on the post-refactor tree ----

template <typename T, typename = void> struct HasRetained : std::false_type
{};
template <typename T>
struct HasRetained<T, decltype(void(T::retainedGeometryBytes()))>
    : std::true_type
{};

// Templated so the branch the tree does not have is never looked up: an
// if constexpr in a plain function still requires both arms to name real
// members.
template <typename Path = DeferredRenderPath> int64_t retainedGeometry()
{
    if constexpr (HasRetained<Path>::value)
    {
        return Path::retainedGeometryBytes();
    }
    else
    {
        // The pre-refactor path serializes each mutation straight into the
        // stream and holds no authoritative geometry, so there is nothing to
        // report and no counter to read.
        return -2;
    }
}

// ---- stream census ----

constexpr size_t kNumCmds = static_cast<size_t>(RenderCmd::lastRenderCmd) + 1;

const char* cmdName(size_t i)
{
    switch (static_cast<RenderCmd>(i))
    {
#define RIVE_MEASURE_CMD_NAME(cmd, POD)                                        \
    case RenderCmd::cmd:                                                       \
        return #cmd;
        RIVE_RENDER_CMD_TABLE(RIVE_MEASURE_CMD_NAME)
#undef RIVE_MEASURE_CMD_NAME
    }
    return "?";
}

struct Census
{
    uint64_t count[kNumCmds] = {};
    uint64_t geomBytes = 0;
    uint64_t commandBytes = 0, blobBytes = 0;
    uint64_t frames = 0;
    bool overrun = false;

    void add(const RenderCommandBuffer& buf)
    {
        frames++;
        commandBytes += buf.commandBytes().size();
        blobBytes += buf.blobBytes().size();
        CommandReader<uint8_t> r(buf.commandBytes(), buf.blobBytes());
        uint8_t type;
        while (r.next(type))
        {
            if (type >= kNumCmds)
            {
                overrun = true;
                break;
            }
            RenderCmd cmd = static_cast<RenderCmd>(type);
            count[type]++;
            switch (cmd)
            {
                case RenderCmd::makePath:
                {
                    auto c = r.read<MakePathPOD>();
                    geomBytes += c.verbCount * sizeof(PathVerb) +
                                 c.pointCount * sizeof(Vec2D);
                    break;
                }
                case RenderCmd::pathAddRawPath:
                {
                    auto c = r.read<PathRawPOD>();
                    geomBytes += c.verbCount * sizeof(PathVerb) +
                                 c.pointCount * sizeof(Vec2D);
                    break;
                }
                default:
                    r.skip(payloadSizeOf(cmd));
                    break;
            }
        }
        overrun |= r.overrun();
    }
};

// ---- one riv ----

struct Phase
{
    double advanceUs = 0, recordUs = 0, snapshotUs = 0, replayUs = 0;
    uint64_t frames = 0;
    Census census;
    int64_t sinkPaths = 0, sinkPaints = 0, sinkShaders = 0, sinkBuffers = 0,
            sinkImages = 0;
};

void row(const char* riv, const char* phase, const char* metric, double value)
{
    printf("MEASURE,%s,%s,%s,%.6f\n", riv, phase, metric, value);
}

void emit(const char* riv, const char* name, const Phase& p)
{
    if (p.frames == 0)
    {
        return;
    }
    double n = static_cast<double>(p.frames);
    row(riv, name, "frames", n);
    row(riv, name, "advance_us", p.advanceUs / n);
    row(riv, name, "record_us", p.recordUs / n);
    row(riv, name, "snapshot_us", p.snapshotUs / n);
    row(riv, name, "replay_us", p.replayUs / n);
    row(riv, name, "cmd_bytes", p.census.commandBytes / n);
    row(riv, name, "blob_bytes", p.census.blobBytes / n);
    row(riv,
        name,
        "stream_bytes",
        (p.census.commandBytes + p.census.blobBytes) / n);
    row(riv, name, "geom_bytes", p.census.geomBytes / n);
    row(riv, name, "sink_paths", p.sinkPaths / n);
    row(riv, name, "sink_paints", p.sinkPaints / n);
    row(riv, name, "sink_shaders", p.sinkShaders / n);
    row(riv, name, "sink_buffers", p.sinkBuffers / n);
    row(riv, name, "sink_images", p.sinkImages / n);
    for (size_t c = 0; c < kNumCmds; c++)
    {
        if (p.census.count[c] != 0)
        {
            std::string m = std::string("op_") + cmdName(c);
            row(riv, name, m.c_str(), p.census.count[c] / n);
        }
    }
}

void measureRiv(const std::string& name, int frames, int warmup)
{
    std::string path = std::string(kRivDir) + name;
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == nullptr)
    {
        printf("MEASURE_SKIP,%s,missing\n", name.c_str());
        return;
    }
    fclose(fp);

    DeferredSession session(rive::ore::ReplayCaps{});
    auto file = ReadRiveFile(path.c_str(), &session);
    if (file == nullptr)
    {
        printf("MEASURE_SKIP,%s,undecodable\n", name.c_str());
        return;
    }
    auto artboard = file->artboardDefault();
    if (artboard == nullptr)
    {
        printf("MEASURE_SKIP,%s,no_artboard\n", name.c_str());
        return;
    }
    auto scene = artboard->defaultScene();

    MSink sink;
    DeferredReplayer replayer;
    Phase first, transient, steady;
    int64_t retainedAtSteady = -3;
    uint32_t dropped = 0;

    for (int frame = 0; frame < frames; frame++)
    {
        Phase* p = frame == 0 ? &first : frame < warmup ? &transient : &steady;
        float seconds = frame == 0 ? 0.f : 1.f / 60.f;

        auto t0 = std::chrono::steady_clock::now();
        if (scene != nullptr)
        {
            scene->advanceAndApply(seconds);
        }
        else
        {
            artboard->advance(seconds);
        }
        auto t1 = std::chrono::steady_clock::now();
        Renderer* renderer = session.screenRenderer();
        renderer->save();
        if (scene != nullptr)
        {
            scene->draw(renderer);
        }
        else
        {
            artboard->draw(renderer);
        }
        renderer->restore();
        auto t2 = std::chrono::steady_clock::now();

        p->census.add(session.commandBuffer());

        DeferredFrame snapshot = snapshotFrame(session);
        session.resetFrame();
        auto t3 = std::chrono::steady_clock::now();

        MFactory& f = sink.f;
        int paths = f.paths, paints = f.paints, shaders = f.shaders,
            buffers = f.buffers, images = f.images;
        replayer.replayFrame(snapshot, sink);
        auto t4 = std::chrono::steady_clock::now();
        dropped += replayer.droppedDraws();

        auto us = [](auto a, auto b) {
            return std::chrono::duration<double, std::micro>(b - a).count();
        };
        p->advanceUs += us(t0, t1);
        p->recordUs += us(t1, t2);
        p->snapshotUs += us(t2, t3);
        p->replayUs += us(t3, t4);
        p->frames++;
        p->sinkPaths += f.paths - paths;
        p->sinkPaints += f.paints - paints;
        p->sinkShaders += f.shaders - shaders;
        p->sinkBuffers += f.buffers - buffers;
        p->sinkImages += f.images - images;

        if (frame == frames - 1)
        {
            retainedAtSteady = retainedGeometry();
        }
    }

    emit(name.c_str(), "first", first);
    emit(name.c_str(), "transient", transient);
    emit(name.c_str(), "steady", steady);
    row(name.c_str(),
        "run",
        "retained_geometry_bytes",
        static_cast<double>(retainedAtSteady));
    row(name.c_str(), "run", "dropped_draws", static_cast<double>(dropped));
    row(name.c_str(),
        "run",
        "stream_overrun",
        first.census.overrun || transient.census.overrun ||
                steady.census.overrun
            ? 1
            : 0);

    // Consumer resident tables after the run: how far the dense vectors had
    // to grow, which is what process wide ids trade against.
    const ResourceTable& t = replayer.table();
    auto live = [](const auto& r) {
        size_t n = 0;
        for (const auto& o : r.objects)
        {
            n += o != nullptr ? 1 : 0;
        }
        return static_cast<double>(n);
    };
    row(name.c_str(),
        "resident",
        "path_slots",
        static_cast<double>(t.paths.objects.size()));
    row(name.c_str(), "resident", "path_live", live(t.paths));
    row(name.c_str(),
        "resident",
        "paint_slots",
        static_cast<double>(t.paints.objects.size()));
    row(name.c_str(), "resident", "paint_live", live(t.paints));
    row(name.c_str(),
        "resident",
        "shader_slots",
        static_cast<double>(t.shaders.objects.size()));
    row(name.c_str(), "resident", "shader_live", live(t.shaders));
    row(name.c_str(),
        "resident",
        "image_slots",
        static_cast<double>(t.images.objects.size()));
    row(name.c_str(),
        "resident",
        "buffer_slots",
        static_cast<double>(t.buffers.objects.size()));
    row(name.c_str(), "resident", "buffer_live", live(t.buffers));
    // Bytes the dense vectors themselves occupy, ignoring the objects.
    double slotBytes = static_cast<double>(t.paths.objects.size()) *
                           (sizeof(rcp<RenderPath>) + 2 * sizeof(uint32_t)) +
                       static_cast<double>(t.paints.objects.size()) *
                           (sizeof(rcp<RenderPaint>) + 2 * sizeof(uint32_t)) +
                       static_cast<double>(t.shaders.objects.size()) *
                           (sizeof(rcp<RenderShader>) + 2 * sizeof(uint32_t)) +
                       static_cast<double>(t.images.objects.size()) *
                           (sizeof(rcp<RenderImage>) + 2 * sizeof(uint32_t)) +
                       static_cast<double>(t.buffers.objects.size()) *
                           (sizeof(rcp<RenderBuffer>) + 2 * sizeof(uint32_t));
    row(name.c_str(), "resident", "slot_vector_bytes", slotBytes);
}
} // namespace

TEST_CASE("deferred measure", "[.][deferred_measure]")
{
    int frames = envInt("RIVE_MEASURE_FRAMES", 3000);
    int warmup = envInt("RIVE_MEASURE_WARMUP", 300);
    printf("MEASURE_CONFIG,frames,%d,warmup,%d\n", frames, warmup);
    printf("MEASURE_CONFIG,retained_instrumented,%d\n",
           HasRetained<DeferredRenderPath>::value ? 1 : 0);
    for (const std::string& name : corpus())
    {
        measureRiv(name, frames, warmup);
    }
}

// Several sessions live at once, each drawing its own riv, so the consumer
// resident vectors size to the process wide id high water rather than to any
// one session's own resources.
TEST_CASE("deferred measure concurrent sessions", "[.][deferred_measure]")
{
    int sessions = envInt("RIVE_MEASURE_SESSIONS", 8);
    int frames = std::max(4, envInt("RIVE_MEASURE_FRAMES", 3000) / 100);
    std::vector<std::string> names = corpus();
    if (names.empty())
    {
        return;
    }

    struct Live
    {
        std::unique_ptr<DeferredSession> session;
        rcp<File> file;
        std::unique_ptr<ArtboardInstance> artboard;
        std::unique_ptr<Scene> scene;
        MSink sink;
        DeferredReplayer replayer;
    };
    std::vector<std::unique_ptr<Live>> live;
    for (int i = 0; i < sessions; i++)
    {
        const std::string& name = names[i % names.size()];
        std::string path = std::string(kRivDir) + name;
        FILE* fp = fopen(path.c_str(), "rb");
        if (fp == nullptr)
        {
            continue;
        }
        fclose(fp);
        auto l = std::make_unique<Live>();
        l->session = std::make_unique<DeferredSession>(rive::ore::ReplayCaps{});
        l->file = ReadRiveFile(path.c_str(), l->session.get());
        if (l->file == nullptr)
        {
            continue;
        }
        l->artboard = l->file->artboardDefault();
        if (l->artboard == nullptr)
        {
            continue;
        }
        l->scene = l->artboard->defaultScene();
        live.push_back(std::move(l));
    }
    printf("MEASURE_CONFIG,concurrent_sessions,%d,frames,%d\n",
           static_cast<int>(live.size()),
           frames);

    for (int frame = 0; frame < frames; frame++)
    {
        for (auto& l : live)
        {
            float seconds = frame == 0 ? 0.f : 1.f / 60.f;
            if (l->scene != nullptr)
            {
                l->scene->advanceAndApply(seconds);
            }
            else
            {
                l->artboard->advance(seconds);
            }
            Renderer* r = l->session->screenRenderer();
            r->save();
            if (l->scene != nullptr)
            {
                l->scene->draw(r);
            }
            else
            {
                l->artboard->draw(r);
            }
            r->restore();
            DeferredFrame snapshot = snapshotFrame(*l->session);
            l->session->resetFrame();
            l->replayer.replayFrame(snapshot, l->sink);
        }
    }

    double totalSlots = 0, totalLive = 0, totalBytes = 0;
    for (size_t i = 0; i < live.size(); i++)
    {
        const ResourceTable& t = live[i]->replayer.table();
        auto liveCount = [](const auto& r) {
            size_t n = 0;
            for (const auto& o : r.objects)
            {
                n += o != nullptr ? 1 : 0;
            }
            return static_cast<double>(n);
        };
        double slots = static_cast<double>(
            t.paths.objects.size() + t.paints.objects.size() +
            t.shaders.objects.size() + t.images.objects.size() +
            t.buffers.objects.size());
        double used = liveCount(t.paths) + liveCount(t.paints) +
                      liveCount(t.shaders) + liveCount(t.images) +
                      liveCount(t.buffers);
        double bytes = slots * (sizeof(rcp<RenderPath>) + 2 * sizeof(uint32_t));
        printf("MEASURE,session_%d,resident,slots,%.0f\n",
               static_cast<int>(i),
               slots);
        printf("MEASURE,session_%d,resident,live,%.0f\n",
               static_cast<int>(i),
               used);
        totalSlots += slots;
        totalLive += used;
        totalBytes += bytes;
    }
    printf("MEASURE,all_sessions,resident,slots,%.0f\n", totalSlots);
    printf("MEASURE,all_sessions,resident,live,%.0f\n", totalLive);
    printf("MEASURE,all_sessions,resident,slot_vector_bytes,%.0f\n",
           totalBytes);
}

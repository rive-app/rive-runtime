/*
 * Copyright 2026 Rive
 */

// Per-frame cost of real .riv files: the state machine's advanceAndApply (which
// runs the artboard's advance and update passes) plus the artboard's draw.
//
// Run from packages/runtime/tests, e.g.
//
//     out/release/bench --duration 10 ArtboardFrame_car_widgets_idle
//
// Assets load from unit_tests/assets unless RIVE_BENCH_ASSETS names another
// directory. Every run() plays kFramesPerRun frames, so the time bench prints
// divided by kFramesPerRun is the cost of one frame. Before each timed run the
// bench restarts from a fresh instance warmed up the same way, so every sample
// plays the same frames of the authored animation. Each bench also prints,
// once from setup(), the work it does per frame over the same scenario:
// render path rebuilds (each drops the renderer's cached triangulation),
// gradients, renderer calls, allocations, and how many frames asked to keep
// animating. Those counts are exact, so they show work removed even where a
// timing is inside the noise. (Allocations aren't counted in Dawn builds; see
// below.) A bench whose .riv can't be read or imported says so and skips, so
// CI can run every bench once on a device that has no assets.
//
// Two renderers:
//   - a counting no-op renderer: the runtime's own advance/update/draw cost,
//     with the counters above;
//   - (_rive) the real RiveRenderer over a null GPU context, so the renderer's
//     CPU work -- tessellation, triangulation and its caches -- is included.

#include "bench.hpp"

#include "common/render_context_null.hpp"
#include "assets/paper.riv.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/file.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/math_types.hpp"
#include "rive/node.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/scene.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "utils/no_op_renderer.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <new>
#include <string>
#include <vector>

using namespace rive;
using namespace rive::gpu;

// -- Allocation counting ------------------------------------------------------
//
// Replaces the global allocation functions for the whole bench binary.
// Counting is off unless a bench's counter pass turns it on, so every other
// bench only pays a relaxed load per allocation. Dawn builds leave this out:
// they link PartitionAlloc's allocator shim, which already replaces them.
#ifndef RIVE_DAWN
#define ARTBOARD_FRAME_BENCH_COUNTS_ALLOCATIONS
#endif

namespace
{
std::atomic<bool> g_countAllocations{false};
std::atomic<uint64_t> g_allocations{0};

#ifdef ARTBOARD_FRAME_BENCH_COUNTS_ALLOCATIONS
void* countedAllocation(size_t size)
{
    if (g_countAllocations.load(std::memory_order_relaxed))
    {
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    }
    void* ptr = malloc(size != 0 ? size : 1);
    if (ptr == nullptr)
    {
        abort();
    }
    return ptr;
}
#endif
} // namespace

#ifdef ARTBOARD_FRAME_BENCH_COUNTS_ALLOCATIONS
// The array forms are replaceable in their own right, so route them here too
// rather than trusting the library's defaults to call the scalar ones.
void* operator new(size_t size) { return countedAllocation(size); }
void* operator new[](size_t size) { return countedAllocation(size); }
void operator delete(void* ptr) noexcept { free(ptr); }
void operator delete[](void* ptr) noexcept { free(ptr); }
void operator delete(void* ptr, size_t) noexcept { free(ptr); }
void operator delete[](void* ptr, size_t) noexcept { free(ptr); }
#endif

namespace
{
// -- Work counters ------------------------------------------------------------

struct WorkCounters
{
    uint64_t renderPathsMade = 0;
    uint64_t renderPathRewinds = 0;
    uint64_t renderPathRawAdds = 0;
    uint64_t gradientsMade = 0;
    uint64_t renderPaintsMade = 0;
    uint64_t saves = 0;
    uint64_t restores = 0;
    uint64_t transforms = 0;
    uint64_t drawPaths = 0;
    uint64_t clipPaths = 0;
    uint64_t drawImages = 0;
};

// The bench process runs one bench on one thread, so a plain global is enough.
WorkCounters g_counters;

class CountingRenderPath : public RenderPath
{
public:
    void rewind() override { g_counters.renderPathRewinds++; }
    void fillRule(FillRule value) override {}
    void addPath(CommandPath* path, const Mat2D& transform) override {}
    void addRenderPath(const RenderPath* path, const Mat2D& transform) override
    {}
    void moveTo(float x, float y) override {}
    void lineTo(float x, float y) override {}
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override
    {}
    void close() override {}
    void addRawPath(const RawPath& path) override
    {
        g_counters.renderPathRawAdds++;
    }
};

class CountingRenderPaint : public RenderPaint
{
public:
    void color(unsigned int value) override {}
    void style(RenderPaintStyle value) override {}
    void thickness(float value) override {}
    void join(StrokeJoin value) override {}
    void cap(StrokeCap value) override {}
    void blendMode(BlendMode value) override {}
    void shader(rcp<RenderShader>) override {}
    void invalidateStroke() override {}
    void feather(float value) override {}
    void modulatedImage(const RenderImage*, ImageSampler, const Mat2D&) override
    {}
};

class CountingRenderShader : public RenderShader
{};

// Mirrors utils/no_op_factory.cpp (not built into this target), counting what
// the runtime asks it to make.
class CountingFactory : public Factory
{
public:
    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                       RenderBufferFlags,
                                       size_t) override
    {
        return nullptr;
    }

    rcp<RenderShader> makeLinearGradient(float,
                                         float,
                                         float,
                                         float,
                                         const ColorInt[],
                                         const float[],
                                         size_t) override
    {
        g_counters.gradientsMade++;
        return make_rcp<CountingRenderShader>();
    }

    rcp<RenderShader> makeRadialGradient(float,
                                         float,
                                         float,
                                         const ColorInt[],
                                         const float[],
                                         size_t) override
    {
        g_counters.gradientsMade++;
        return make_rcp<CountingRenderShader>();
    }

    rcp<RenderPath> makeRenderPath(RawPath&, FillRule) override
    {
        g_counters.renderPathsMade++;
        return make_rcp<CountingRenderPath>();
    }

    rcp<RenderPath> makeEmptyRenderPath() override
    {
        g_counters.renderPathsMade++;
        return make_rcp<CountingRenderPath>();
    }

    rcp<RenderPaint> makeRenderPaint() override
    {
        g_counters.renderPaintsMade++;
        return make_rcp<CountingRenderPaint>();
    }

    rcp<RenderImage> decodeImage(Span<const uint8_t>) override
    {
        return nullptr;
    }
};

class CountingRenderer : public NoOpRenderer
{
public:
    void save() override { g_counters.saves++; }
    void restore() override { g_counters.restores++; }
    void transform(const Mat2D&) override { g_counters.transforms++; }
    void drawPath(RenderPath*, RenderPaint*) override
    {
        g_counters.drawPaths++;
    }
    void clipPath(RenderPath*) override { g_counters.clipPaths++; }
    void drawImage(const RenderImage*, ImageSampler, BlendMode, float) override
    {
        g_counters.drawImages++;
    }
};

// -- Scenarios
// ------------------------------------------------------------------

enum class Scenario
{
    // The default state machine as authored.
    idle,
    // Every top-level node translates together each frame: a rigid move of the
    // whole scene, what scrolling or a parent animation does to its contents.
    move,
    // Only one leaf shape moves: a large, mostly idle artboard.
    poke,
    // The artboard's opacity animates: a fade over everything.
    fade,
    // Every scroll constraint sweeps its full range.
    scroll,
};

enum class RendererKind
{
    counting,
    rive,
};

constexpr int kWarmupFrames = 120;
constexpr int kFramesPerRun = 120;
constexpr float kFrameSeconds = 1.0f / 60.0f;

// Reads unit_tests/assets/[name], or [name] under RIVE_BENCH_ASSETS. False when
// it isn't there -- CI runs every bench once from places without the assets.
bool readAsset(const char* name, std::vector<uint8_t>* bytes)
{
    const char* dir = getenv("RIVE_BENCH_ASSETS");
    std::string path =
        std::string(dir != nullptr ? dir : "unit_tests/assets") + "/" + name;
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return false;
    }
    bytes->assign(std::istreambuf_iterator<char>(stream),
                  std::istreambuf_iterator<char>());
    return true;
}

class ArtboardFrameBench : public Bench
{
public:
    // A null asset name selects the embedded paper.riv.
    ArtboardFrameBench(const char* asset,
                       Scenario scenario,
                       RendererKind rendererKind) :
        m_asset(asset), m_scenario(scenario), m_rendererKind(rendererKind)
    {}

    void setup() override
    {
        Factory* factory = &m_countingFactory;
        if (m_rendererKind == RendererKind::rive)
        {
            m_nullContext = RenderContextNULL::MakeContext();
            factory = m_nullContext.get();
        }
        if (m_asset != nullptr)
        {
            if (!readAsset(m_asset, &m_bytes))
            {
                skip("can't open it (run from packages/runtime/tests, or "
                     "point RIVE_BENCH_ASSETS at tests/unit_tests/assets)");
                return;
            }
            m_file = File::import(
                Span<const uint8_t>(m_bytes.data(), m_bytes.size()),
                factory);
        }
        else
        {
            m_file = File::import(assets::paper_riv(), factory);
        }
        if (m_file == nullptr)
        {
            skip("it didn't import (an unfetched Git LFS pointer?)");
            return;
        }
        instantiate();

        if (m_rendererKind == RendererKind::rive)
        {
            m_renderer = std::make_unique<RiveRenderer>(m_nullContext.get());
            auto width =
                std::max(1u, static_cast<uint32_t>(m_artboard->width()));
            auto height =
                std::max(1u, static_cast<uint32_t>(m_artboard->height()));
            m_renderTarget =
                m_nullContext->static_impl_cast<RenderContextNULL>()
                    ->makeRenderTarget(width, height);
        }
        else
        {
            m_renderer = std::make_unique<CountingRenderer>();
        }

        reportScenarioTargets();
        warmUp();
        printCounters();
    }

    // Without this the authored animation keeps running from one sample to
    // the next, each sample covers different frames, and bench keeps
    // whichever happened to be cheapest.
    void beforeRun() override
    {
        if (m_skipped)
        {
            return;
        }
        instantiate();
        warmUp();
    }

    int run() const override
    {
        if (m_skipped)
        {
            return 0;
        }
        int keepGoing = 0;
        for (int i = 0; i < kFramesPerRun; ++i)
        {
            keepGoing += frame() ? 1 : 0;
        }
        return keepGoing;
    }

private:
    void skip(const char* reason)
    {
        fprintf(stderr,
                "artboard_frame_bench: skipping %s: %s\n",
                m_asset != nullptr ? m_asset : "paper.riv",
                reason);
        m_skipped = true;
    }

    // A fresh instance of the default artboard and its scene, with the
    // view model and scene chosen the way the player does
    // (tests/player/player.cpp).
    void instantiate()
    {
        // The scene refers to the artboard, so it goes first.
        m_scene = nullptr;
        m_movers.clear();
        m_scrolls.clear();
        m_artboard = m_file->artboardDefault();
        m_viewModelInstance =
            m_file->createDefaultViewModelInstance(m_artboard.get());
        if (m_viewModelInstance != nullptr)
        {
            m_artboard->bindViewModelInstance(m_viewModelInstance);
        }
        std::unique_ptr<StateMachineInstance> stateMachine =
            m_artboard->defaultStateMachine();
        if (stateMachine == nullptr && m_artboard->stateMachineCount() > 0)
        {
            stateMachine = m_artboard->stateMachineAt(0);
        }
        m_scene = std::move(stateMachine);
        if (m_scene == nullptr)
        {
            m_scene = m_artboard->animationAt(0);
        }
        if (m_scene != nullptr && m_viewModelInstance != nullptr)
        {
            m_scene->bindViewModelInstance(m_viewModelInstance);
        }
        collectScenarioTargets();
        m_frame = 0;
    }

    void warmUp()
    {
        for (int i = 0; i < kWarmupFrames; ++i)
        {
            frame();
        }
    }

    void collectScenarioTargets()
    {
        switch (m_scenario)
        {
            case Scenario::move:
                for (auto object : m_artboard->objects())
                {
                    if (object == nullptr || object == m_artboard.get() ||
                        !object->is<Node>() ||
                        object->as<Node>()->parent() != m_artboard.get())
                    {
                        continue;
                    }
                    auto node = object->as<Node>();
                    // Files from before 7.3 let the layout own a layout's x/y,
                    // so moving one of those would measure nothing.
                    if (node->is<LayoutComponent>() &&
                        !node->as<LayoutComponent>()->composesLayoutOffset())
                    {
                        continue;
                    }
                    m_movers.push_back({node, node->x(), node->y()});
                }
                break;
            case Scenario::poke:
                for (auto shape : m_artboard->find<Shape>())
                {
                    m_movers.push_back({shape, shape->x(), shape->y()});
                    break;
                }
                break;
            case Scenario::scroll:
                m_scrolls = m_artboard->find<ScrollConstraint>();
                break;
            case Scenario::idle:
            case Scenario::fade:
                break;
        }
    }

    void reportScenarioTargets() const
    {
        if (m_scenario == Scenario::move || m_scenario == Scenario::poke)
        {
            fprintf(stderr,
                    "artboard_frame_bench: moving %zu nodes\n",
                    m_movers.size());
        }
        if (m_scenario == Scenario::scroll && m_scrolls.empty())
        {
            fprintf(stderr, "artboard_frame_bench: no scroll constraint\n");
        }
    }

    // Returns what advanceAndApply returned: whether the scene wants another
    // frame.
    bool frame() const
    {
        // One full period of every scenario per run, so each timed run covers
        // the same work.
        float phase = 2.0f * math::PI * (float)(m_frame % kFramesPerRun) /
                      (float)kFramesPerRun;
        m_frame++;
        switch (m_scenario)
        {
            case Scenario::move:
            case Scenario::poke:
                for (const auto& mover : m_movers)
                {
                    mover.node->x(mover.x + 20.0f * std::sin(phase));
                    mover.node->y(mover.y + 10.0f * std::cos(phase));
                }
                break;
            case Scenario::fade:
                m_artboard->opacity(0.55f + 0.45f * std::cos(phase));
                break;
            case Scenario::scroll:
                for (auto scroll : m_scrolls)
                {
                    float t = 0.5f - 0.5f * std::cos(phase);
                    scroll->scrollOffsetX(scroll->maxOffsetX() * t);
                    scroll->scrollOffsetY(scroll->maxOffsetY() * t);
                }
                break;
            case Scenario::idle:
                break;
        }

        bool keepGoing = false;
        if (m_scene != nullptr)
        {
            keepGoing = m_scene->advanceAndApply(kFrameSeconds);
        }
        else
        {
            keepGoing = m_artboard->advance(kFrameSeconds);
        }

        if (m_rendererKind == RendererKind::rive)
        {
            m_nullContext->beginFrame({
                .renderTargetWidth = m_renderTarget->width(),
                .renderTargetHeight = m_renderTarget->height(),
            });
            m_artboard->draw(m_renderer.get());
            m_nullContext->flush({.renderTarget = m_renderTarget.get()});
        }
        else
        {
            m_artboard->draw(m_renderer.get());
        }
        return keepGoing;
    }

    void printCounters()
    {
        g_counters = WorkCounters();
        g_allocations.store(0);
        g_countAllocations.store(true);
        int keepGoing = 0;
        for (int i = 0; i < kFramesPerRun; ++i)
        {
            keepGoing += frame() ? 1 : 0;
        }
        g_countAllocations.store(false);

        auto perFrame = [](uint64_t value) {
            return (double)value / (double)kFramesPerRun;
        };
#ifdef ARTBOARD_FRAME_BENCH_COUNTS_ALLOCATIONS
        printf("[%d frames/run] per frame: allocs %.1f",
               kFramesPerRun,
               perFrame(g_allocations.load()));
#else
        printf("[%d frames/run] per frame: allocs n/a", kFramesPerRun);
#endif
        if (m_rendererKind == RendererKind::counting)
        {
            printf(", path rebuilds %.1f (rewind %.1f, addRawPath %.1f), "
                   "paths made %.1f, gradients %.1f, paints made %.1f, "
                   "save %.1f, restore %.1f, transform %.1f, drawPath %.1f, "
                   "clipPath %.1f, drawImage %.1f",
                   perFrame(g_counters.renderPathRewinds),
                   perFrame(g_counters.renderPathRewinds),
                   perFrame(g_counters.renderPathRawAdds),
                   perFrame(g_counters.renderPathsMade),
                   perFrame(g_counters.gradientsMade),
                   perFrame(g_counters.renderPaintsMade),
                   perFrame(g_counters.saves),
                   perFrame(g_counters.restores),
                   perFrame(g_counters.transforms),
                   perFrame(g_counters.drawPaths),
                   perFrame(g_counters.clipPaths),
                   perFrame(g_counters.drawImages));
        }
        printf(", keep-going frames %d/%d\n", keepGoing, kFramesPerRun);
        fflush(stdout);
    }

    struct Mover
    {
        Node* node;
        float x;
        float y;
    };

    const char* m_asset;
    Scenario m_scenario;
    RendererKind m_rendererKind;

    CountingFactory m_countingFactory;
    std::unique_ptr<RenderContext> m_nullContext;
    rcp<RenderTarget> m_renderTarget;
    std::vector<uint8_t> m_bytes;
    rcp<File> m_file;
    std::unique_ptr<ArtboardInstance> m_artboard;
    rcp<ViewModelInstance> m_viewModelInstance;
    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<Renderer> m_renderer;
    std::vector<Mover> m_movers;
    std::vector<ScrollConstraint*> m_scrolls;
    mutable uint64_t m_frame = 0;
    bool m_skipped = false;
};

#define ARTBOARD_FRAME_BENCH(NAME, ASSET, SCENARIO, RENDERER)                  \
    class ArtboardFrame_##NAME : public ArtboardFrameBench                     \
    {                                                                          \
    public:                                                                    \
        ArtboardFrame_##NAME() :                                               \
            ArtboardFrameBench(ASSET,                                          \
                               Scenario::SCENARIO,                             \
                               RendererKind::RENDERER)                         \
        {}                                                                     \
    };                                                                         \
    REGISTER_BENCH(ArtboardFrame_##NAME);

// Layout UI.
ARTBOARD_FRAME_BENCH(db_health_tracker_idle,
                     "db_health_tracker.riv",
                     idle,
                     counting)
ARTBOARD_FRAME_BENCH(db_health_tracker_idle_rive,
                     "db_health_tracker.riv",
                     idle,
                     rive)
ARTBOARD_FRAME_BENCH(data_viz_demo_idle, "data_viz_demo.riv", idle, counting)
ARTBOARD_FRAME_BENCH(layout_paint_idle,
                     "layout/layout_paint.riv",
                     idle,
                     counting)
ARTBOARD_FRAME_BENCH(layout_paint_move,
                     "layout/layout_paint.riv",
                     move,
                     counting)
ARTBOARD_FRAME_BENCH(layout_paint_move_rive,
                     "layout/layout_paint.riv",
                     move,
                     rive)
ARTBOARD_FRAME_BENCH(layout_animation_nested_idle,
                     "layout/layout_animation_nested.riv",
                     idle,
                     counting)
ARTBOARD_FRAME_BENCH(layout_animation_component_list_idle,
                     "layout/layout_animation_component_list.riv",
                     idle,
                     counting)
ARTBOARD_FRAME_BENCH(layout_scroll_vertical_scroll,
                     "layout/layout_scroll_vertical.riv",
                     scroll,
                     counting)
ARTBOARD_FRAME_BENCH(layout_scroll_vertical_scroll_rive,
                     "layout/layout_scroll_vertical.riv",
                     scroll,
                     rive)
ARTBOARD_FRAME_BENCH(stack_participant_idle,
                     "layout/stack_participant.riv",
                     idle,
                     counting)
ARTBOARD_FRAME_BENCH(hug_participant_idle,
                     "layout/hug_participant.riv",
                     idle,
                     counting)
ARTBOARD_FRAME_BENCH(scroll_participant_scroll,
                     "layout/scroll_participant.riv",
                     scroll,
                     counting)

// Vector animation.
ARTBOARD_FRAME_BENCH(car_widgets_idle, "car_widgets_v01.riv", idle, counting)
ARTBOARD_FRAME_BENCH(car_widgets_move, "car_widgets_v01.riv", move, counting)
ARTBOARD_FRAME_BENCH(car_widgets_fade, "car_widgets_v01.riv", fade, counting)
ARTBOARD_FRAME_BENCH(car_widgets_idle_rive, "car_widgets_v01.riv", idle, rive)
ARTBOARD_FRAME_BENCH(hunter_x_idle, "hunter_x_demo.riv", idle, counting)
ARTBOARD_FRAME_BENCH(hunter_x_fade, "hunter_x_demo.riv", fade, counting)
ARTBOARD_FRAME_BENCH(zombie_skins_idle, "zombie_skins.riv", idle, counting)
ARTBOARD_FRAME_BENCH(zombie_skins_fade, "zombie_skins.riv", fade, counting)
ARTBOARD_FRAME_BENCH(trim_path_idle, "trim_path.riv", idle, counting)
ARTBOARD_FRAME_BENCH(trim_path_move, "trim_path.riv", move, counting)

// Nested artboards and lists.
ARTBOARD_FRAME_BENCH(planets_grid_idle,
                     "layoutstest_8-planets-grid.riv",
                     idle,
                     counting)
ARTBOARD_FRAME_BENCH(superbowl_idle, "superbowl.riv", idle, counting)
ARTBOARD_FRAME_BENCH(echo_show_idle, "echo_show_demo.riv", idle, counting)

// Large, mostly idle.
ARTBOARD_FRAME_BENCH(paper_poke, nullptr, poke, counting)
ARTBOARD_FRAME_BENCH(paper_poke_rive, nullptr, poke, rive)
ARTBOARD_FRAME_BENCH(bidirectional_binding_idle,
                     "bidirectional_binding_source.riv",
                     idle,
                     counting)
} // namespace

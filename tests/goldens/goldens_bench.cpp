/*
 * Copyright 2022 Rive
 */

// Env gated diagnostics and the RIVE_GOLDENS_BENCH benchmark, split out of
// goldens.cpp.

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "goldens_shared.hpp"

#if defined(WITH_RIVE_SCRIPTING) && defined(RIVE_CANVAS)

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <unordered_map>

// Tallies per resource mutations and draws in one recorded 2D frame to spot
// redundant rebuilds.
static void analyze_frame_redundancy(const rive::cmd::RenderCommandBuffer& cmd)
{
    using namespace rive::cmd;
    RenderCommandReader r(cmd.commandBytes(), cmd.blobBytes());
    std::unordered_map<RenderHandle, int> rewinds, addRaw, draws, paintSets;
    std::unordered_map<RenderHandle, uint32_t> lastColor;
    int colorSets = 0, colorSameValueRepeat = 0;
    size_t geomBytes = 0;
    uint8_t type;
    while (r.next(type))
    {
        switch (static_cast<RenderCmd>(type))
        {
            case RenderCmd::pathRewind:
                rewinds[r.read<ResIdPOD>().id]++;
                break;
            case RenderCmd::pathFillRule:
                r.read<PathFillRulePOD>();
                break;
            case RenderCmd::pathAddRawPath:
            {
                auto c = r.read<PathRawPOD>();
                addRaw[c.path]++;
                geomBytes += c.verbCount * sizeof(rive::PathVerb) +
                             c.pointCount * sizeof(rive::Vec2D);
                break;
            }
            case RenderCmd::pathAddRenderPath:
                r.read<PathAddPathPOD>();
                break;
            case RenderCmd::paintStyle:
            case RenderCmd::paintJoin:
            case RenderCmd::paintCap:
            case RenderCmd::paintBlendMode:
                paintSets[r.read<PaintU8POD>().paint]++;
                break;
            case RenderCmd::paintColor:
            {
                auto c = r.read<PaintColorPOD>();
                paintSets[c.paint]++;
                colorSets++;
                auto it = lastColor.find(c.paint);
                if (it != lastColor.end() && it->second == c.color)
                    colorSameValueRepeat++;
                lastColor[c.paint] = c.color;
                break;
            }
            case RenderCmd::paintThickness:
            case RenderCmd::paintFeather:
                paintSets[r.read<PaintFloatPOD>().paint]++;
                break;
            case RenderCmd::paintShader:
                paintSets[r.read<PaintShaderPOD>().paint]++;
                break;
            case RenderCmd::paintInvalidateStroke:
                r.read<ResIdPOD>();
                break;
            case RenderCmd::save:
            case RenderCmd::restore:
            case RenderCmd::makeEmptyPath:
            case RenderCmd::makePaint:
                break;
            case RenderCmd::transform:
                r.read<TransformPOD>();
                break;
            case RenderCmd::drawPath:
                draws[r.read<DrawPathPOD>().path]++;
                break;
            case RenderCmd::clipPath:
                r.read<ClipPathPOD>();
                break;
            case RenderCmd::resourceNewVersion:
                r.read<ResourceVersionPOD>();
                break;
            case RenderCmd::drawImage:
                r.read<DrawImagePOD>();
                break;
            case RenderCmd::drawImageMesh:
                r.read<DrawImageMeshPOD>();
                break;
            case RenderCmd::modulateOpacity:
                r.read<OpacityPOD>();
                break;
            case RenderCmd::canvasContentBegin:
                r.read<CanvasContentPOD>();
                break;
            case RenderCmd::canvasContentEnd:
                r.read<ResIdPOD>();
                break;
            case RenderCmd::makePath:
                r.read<MakePathPOD>();
                break;
            case RenderCmd::makeLinearGradient:
                r.read<LinearGradientPOD>();
                break;
            case RenderCmd::makeRadialGradient:
                r.read<RadialGradientPOD>();
                break;
            case RenderCmd::decodeImage:
                r.read<DecodeImagePOD>();
                break;
            case RenderCmd::makeBuffer:
                r.read<MakeBufferPOD>();
                break;
            case RenderCmd::bufferData:
                r.read<BufferDataPOD>();
                break;
            case RenderCmd::destroyResource:
                r.read<DestroyResourcePOD>();
                break;
        }
    }
    auto sum = [](const std::unordered_map<RenderHandle, int>& m) {
        int t = 0;
        for (auto& kv : m)
            t += kv.second;
        return t;
    };
    auto multi = [](const std::unordered_map<RenderHandle, int>& m) {
        int t = 0, mx = 0;
        for (auto& kv : m)
        {
            if (kv.second > 1)
                t++;
            mx = std::max(mx, kv.second);
        }
        return std::pair<int, int>(t, mx);
    };
    int totalRewind = sum(rewinds), totalAdd = sum(addRaw),
        totalDraw = sum(draws), totalPaint = sum(paintSets);
    auto rw = multi(rewinds);
    auto ad = multi(addRaw);
    auto pt = multi(paintSets);

    printf("\n-- frame redundancy analysis (one clean frame) --\n");
    printf("  paths: %zu distinct rewound, %d total rewinds  "
           "(%d rewound >1x, max %dx)\n",
           rewinds.size(),
           totalRewind,
           rw.first,
           rw.second);
    printf("  paths: %zu distinct addRawPath, %d total adds   "
           "(%d added >1x, max %dx), geom %.1f KB\n",
           addRaw.size(),
           totalAdd,
           ad.first,
           ad.second,
           geomBytes / 1024.0);
    printf("  paints: %zu distinct, %d total property sets   "
           "(%d set >1x, max %dx)\n",
           paintSets.size(),
           totalPaint,
           pt.first,
           pt.second);
    printf("  paint color sets: %d total, %d set to the SAME value again "
           "(redundant)\n",
           colorSets,
           colorSameValueRepeat);
    printf("  draws: %zu distinct paths drawn, %d total drawPath\n",
           draws.size(),
           totalDraw);
    if (rewinds.size() > 0)
        printf("  => rebuild ratio: %.2f rewinds/path, %.2f adds/path "
               "(1.0 = each built once; >1 = redundant rebuilds)\n",
               double(totalRewind) / rewinds.size(),
               addRaw.empty() ? 0.0 : double(totalAdd) / addRaw.size());
}

// Counts drawPath commands that resolve against the resident table versus ones
// skipped, to tell missing resources apart from other replay bugs.
static void diagnose_replay_coverage(const rive::cmd::RenderCommandBuffer& cmd,
                                     const rive::cmd::ResourceTable& t)
{
    using namespace rive::cmd;
    RenderCommandReader r(cmd.commandBytes(), cmd.blobBytes());
    int total = 0, resolved = 0, pNull = 0, pOOR = 0, ptNull = 0, ptOOR = 0;
    RenderHandle maxPath = 0, maxPaint = 0;
    uint8_t type;
    while (r.next(type))
    {
        switch (static_cast<RenderCmd>(type))
        {
            case RenderCmd::drawPath:
            {
                auto c = r.read<DrawPathPOD>();
                total++;
                maxPath = std::max(maxPath, c.path);
                maxPaint = std::max(maxPaint, c.paint);
                bool pOk = t.paths.get(c.path) != nullptr;
                bool ptOk = t.paints.get(c.paint) != nullptr;
                if (c.path >= t.paths.objects.size())
                    pOOR++;
                else if (!pOk)
                    pNull++;
                if (c.paint >= t.paints.objects.size())
                    ptOOR++;
                else if (!ptOk)
                    ptNull++;
                if (pOk && ptOk)
                    resolved++;
                break;
            }
            case RenderCmd::pathRewind:
            case RenderCmd::clipPath:
            case RenderCmd::paintInvalidateStroke:
            case RenderCmd::canvasContentEnd:
                r.read<ResIdPOD>();
                break;
            case RenderCmd::pathFillRule:
                r.read<PathFillRulePOD>();
                break;
            case RenderCmd::pathAddRawPath:
                r.read<PathRawPOD>();
                break;
            case RenderCmd::pathAddRenderPath:
                r.read<PathAddPathPOD>();
                break;
            case RenderCmd::paintStyle:
            case RenderCmd::paintJoin:
            case RenderCmd::paintCap:
            case RenderCmd::paintBlendMode:
                r.read<PaintU8POD>();
                break;
            case RenderCmd::paintColor:
                r.read<PaintColorPOD>();
                break;
            case RenderCmd::paintThickness:
            case RenderCmd::paintFeather:
                r.read<PaintFloatPOD>();
                break;
            case RenderCmd::paintShader:
                r.read<PaintShaderPOD>();
                break;
            case RenderCmd::transform:
                r.read<TransformPOD>();
                break;
            case RenderCmd::drawImage:
                r.read<DrawImagePOD>();
                break;
            case RenderCmd::drawImageMesh:
                r.read<DrawImageMeshPOD>();
                break;
            case RenderCmd::modulateOpacity:
                r.read<OpacityPOD>();
                break;
            case RenderCmd::canvasContentBegin:
                r.read<CanvasContentPOD>();
                break;
            case RenderCmd::makePath:
                r.read<MakePathPOD>();
                break;
            case RenderCmd::makeLinearGradient:
                r.read<LinearGradientPOD>();
                break;
            case RenderCmd::makeRadialGradient:
                r.read<RadialGradientPOD>();
                break;
            case RenderCmd::decodeImage:
                r.read<DecodeImagePOD>();
                break;
            case RenderCmd::makeBuffer:
                r.read<MakeBufferPOD>();
                break;
            case RenderCmd::bufferData:
                r.read<BufferDataPOD>();
                break;
            default:
                break; // no payload
        }
    }
    printf(
        "\n-- replay coverage diagnosis (clean frame vs resident table) --\n");
    printf("  table: %zu paths, %zu paints\n",
           t.paths.objects.size(),
           t.paints.objects.size());
    printf("  drawPath: %d total, %d resolved, skipped path[null %d, OOR %d] "
           "paint[null %d, OOR %d]\n",
           total,
           resolved,
           pNull,
           pOOR,
           ptNull,
           ptOOR);
    printf("  max referenced: path id %u, paint id %u\n", maxPath, maxPaint);
}

// Drains one recorded frame through the caller owned replayer. Leaves the
// screen frame open for the caller to present via endFrame.
static void replay_deferred_frame(rive::cmd::DeferredReplayer& replayer,
                                  rive::cmd::DeferredSession* session)
{
    GoldensFrameSink sink;
    replayer.replayFrame(*session, sink);
}

void run_benchmark(const std::vector<uint8_t>& bytes,
                   const char* artboardName,
                   const char* stateMachineName,
                   int iters)
{
    using clock = std::chrono::steady_clock;
    auto us = [](clock::duration d) {
        return std::chrono::duration<double, std::micro>(d).count();
    };
    auto* win = TestingWindow::Get();
    const int cellSize = 256;
    const rive::AABB cell(0, 0, cellSize, cellSize);
    const float dt = 1.0f / 60.0f;
    const int kWarmup = 8;

    auto drawInto = [&](rive::Renderer* r, rive::Scene* s, rive::Artboard* a) {
        r->save();
        r->align(rive::Fit::cover, rive::Alignment::center, cell, s->bounds());
        a->drawInternal(r);
        r->restore();
    };

    // Immediate: full main thread frame.
    RIVLoader imm(bytes,
                  artboardName,
                  stateMachineName,
                  RIVLoader::DeferMode::Immediate);
    auto* immScene = imm.stateMachine();
    auto* immArt = imm.artboard();
    immScene->advanceAndApply(0.0f);
    auto immFrame = [&]() {
        immScene->advanceAndApply(dt);
        auto r = win->beginFrame({.clearColor = 0xffffffff});
        drawInto(r.get(), immScene, immArt);
        win->endFrame();
    };
    for (int i = 0; i < kWarmup; ++i)
        immFrame();
    auto t0 = clock::now();
    for (int i = 0; i < iters; ++i)
        immFrame();
    double immUs = us(clock::now() - t0) / iters;

    // Deferred record: no GPU submission.
    RIVLoader def(bytes,
                  artboardName,
                  stateMachineName,
                  RIVLoader::DeferMode::Deferred);
    auto* session = def.deferredSession();
    auto* defScene = def.stateMachine();
    auto* defArt = def.artboard();
    defScene->advanceAndApply(0.0f);
    auto recFrame = [&]() {
        defScene->advanceAndApply(dt);
        session->recordOreReplayMarker();
        auto r = session->makeScreenRenderer();
        drawInto(r.get(), defScene, defArt);
    };
    // The 2D stream accumulates because recFrame never resets. Resources are
    // created on the first frame only, so later deltas are draws only.
    auto bytes2D = [&]() -> size_t {
        return session->commandBuffer().commandBytes().size() +
               session->commandBuffer().blobBytes().size();
    };
    auto streamBytes = [&]() -> size_t {
        return bytes2D() + session->oreContext().streamBytes().total();
    };
    recFrame(); // first frame includes one time resource creation
    auto coldOre = session->oreContext().streamBytes();
    size_t cold2D = bytes2D();
    for (int i = 1; i < kWarmup; ++i)
        recFrame();
    size_t before = streamBytes();
    size_t before2D = bytes2D();
    auto t1 = clock::now();
    for (int i = 0; i < iters; ++i)
        recFrame();
    double recUs = us(clock::now() - t1) / iters;
    double perFrameBytes = double(streamBytes() - before) / iters;
    double perFrame2D = double(bytes2D() - before2D) / iters;

    // RIVE_GOLDENS_ORE_HISTO prints an Ore opcode histogram for one clean
    // frame to diagnose per frame resource churn.
    if (goldens_getenv("RIVE_GOLDENS_ORE_HISTO"))
    {
        session->resetFrame();
        recFrame();
        using rive::ore::cmd::CommandType;
        static const char* kNames[] = {"beginRenderPass", "setPipeline",
                                       "setVertexBuffer", "setIndexBuffer",
                                       "setBindGroup",    "setViewport",
                                       "setScissorRect",  "setStencilRef",
                                       "setBlendColor",   "draw",
                                       "drawIndexed",     "finish",
                                       "makeBuffer",      "makeTexture",
                                       "makeSampler",     "makeShaderModule",
                                       "makeBGLayout",    "makeTextureView",
                                       "makePipeline",    "makeBindGroup",
                                       "bufferUpdate",    "textureUpload",
                                       "destroyResource"};
        int counts[64] = {};
        auto& cb = session->oreContext().stream();
        rive::ore::cmd::OreCommandReader rd(cb.commandBytes(), cb.blobBytes());
        CommandType t;
        while (rd.next(t))
        {
            uint8_t v = static_cast<uint8_t>(t);
            if (v < 64)
            {
                counts[v]++;
            }
            rive::ore::cmd::skipOreCommand(t, rd);
        }
        printf("\n-- one steady frame, Ore opcode histogram --\n");
        for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); ++i)
        {
            if (counts[i] != 0)
            {
                printf("    %-16s : %d\n", kNames[i], counts[i]);
            }
        }
    }

    // Deferred replay: one recorded frame replayed repeatedly, cold and
    // steady, to isolate the amortizable resource creation cost.
    RIVLoader rep(bytes,
                  artboardName,
                  stateMachineName,
                  RIVLoader::DeferMode::Deferred);
    auto* repSession = rep.deferredSession();
    auto* repScene = rep.stateMachine();
    auto* repArt = rep.artboard();
    repScene->advanceAndApply(0.0f);
    for (int i = 0; i < kWarmup; ++i)
        repScene->advanceAndApply(dt);
    repSession->recordOreReplayMarker();
    {
        auto r = repSession->makeScreenRenderer();
        drawInto(r.get(), repScene, repArt);
    }

    const int kReplays = 30;
    // Cold: a fresh replayer each frame recreates every resource.
    for (int i = 0; i < 3; ++i)
    {
        rive::cmd::DeferredReplayer cold;
        replay_deferred_frame(cold, repSession);
        win->endFrame();
    }
    auto t2 = clock::now();
    for (int i = 0; i < kReplays; ++i)
    {
        rive::cmd::DeferredReplayer cold;
        replay_deferred_frame(cold, repSession);
        win->endFrame();
    }
    double coldUs = us(clock::now() - t2) / kReplays;

    // Steady: Ore makes are idempotent so Ore resources stay resident. 2D
    // makes overwrite rather than skip, so 2D resources are recreated.
    rive::cmd::DeferredReplayer steady;
    for (int i = 0; i < 3; ++i)
    {
        replay_deferred_frame(steady, repSession);
        win->endFrame();
    }
    auto t3 = clock::now();
    for (int i = 0; i < kReplays; ++i)
    {
        replay_deferred_frame(steady, repSession);
        win->endFrame();
    }
    double steadyUs = us(clock::now() - t3) / kReplays;

    // Phase breakdown for pure 2D scenes. Replay runs on a clean single frame
    // against a primed resident table so it reflects one real frame.
    bool pure2D = repSession->oreContext().streamBytes().commands == 0;
    double mImmAdv = 0, mDefAdv = 0, mImmRen = 0, mDefRec = 0;
    double immAdv = 0, immCpu = 0, immGpu = 0;
    double repAdv = 0, repRecDraw = 0, repCpu = 0, repGpu = 0;
    if (pure2D)
    {
        for (int i = 0; i < kReplays + 3; ++i)
        {
            auto a = clock::now();
            immScene->advanceAndApply(dt);
            rive::Artboard::incFrameId();
            auto b = clock::now();
            auto r = win->beginFrame({.clearColor = 0xffffffff});
            drawInto(r.get(), immScene, immArt);
            auto c = clock::now();
            win->endFrame();
            auto d = clock::now();
            if (i >= 3)
            {
                immAdv += us(b - a);
                immCpu += us(c - b);
                immGpu += us(d - c);
            }
        }
        immAdv /= kReplays;
        immCpu /= kReplays;
        immGpu /= kReplays;

        // Prime the resident table with one full replay, then measure clean
        // single frames against it.
        rive::cmd::ResourceTable t2;
        rive::cmd::replayRenderCommands(win->factory(),
                                        nullptr,
                                        repSession->commandBuffer(),
                                        t2);
        for (int i = 0; i < kReplays + 3; ++i)
        {
            repSession->resetFrame();
            auto a = clock::now();
            repScene->advanceAndApply(dt);
            rive::Artboard::incFrameId();
            auto a2 = clock::now();
            {
                auto rr = repSession->makeScreenRenderer();
                drawInto(rr.get(), repScene, repArt);
            }
            auto b = clock::now();
            // Consumer replay against the resident table.
            auto screen = win->beginFrame({.clearColor = 0xffffffff});
            rive::cmd::replayRenderCommands(win->factory(),
                                            screen.get(),
                                            repSession->commandBuffer(),
                                            t2);
            auto c = clock::now();
            bool last = (i == kReplays + 2);
            std::vector<uint8_t> px;
            win->endFrame(last && goldens_getenv("RIVE_GOLDENS_BENCH_DUMP")
                              ? &px
                              : nullptr);
            auto d = clock::now();
            if (last && goldens_getenv("RIVE_GOLDENS_BENCH_DUMP"))
                dumpPixelsAsPng("bench_consumer",
                                win->width(),
                                win->height(),
                                std::move(px));
            if (i >= 3)
            {
                repAdv += us(a2 - a);
                repRecDraw += us(b - a2);
                repCpu += us(c - b);
                repGpu += us(d - c);
            }
        }
        analyze_frame_redundancy(repSession->commandBuffer());
        diagnose_replay_coverage(repSession->commandBuffer(), t2);
        repAdv /= kReplays;
        repRecDraw /= kReplays;
        repCpu /= kReplays;
        repGpu /= kReplays;

        // Two fresh artboards advanced in lockstep so advance is compared at
        // the same animation state, isolating the serializer overhead.
        RIVLoader immM(bytes,
                       artboardName,
                       stateMachineName,
                       RIVLoader::DeferMode::Immediate);
        RIVLoader defM(bytes,
                       artboardName,
                       stateMachineName,
                       RIVLoader::DeferMode::Deferred);
        auto* immMs = immM.stateMachine();
        auto* immMa = immM.artboard();
        auto* defMs = defM.stateMachine();
        auto* defMa = defM.artboard();
        auto* defMsess = defM.deferredSession();
        immMs->advanceAndApply(0.0f);
        defMs->advanceAndApply(0.0f);
        for (int i = 0; i < kReplays + 5; ++i)
        {
            defMsess->resetFrame();
            auto t0 = clock::now();
            immMs->advanceAndApply(dt);
            auto t1 = clock::now();
            defMs->advanceAndApply(dt); // same frame, plus serialize
            auto t2 = clock::now();
            rive::Artboard::incFrameId();
            auto rim = win->beginFrame({.clearColor = 0xffffffff});
            auto t3 = clock::now();
            drawInto(rim.get(), immMs, immMa);
            auto t4 = clock::now();
            win->endFrame();
            auto t5 = clock::now();
            {
                auto rr = defMsess->makeScreenRenderer();
                drawInto(rr.get(), defMs, defMa); // records instead of drawing
            }
            auto t6 = clock::now();
            if (i >= 5)
            {
                mImmAdv += us(t1 - t0);
                mDefAdv += us(t2 - t1);
                mImmRen += us(t4 - t3);
                mDefRec += us(t6 - t5);
            }
        }
        mImmAdv /= kReplays;
        mDefAdv /= kReplays;
        mImmRen /= kReplays;
        mDefRec /= kReplays;
    }

    printf("\n=== deferred-rendering benchmark (%d iters @ 60fps) ===\n",
           iters);
    printf("scene resolution: %dx%d, 1 cell\n", cellSize, cellSize);
    printf("\n-- per-frame timing (microseconds) --\n");
    printf("  immediate  (main thread, record + GPU submit) : %9.1f us\n",
           immUs);
    printf("  deferred   RECORD only (main thread)          : %9.1f us  "
           "(%.2fx immediate)\n",
           recUs,
           recUs / immUs);
    printf("  deferred   REPLAY  cold (recreate every frame): %9.1f us  "
           "(%.2fx immediate)\n",
           coldUs,
           coldUs / immUs);
    printf("  deferred   REPLAY  steady (Ore resident)      : %9.1f us  "
           "(%.2fx immediate)  [MEASURED]\n",
           steadyUs,
           steadyUs / immUs);
    printf("    Ore shader/pipeline recompile saved/frame   : %9.1f us\n",
           coldUs - steadyUs);
    printf("\n-- serialized stream size --\n");
    printf("  2D  ordered stream, first frame (creates+draws): %9zu B  "
           "(%.1f KB)\n",
           cold2D,
           cold2D / 1024.0);
    printf("  Ore ordered stream, first frame (creates+passes): %9zu B  "
           "(%.1f KB)\n",
           coldOre.total(),
           coldOre.total() / 1024.0);
    printf("  steady per-frame (crosses every frame)        : %9.0f B  "
           "(%.2f KB)\n",
           perFrameBytes,
           perFrameBytes / 1024.0);
    printf("    2D   draws (steady, creates amortized) : %9.0f B\n",
           perFrame2D);
    if (pure2D)
    {
        printf("\n-- phase breakdown (pure-2D, single clean frame, us) --\n");
        printf("  IMMEDIATE (all on the main thread):\n");
        printf("    advance (anim / IK / skin / databind) : %8.1f us\n",
               immAdv);
        printf("    render CPU (issue draw calls)         : %8.1f us\n",
               immCpu);
        printf("    flush + present (feed the GPU)        : %8.1f us\n",
               immGpu);
        printf("    total                                 : %8.1f us\n",
               immAdv + immCpu + immGpu);
        printf("  DEFERRED:\n");
        printf("    PRODUCER (main): advance (+serialize) : %8.1f us  "
               "(immediate advance was %.1f)\n",
               repAdv,
               immAdv);
        printf("    PRODUCER (main): record draw commands : %8.1f us  "
               "(immediate draw-CPU was %.1f)\n",
               repRecDraw,
               immCpu);
        printf("    PRODUCER total (main thread)          : %8.1f us\n",
               repAdv + repRecDraw);
        printf("    CONSUMER (render): replay CPU         : %8.1f us  "
               "(parse + apply + draw calls)\n",
               repCpu);
        printf("    CONSUMER (render): flush + present    : %8.1f us\n",
               repGpu);
        printf("    consumer total (render thread)        : %8.1f us\n",
               repCpu + repGpu);
        printf("  deltas:\n");
        printf("    GPU feed: replay vs immediate         : %+8.1f us  "
               "(should be ~0 — identical work)\n",
               repGpu - immGpu);
        printf("    parse/apply tax: replayCPU - immCPU   : %+8.1f us\n",
               repCpu - immCpu);
        printf("    advance moved off render thread       : %8.1f us\n",
               immAdv);
        printf(
            "\n-- matched-frame serializer cost (same animation state) --\n");
        printf("    advance: immediate %.1f vs deferred %.1f  "
               "=> serializer-in-advance %+.1f us\n",
               mImmAdv,
               mDefAdv,
               mDefAdv - mImmAdv);
        printf("    draws  : immediate issue %.1f vs deferred record %.1f  "
               "=> %+.1f us\n",
               mImmRen,
               mDefRec,
               mDefRec - mImmRen);
        printf("    total serializer overhead vs immediate: %+.1f us/frame\n",
               (mDefAdv - mImmAdv) + (mDefRec - mImmRen));
    }
    printf("=======================================================\n\n");
}

#endif // WITH_RIVE_SCRIPTING && RIVE_CANVAS

#endif // TESTING

/*
 * Copyright 2022 Rive
 */

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "gm.hpp"
#include "gm_runner.hpp"
#include "common/testing_window.hpp"
#include "common/test_harness.hpp"

#if defined(RIVE_ANDROID) && !defined(RIVE_UNREAL)
#include "common/rive_android_app.hpp"
#include <sys/system_properties.h>
#endif

#ifdef __EMSCRIPTEN__
#include "common/rive_wasm_app.hpp"
#include <emscripten/emscripten.h>
#endif

#include <functional>

using namespace rivegm;

std::vector<std::tuple<std::function<GM*(void)>, std::string>> gmRegistry;
// Deferred parity families. Each runs its immediate GM and every deferred
// variant in-process and requires byte identical frames, so the machinery
// carries no goldens of its own; the scenes' pixels are the renderer GMs' job.
std::vector<std::tuple<std::vector<std::function<GM*(void)>>, std::string>>
    parityRegistry;
extern "C" void gms_build_registry()
{
    // Only call gms_build_registry() once!
    assert(gmRegistry.empty());

#define MAKE_GM(NAME)                                                          \
    extern GM* RIVE_MACRO_CONCAT(make_, NAME)();                               \
    gmRegistry.emplace_back(RIVE_MACRO_CONCAT(make_, NAME), #NAME);

#define MAKE_PARITY_GM2(NAME, IMM, VAR)                                        \
    extern GM* RIVE_MACRO_CONCAT(make_, IMM)();                                \
    extern GM* RIVE_MACRO_CONCAT(make_, VAR)();                                \
    parityRegistry.emplace_back(                                               \
        std::vector<std::function<GM*(void)>>{RIVE_MACRO_CONCAT(make_, IMM),   \
                                              RIVE_MACRO_CONCAT(make_, VAR)},  \
        #NAME);

#define MAKE_PARITY_GM3(NAME, IMM, VAR1, VAR2)                                 \
    extern GM* RIVE_MACRO_CONCAT(make_, IMM)();                                \
    extern GM* RIVE_MACRO_CONCAT(make_, VAR1)();                               \
    extern GM* RIVE_MACRO_CONCAT(make_, VAR2)();                               \
    parityRegistry.emplace_back(                                               \
        std::vector<std::function<GM*(void)>>{RIVE_MACRO_CONCAT(make_, IMM),   \
                                              RIVE_MACRO_CONCAT(make_, VAR1),  \
                                              RIVE_MACRO_CONCAT(make_, VAR2)}, \
        #NAME);

    // Add slow GMs first so they get more time to run in a multiprocess
    // execution.
    MAKE_GM(hittest_nonZero)
    MAKE_GM(hittest_evenOdd)
    MAKE_GM(mandoline)
    MAKE_GM(lots_of_images)
    MAKE_GM(lots_of_images_sampled)
    MAKE_GM(feathertext_roboto)
    MAKE_GM(feathertext_montserrat)
    MAKE_GM(feathertext_roboto_mirrored)
    MAKE_GM(feathertext_montserrat_mirrored)

    // Add the normal (not slow) gms last.
    MAKE_GM(atlastypes)
    MAKE_GM(batchedconvexpaths)
    MAKE_GM(batchedtriangulations)
    MAKE_GM(bevel180strokes)
    MAKE_GM(beziers)
    MAKE_GM(bug615686)
    MAKE_GM(cliprectintersections)
    MAKE_GM(cliprects)
    MAKE_GM(clip_shapes_large_corners)
    MAKE_GM(clip_shapes_large_corners_feathered)
    MAKE_GM(clip_shapes_large_corners_feathered_blend)
    MAKE_GM(clip_shapes_small_corners)
    MAKE_GM(clip_shapes_small_corners_feathered)
    MAKE_GM(clip_shapes_small_corners_feathered_blend)
    MAKE_GM(concavepaths)
    MAKE_GM(convexpaths)
    MAKE_GM(convex_lineonly_ths)
    MAKE_GM(crbug_996140)
    MAKE_GM(cubicpath)
    MAKE_GM(cubicclosepath)
    MAKE_GM(clippedcubic)
    MAKE_GM(clippedcubic2)
    MAKE_GM(bug5099)
    MAKE_GM(bug6083)
    MAKE_GM(degengrad)
    MAKE_GM(dithertypes)
    MAKE_GM(dstreadshuffle)
    MAKE_GM(emptyclear)
    MAKE_GM(emptytransparentclear)
    MAKE_GM(feather_shapes)
    MAKE_GM(feather_corner)
    MAKE_GM(feather_roundcorner)
    MAKE_GM(feather_ellipse)
    MAKE_GM(feather_cusp)
    MAKE_GM(feather_strokes)
    MAKE_GM(image)
    MAKE_GM(image_filter_options)
    MAKE_GM(image_aa_border)
    MAKE_GM(image_lod)
    MAKE_GM(gamma_texture)
    MAKE_GM(gamma_correction_clip)
    MAKE_GM(interleavedfeather)
    MAKE_GM(interleavedfillrule)
    MAKE_GM(labyrinth_square)
    MAKE_GM(labyrinth_round)
    MAKE_GM(labyrinth_butt)
    MAKE_GM(lots_of_grads_simple)
    MAKE_GM(lots_of_grads_complex)
    MAKE_GM(lots_of_grad_spans)
    MAKE_GM(lots_of_grads_clipped)
    MAKE_GM(lots_of_grads_mixed)
    MAKE_GM(lots_of_tess_spans_stroke)
    MAKE_GM(mesh)
    MAKE_GM(mesh_ht_7)
    MAKE_GM(mesh_ht_1)
    MAKE_GM(mutating_fill_rule)
    MAKE_GM(oval)
    MAKE_GM(OverStroke)
    MAKE_GM(overfill_opaque)
    MAKE_GM(overfill_transparent)
    MAKE_GM(overfill_blendmodes)
    MAKE_GM(overstroke_opaque)
    MAKE_GM(overstroke_transparent)
    MAKE_GM(overstroke_blendmodes)
    MAKE_GM(parallelclips)
    MAKE_GM(pathfill)
    MAKE_GM(rotatedcubicpath)
    MAKE_GM(bug7792)
    MAKE_GM(path_stroke_clip_crbug1070835)
    MAKE_GM(path_skbug_11859)
    MAKE_GM(path_skbug_11886)
    MAKE_GM(poly_nonZero)
    MAKE_GM(poly_evenOdd)
    MAKE_GM(poly_clockwise)
    MAKE_GM(preserverendertarget)
    MAKE_GM(preserverendertarget_empty)
    MAKE_GM(rawtext)
    MAKE_GM(interleaved_subpasses_with_dst_blend)
    MAKE_GM(interleaved_subpasses_with_dst_blend2)
    MAKE_GM(rect)
    MAKE_GM(rect_grad)
    MAKE_GM(retrofittedcubictriangles)
    MAKE_GM(roundjoinstrokes)
    MAKE_GM(strokedlines)
    MAKE_GM(strokefill)
    MAKE_GM(bug339297)
    MAKE_GM(bug339297_as_clip)
    MAKE_GM(bug6987)
    MAKE_GM(strokes_round)
    MAKE_GM(strokes_poly)
    MAKE_GM(strokes3)
    MAKE_GM(strokes_zoomed)
    MAKE_GM(zero_control_stroke)
    MAKE_GM(zeroPath)
    MAKE_GM(teenyStrokes)
    MAKE_GM(CubicStroke)
    MAKE_GM(zerolinestroke)
    MAKE_GM(quadcap)
    MAKE_GM(inner_join_geometry)
    MAKE_GM(skbug12244)
    MAKE_GM(offscreen_render_target)
    MAKE_GM(offscreen_render_target_nonrenderable)
    MAKE_GM(offscreen_render_target_lum)
    MAKE_GM(offscreen_render_target_lum_nonrenderable)
    MAKE_GM(offscreen_render_target_transparent_clear)
    MAKE_GM(offscreen_render_target_nonrenderable_transparent_clear)
    MAKE_GM(offscreen_render_target_lum_transparent_clear)
    MAKE_GM(offscreen_render_target_lum_nonrenderable_transparent_clear)
    MAKE_GM(offscreen_render_target_preserve)
    MAKE_GM(offscreen_render_target_preserve_nonrenderable)
    MAKE_GM(offscreen_render_target_preserve_lum)
    MAKE_GM(offscreen_render_target_preserve_lum_nonrenderable)
    MAKE_GM(offscreen_virtual_tiles_nonrenderable)
    MAKE_GM(offscreen_virtual_tiles_preserve_nonrenderable)
    MAKE_GM(offscreen_virtual_tiles_lum_nonrenderable)
    MAKE_GM(offscreen_virtual_tiles_preserve_lum_nonrenderable)
    MAKE_GM(transparentclear)
    MAKE_GM(verycomplexgrad)
    MAKE_GM(widebuttcaps)
    MAKE_GM(xfermodes2)
    MAKE_GM(trickycubicstrokes_roundcaps)
    MAKE_GM(emptyfeather)
    MAKE_GM(feather_polyshapes)
    MAKE_GM(largeclippedpath_evenodd_nested)
    MAKE_GM(largeclippedpath_clockwise)
    MAKE_GM(largeclippedpath_winding)
    MAKE_GM(largeclippedpath_evenodd)
    MAKE_GM(largeclippedpath_winding_nested)
    MAKE_GM(largeclippedpath_clockwise_nested)
    MAKE_GM(negative_interior_triangles)
    MAKE_GM(negative_interior_triangles_as_clip)
    MAKE_GM(transparentclear_blendmode)
    MAKE_GM(emptystrokefeather)
    MAKE_GM(emptystroke)
    MAKE_GM(trickycubicstrokes)
    MAKE_GM(preserverendertarget_blendmode)
    MAKE_GM(trickycubicstrokes_feather)
#ifdef RIVE_CANVAS
    MAKE_GM(render_canvas_basic)
    MAKE_GM(render_canvas_persistence)
    MAKE_GM(render_canvas_prepass)
    MAKE_GM(render_canvas_prepass_multi)
#ifdef WITH_RIVE_SCRIPTING
    MAKE_GM(canvas_dag_chain)
    MAKE_GM(canvas_dag_chain_reversed)
    MAKE_GM(canvas_dag_cycle)
#endif
#if defined(ORE_BACKEND_METAL) || defined(ORE_BACKEND_D3D11) ||                \
    defined(ORE_BACKEND_D3D12) || defined(ORE_BACKEND_GL) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK) ||                    \
    defined(ORE_BACKEND_RHI)
    MAKE_GM(ore_triangle)
    MAKE_GM(ore_mrt)
    MAKE_GM(ore_depth)
    MAKE_GM(ore_cubemap)
    MAKE_GM(ore_image_view)
    MAKE_GM(ore_canvas_import)
    MAKE_GM(ore_binding_witness)
    MAKE_GM(ore_pool_rollover)
    MAKE_GM(ore_binding_mixed_kind)
    MAKE_GM(ore_binding_multi_group)
    MAKE_GM(ore_binding_shadow_sampler)
    MAKE_GM(ore_binding_shadow_sampler_d32)
    MAKE_GM(ore_binding_vs_texture)
    MAKE_GM(ore_binding_dynamic_ubo)
    MAKE_GM(ore_buffer_update_between_draws)
    MAKE_GM(ore_msaa_resolve)
    MAKE_GM(ore_blend_stencil)
    MAKE_GM(ore_array_upload)
    MAKE_GM(ore_layout_reuse)
    MAKE_GM(ore_layout_mismatch)
    MAKE_GM(ore_depth_only_pipeline)
    MAKE_PARITY_GM3(ore_deferred_replay,
                    ore_deferred_replay_immediate,
                    ore_deferred_replay,
                    ore_deferred_replay_inline)
    MAKE_PARITY_GM2(ore_deferred_multipass,
                    ore_deferred_multipass_immediate,
                    ore_deferred_multipass)
    MAKE_PARITY_GM3(ore_deferred_resource,
                    ore_deferred_resource_immediate,
                    ore_deferred_resource,
                    ore_deferred_resource_unified)
    MAKE_PARITY_GM2(ore_deferred_context,
                    ore_deferred_context_immediate,
                    ore_deferred_context)
    MAKE_PARITY_GM2(render_deferred_canvas,
                    render_deferred_canvas_immediate,
                    render_deferred_canvas)
#endif
#endif
    // 2D only so these are not gated on Ore.
    MAKE_PARITY_GM2(serialized_replay_2d,
                    serialized_replay_2d_immediate,
                    serialized_replay_2d)
    MAKE_PARITY_GM2(render_deferred_2d,
                    render_deferred_2d_immediate,
                    render_deferred_2d)
}

void GMRunner::dumpGM(GM* gm, const std::string& gmName)
{
    uint32_t width = gm->width();
    uint32_t height = gm->height();
    TestingWindow::Get()->resize(width, height);
    std::vector<uint8_t> pixels;
    if (m_verbose)
    {
        printf("[gms] Running %s...\n", gmName.c_str());
    }
    for (int l = 0; l < m_loopCount; l++)
    {
        gm->run(gmName.c_str(), &pixels);
    }
    assert(pixels.size() == height * width * 4);
    if (TestHarness::Instance().initialized())
    {
        TestHarness::Instance().savePNG({
            .name = gmName,
            .width = width,
            .height = height,
            .pixels = std::move(pixels),
        });
    }
    if (m_verbose)
    {
        printf("[gms] Sent %s.png\n", gmName.c_str());
    }
}

void GMRunner::runParityGM(
    const std::vector<std::function<rivegm::GM*(void)>>& makers,
    const std::string& name)
{
    if (m_verbose)
    {
        printf("[gms] Running parity %s...\n", name.c_str());
    }
    std::vector<uint8_t> immediate;
    std::vector<uint8_t> variant;
    for (size_t i = 0; i < makers.size(); ++i)
    {
        std::unique_ptr<GM> gm(makers[i]());
        if (!gm)
        {
            return;
        }
        TestingWindow::Get()->resize(gm->width(), gm->height());
        gm->onceBeforeDraw();
        std::vector<uint8_t>* out = i == 0 ? &immediate : &variant;
        out->clear();
        gm->run(name.c_str(), out);
        if (i == 0)
        {
            continue;
        }
        bool match = variant.size() == immediate.size();
        int worst = 0;
        if (match && m_parityMaxChannelDiff > 0)
        {
            for (size_t b = 0; b < variant.size(); ++b)
            {
                int diff = std::abs(static_cast<int>(variant[b]) -
                                    static_cast<int>(immediate[b]));
                worst = std::max(worst, diff);
            }
            match = worst <= m_parityMaxChannelDiff;
        }
        else if (match)
        {
            match = variant == immediate;
        }
        if (!match)
        {
            m_parityFailures++;
            fprintf(stderr,
                    "[gms] PARITY FAILED: %s variant %zu does not match the "
                    "immediate frame (worst channel diff %d, allowed %d)\n",
                    name.c_str(),
                    i,
                    worst,
                    m_parityMaxChannelDiff);
        }
    }
}

static bool contains(const std::string& str, const std::string& substr)
{
    auto pos = str.find(substr, 0);
    return pos < str.size();
}

void GMRunner::init()
{
    // Only one registry per process, however many runners walk it.
    if (gmRegistry.empty())
    {
        gms_build_registry();
    }
    m_nextGM = 0;
}

bool GMRunner::doFrame()
{
    // At most one GM per call, so a host that owns the main loop gets to tick
    // between them. GMs this process doesn't draw cost nothing, so skipping
    // them doesn't burn a frame.
    while (m_nextGM < gmRegistry.size())
    {
        const auto& [make_gm, gmName] = gmRegistry[m_nextGM++];

        // Scope the GM so that it destructs (and releases its resources) before
        // we call `onceAfterGM` which potentially tears down the entire display
        // devices (see: TestingWindowAndroidVulkan)
        {
            std::unique_ptr<GM> gm(make_gm());

            if (!gm)
            {
                continue;
            }
            if (m_match.size() && !contains(gmName, m_match))
            {
                continue; // This gm got filtered out by the '--match' argument.
            }
            if (!TestHarness::Instance().claimGMTest(gmName))
            {
                continue; // A different process already drew this gm.
            }
            gm->onceBeforeDraw();

            dumpGM(gm.get(), gmName);
        }

        // Allow the testing window to do any cleanup it might want to do
        // between GMs
        TestingWindow::Get()->onceAfterGM();

        if (m_interactive)
        {
            // Wait for any key if in interactive mode.
            TestingWindow::InputEventData inputEventData =
                TestingWindow::Get()->waitForInputEvent();
            // Anything that isn't a key press will not progress.
            while (inputEventData.eventType !=
                   TestingWindow::InputEvent::KeyPress)
            {
                inputEventData = TestingWindow::Get()->waitForInputEvent();
            }
        }
#if defined(RIVE_ANDROID) && !defined(RIVE_UNREAL)
        if (!rive_android_app_poll_once())
        {
            return false;
        }
#endif
#ifdef __EMSCRIPTEN__
        // Yield control back to the browser so it can process its event loop.
        emscripten_sleep(1);
#endif
        return true;
    }

    // Then the deferred parity families, one per call as well.
    while (m_nextParityGM < parityRegistry.size())
    {
        const auto& [makers, gmName] = parityRegistry[m_nextParityGM++];

        if (m_match.size() && !contains(gmName, m_match))
        {
            continue;
        }
        // Claimed like any GM so one worker runs each family; they store no
        // golden, the claim only partitions the work.
        if (!TestHarness::Instance().claimGMTest(gmName))
        {
            continue;
        }

        runParityGM(makers, gmName);
        TestingWindow::Get()->onceAfterGM();
#ifdef __EMSCRIPTEN__
        emscripten_sleep(1);
#endif
        return true;
    }

    if (m_parityFailures != 0)
    {
        fprintf(stderr, "[gms] %d parity failures\n", m_parityFailures);
        fflush(stderr);
    }

    return false;
}

static bool is_arg(const char arg[],
                   const char target[],
                   const char alt[] = nullptr)
{
    return !strcmp(arg, target) || (arg && !strcmp(arg, alt));
}

bool GMRunner::parseArgs(int argc,
                         const char* const argv[],
                         FrameRunner::LaunchOptions& options)
{
    // The gms have always defaulted to a windowed GL backend, unlike the other
    // tools.
    options.backend = TestingWindow::Backend::gl;
    options.visibility = TestingWindow::Visibility::window;

    bool wantVulkanSynchronizationValidation = false;
    bool onlyUbershaders = false;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--test_harness") == 0)
        {
            TestHarness::Instance().init(TCPClient::Connect(argv[++i]),
                                         m_pngThreads);
            continue;
        }
        if (strcmp(argv[i], "--sync-validation") == 0)
        {
            wantVulkanSynchronizationValidation = true;
            continue;
        }
        if (is_arg(argv[i], "--output", "-o"))
        {
            TestHarness::Instance().init(std::filesystem::path(argv[++i]),
                                         m_pngThreads);
            continue;
        }
        if (is_arg(argv[i], "--loop", "-l"))
        {
            m_loopCount = atoi(argv[++i]);
            continue;
        }
        if (is_arg(argv[i], "--match", "-m"))
        {
            m_match = argv[++i];
            continue;
        }
        if (is_arg(argv[i], "--fast-png", "-f"))
        {
            TestHarness::Instance().setPNGCompression(PNGCompression::fast_rle);
            continue;
        }
        if (is_arg(argv[i], "--interactive", "-i"))
        {
            m_interactive = true;
            continue;
        }
        if (is_arg(argv[i], "--backend", "-b"))
        {
            options.backend =
                TestingWindow::ParseBackend(argv[++i], &options.backendParams);
            continue;
        }
        if (is_arg(argv[i], "--headless", "-d"))
        {
            options.visibility = TestingWindow::Visibility::headless;
            continue;
        }
        if (is_arg(argv[i], "--verbose", "-v"))
        {
            m_verbose = true;
            continue;
        }
        if (is_arg(argv[i], "--only_ubershaders", "-u"))
        {
            onlyUbershaders = true;
            continue;
        }
        if (sscanf(argv[i], "-p%d", &m_pngThreads) == 1)
        {
            m_pngThreads = std::max(m_pngThreads, 1);
            continue;
        }
        printf("Unrecognized argument %s\n", argv[i]);
        return false;
    }

    options.backendParams.wantVulkanSynchronizationValidation =
        wantVulkanSynchronizationValidation;

    // By default we want the gms to use synchronously compiled
    // shaders/pipleines, in an attempt at determinism.
    options.backendParams.shaderCompilationMode =
        onlyUbershaders ? rive::gpu::ShaderCompilationMode::onlyUbershaders
                        : rive::gpu::ShaderCompilationMode::alwaysSynchronous;

    m_parityMaxChannelDiff = options.backendParams.atomic ? 8 : 0;

    return true;
}

// Draws every GM and then tears the process down. The unreal tool widget
// instead ticks doFrame() itself, one GM per engine frame, and owns the
// teardown.
#ifndef RIVE_UNREAL
static int gms_run_to_completion(int argc, const char* const argv[])
{
#ifdef _WIN32
    // Cause stdout and stderr to print immediately without buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif

    GMRunner runner;
    FrameRunner::LaunchOptions options;
    if (!runner.parseArgs(argc, argv, options))
    {
        return 1;
    }

    void* platformWindow = nullptr;
#if defined(RIVE_ANDROID) && !defined(RIVE_UNREAL)
    // Make sure the testing harness always gets initialized on Android so we
    // pipe stdout & stderr to the android log always get pngs.
    if (!TestHarness::Instance().initialized())
    {
        // Android introduced a lot of changes to external storage at v11. We
        // need to dump the pngs to different locations pre and post 11.
        char androidOSVersion[PROP_VALUE_MAX + 1] = {0};
        __system_property_get("ro.build.version.release", androidOSVersion);
        int androidOSVersionMajor = atoi(androidOSVersion);
        const char* pngLocation =
            androidOSVersionMajor >= 11
                ? "/sdcard/Pictures/rive_gms"
                : "/sdcard/Android/data/app.rive.android_tests/files/data/gms";
        TestHarness::Instance().init(std::filesystem::path(pngLocation), 4);
        // When the app is launched with no test harness, presumably via tap or
        // some other automation process, always do verbose output.
        runner.setVerbose(true);
    }
    // Render directly to the main window to give feedback.
    platformWindow = rive_android_app_wait_for_window();
    if (platformWindow != nullptr)
    {
        options.visibility = TestingWindow::Visibility::fullscreen;
    }
#endif
    TestingWindow::Init(options.backend,
                        options.backendParams,
                        options.visibility,
                        platformWindow);

    runner.init();
    while (runner.doFrame())
    {
    }

    const bool parityFailed = runner.parityFailures() != 0;

    gmRegistry.clear();
    parityRegistry.clear();
    TestingWindow::Destroy(); // Exercise our PLS teardown process now that
                              // we're done.
    TestHarness::Instance().shutdown();
#ifdef __EMSCRIPTEN__
    EM_ASM(if (window && window.close) window.close(););
#endif
    if (parityFailed)
    {
        abort();
    }
    return 0;
}

#endif // !RIVE_UNREAL

#if defined(RIVE_UNREAL)

extern "C" REGISTRY_HANDLE gms_get_registry_head() { return 0; }

extern "C" REGISTRY_HANDLE gms_registry_get_next(
    REGISTRY_HANDLE position_handle)
{
    assert(position_handle >= 0);
    if (position_handle == gmRegistry.size() - 1)
        return INVALID_REGISTRY;
    return position_handle + 1;
}

extern "C" bool gms_run_gm(REGISTRY_HANDLE position_handle)
{
    assert(position_handle >= 0);
    assert(position_handle < gmRegistry.size());
    const auto& [make_gm, gmName] = gmRegistry[position_handle];

    std::unique_ptr<GM> gm(make_gm());

    if (!gm)
    {
        return false;
    }

    gm->onceBeforeDraw();

    uint32_t width = gm->width();
    uint32_t height = gm->height();
    TestingWindow::Get()->resize(width, height);
    gm->run(gmName.c_str(), nullptr);

    return true;
}

extern "C" bool gms_registry_get_name(REGISTRY_HANDLE position_handle,
                                      std::string& name)
{
    assert(position_handle >= 0);
    assert(position_handle < gmRegistry.size());
    const auto& [make_gm, gmName] = gmRegistry[position_handle];

    name = gmName;
    return true;
}

extern "C" bool gms_registry_get_size(REGISTRY_HANDLE position_handle,
                                      size_t& width,
                                      size_t& height)
{
    assert(position_handle >= 0);
    assert(position_handle < gmRegistry.size());
    const auto& [make_gm, gmName] = gmRegistry[position_handle];

    std::unique_ptr<GM> gm(make_gm());

    width = 0;
    height = 0;

    if (!gm)
    {
        return false;
    }

    width = gm->width();
    height = gm->height();

    return true;
}

#endif // RIVE_UNREAL

// Unreal drives GMRunner directly; it has no entry point of its own here.
#ifndef RIVE_UNREAL

#if defined(EXTERN_TOOLS)
extern int rive_main(int argc, const char* argv[])
#elif defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
int gms_ios_main(int argc, const char* argv[])
#elif defined(RIVE_ANDROID)
int rive_android_main(int argc, const char* const* argv)
#elif defined(__EMSCRIPTEN__)
int rive_wasm_main(int argc, const char* const* argv)
#else
int main(int argc, const char* argv[])
#endif
{
    return gms_run_to_completion(argc, argv);
}

#endif // !RIVE_UNREAL

#endif

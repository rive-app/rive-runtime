/*
 * Copyright 2022 Rive
 */

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "gm.hpp"
#include "common/testing_window.hpp"
#include "common/test_harness.hpp"

#ifdef RIVE_ANDROID
#include "common/rive_android_app.hpp"
#include <sys/system_properties.h>
#endif

#ifdef __EMSCRIPTEN__
#include "common/rive_wasm_app.hpp"
#include <emscripten/emscripten.h>
#endif

#include <functional>

using namespace rivegm;

static bool verbose = false;
static int loopCount = 1;
std::vector<std::tuple<std::function<GM*(void)>, std::string>> gmRegistry;
extern "C" void gms_build_registry()
{
    // Only call gms_build_registry() once!
    assert(gmRegistry.empty());

#define MAKE_GM(NAME)                                                          \
    extern GM* RIVE_MACRO_CONCAT(make_, NAME)();                               \
    gmRegistry.emplace_back(RIVE_MACRO_CONCAT(make_, NAME), #NAME);

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
}

static void dump_gm(GM* gm, const std::string& name)
{
    uint32_t width = gm->width();
    uint32_t height = gm->height();
    TestingWindow::Get()->resize(width, height);
    std::vector<uint8_t> pixels;
    if (verbose)
    {
        printf("[gms] Running %s...\n", name.c_str());
    }
    for (int l = 0; l < loopCount; l++)
    {
        gm->run(&pixels);
    }
    assert(pixels.size() == height * width * 4);
    if (TestHarness::Instance().initialized())
    {
        TestHarness::Instance().savePNG({
            .name = name,
            .width = width,
            .height = height,
            .pixels = std::move(pixels),
        });
    }
    if (verbose)
    {
        printf("[gms] Sent %s.png\n", name.c_str());
    }
}

static bool contains(const std::string& str, const std::string& substr)
{
    auto pos = str.find(substr, 0);
    return pos < str.size();
}

static void dumpGMs(const std::string& match, bool interactive)
{

    for (const auto& [make_gm, name] : gmRegistry)
    {
        // Scope the GM so that it destructs (and releases its resources) before
        // we call `onceAfterGM` which potentially tears down the entire display
        // devices (see: TestingWindowAndroidVulkan)
        {
            std::unique_ptr<GM> gm(make_gm());

            if (!gm)
            {
                continue;
            }
            if (match.size() && !contains(name, match))
            {
                continue; // This gm got filtered out by the '--match' argument.
            }
            if (!TestHarness::Instance().claimGMTest(name))
            {
                continue; // A different process already drew this gm.
            }
            gm->onceBeforeDraw();

            dump_gm(gm.get(), name);
        }

        // Allow the testing window to do any cleanup it might want to do
        // between GMs
        TestingWindow::Get()->onceAfterGM();

        if (interactive)
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
            return;
        }
#endif
#ifdef __EMSCRIPTEN__
        // Yield control back to the browser so it can process its event loop.
        emscripten_sleep(1);
#endif
    }
}

static bool is_arg(const char arg[],
                   const char target[],
                   const char alt[] = nullptr)
{
    return !strcmp(arg, target) || (arg && !strcmp(arg, alt));
}

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
    gm->run(nullptr);

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

extern "C" int gms_main(int argc, const char* argv[])
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
#ifdef _WIN32
    // Cause stdout and stderr to print immediately without buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif

    const char* match = "";
    bool interactive = false;
    auto backend = TestingWindow::Backend::gl;
    TestingWindow::BackendParams backendParams;
    auto visibility = TestingWindow::Visibility::window;
    int pngThreads = 2;
    bool wantVulkanSynchronizationValidation = false;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--test_harness") == 0)
        {
            TestHarness::Instance().init(TCPClient::Connect(argv[++i]),
                                         pngThreads);
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
                                         pngThreads);
            continue;
        }
        if (is_arg(argv[i], "--loop", "-l"))
        {
            loopCount = atoi(argv[++i]);
            continue;
        }
        if (is_arg(argv[i], "--match", "-m"))
        {
            match = argv[++i];
            continue;
        }
        if (is_arg(argv[i], "--fast-png", "-f"))
        {
            TestHarness::Instance().setPNGCompression(PNGCompression::fast_rle);
            continue;
        }
        if (is_arg(argv[i], "--interactive", "-i"))
        {
            interactive = true;
            continue;
        }
        if (is_arg(argv[i], "--backend", "-b"))
        {
            backend = TestingWindow::ParseBackend(argv[++i], &backendParams);
            continue;
        }
        if (is_arg(argv[i], "--headless", "-d"))
        {
            visibility = TestingWindow::Visibility::headless;
            continue;
        }
        if (is_arg(argv[i], "--verbose", "-v"))
        {
            verbose = true;
            continue;
        }
        if (sscanf(argv[i], "-p%d", &pngThreads) == 1)
        {
            pngThreads = std::max(pngThreads, 1);
            continue;
        }
        printf("Unrecognized argument %s\n", argv[i]);
        return 1;
    }

    void* platformWindow = nullptr;
    backendParams.wantVulkanSynchronizationValidation =
        wantVulkanSynchronizationValidation;

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
        verbose = true;
    }
    // Render directly to the main window to give feedback.
    platformWindow = rive_android_app_wait_for_window();
    if (platformWindow != nullptr)
    {
        visibility = TestingWindow::Visibility::fullscreen;
    }
#endif
    TestingWindow::Init(backend, backendParams, visibility, platformWindow);
#ifndef RIVE_UNREAL // unreal calls this directly instead
    gms_build_registry();
#endif

    dumpGMs(std::string(match), interactive);

    gmRegistry.clear();
    TestingWindow::Destroy(); // Exercise our PLS teardown process now that
                              // we're done.
    TestHarness::Instance().shutdown();
#ifdef __EMSCRIPTEN__
    EM_ASM(if (window && window.close) window.close(););
#endif
    return 0;
}

#endif

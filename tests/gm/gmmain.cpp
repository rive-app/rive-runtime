/*
 * Copyright 2022 Rive
 */

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "gm.hpp"
#include "gmutils.hpp"
#include "utils/no_op_renderer.hpp"
#include "common/testing_window.hpp"
#include "common/test_harness.hpp"

#ifdef RIVE_ANDROID
#include "common/rive_android_app.hpp"
#include <sys/system_properties.h>
#endif

using namespace rivegm;

static bool verbose = false;

DECLARE_GMREGISTRY(batchedconvexpaths)
DECLARE_GMREGISTRY(batchedtriangulations)
DECLARE_GMREGISTRY(bevel180strokes)
DECLARE_GMREGISTRY(beziers)
DECLARE_GMREGISTRY(bug615686)
DECLARE_GMREGISTRY(cliprectintersections)
DECLARE_GMREGISTRY(cliprects)
DECLARE_GMREGISTRY(concavepaths)
DECLARE_GMREGISTRY(convexpaths)
DECLARE_GMREGISTRY(convex_lineonly_ths)
DECLARE_GMREGISTRY(crbug_996140)
DECLARE_GMREGISTRY(cubicpath)
DECLARE_GMREGISTRY(cubicclosepath)
DECLARE_GMREGISTRY(clippedcubic)
DECLARE_GMREGISTRY(clippedcubic2)
DECLARE_GMREGISTRY(bug5099)
DECLARE_GMREGISTRY(bug6083)
DECLARE_GMREGISTRY(degengrad)
DECLARE_GMREGISTRY(dstreadshuffle)
DECLARE_GMREGISTRY(emptyclear)
DECLARE_GMREGISTRY(feather_shapes)
DECLARE_GMREGISTRY(feather_corner)
DECLARE_GMREGISTRY(feather_roundcorner)
DECLARE_GMREGISTRY(feather_ellipse)
DECLARE_GMREGISTRY(feather_cusp)
DECLARE_GMREGISTRY(feather_strokes)
DECLARE_GMREGISTRY(image)
DECLARE_GMREGISTRY(image_aa_border)
DECLARE_GMREGISTRY(image_lod)
DECLARE_GMREGISTRY(interleavedfeather)
DECLARE_GMREGISTRY(interleavedfillrule)
DECLARE_GMREGISTRY(labyrinth_square)
DECLARE_GMREGISTRY(labyrinth_round)
DECLARE_GMREGISTRY(labyrinth_butt)
DECLARE_GMREGISTRY(lots_of_grads_simple)
DECLARE_GMREGISTRY(lots_of_grads_complex)
DECLARE_GMREGISTRY(lots_of_grad_spans)
DECLARE_GMREGISTRY(lots_of_grads_clipped)
DECLARE_GMREGISTRY(lots_of_grads_mixed)
DECLARE_GMREGISTRY(mesh)
DECLARE_GMREGISTRY(mesh_ht_7)
DECLARE_GMREGISTRY(mesh_ht_1)
DECLARE_GMREGISTRY(mutating_fill_rule)
DECLARE_GMREGISTRY(oval)
DECLARE_GMREGISTRY(OverStroke)
DECLARE_GMREGISTRY(parallelclips)
DECLARE_GMREGISTRY(pathfill)
DECLARE_GMREGISTRY(rotatedcubicpath)
DECLARE_GMREGISTRY(bug7792)
DECLARE_GMREGISTRY(path_stroke_clip_crbug1070835)
DECLARE_GMREGISTRY(path_skbug_11859)
DECLARE_GMREGISTRY(path_skbug_11886)
DECLARE_GMREGISTRY(poly_nonZero)
DECLARE_GMREGISTRY(poly_evenOdd)
DECLARE_GMREGISTRY(poly_clockwise)
DECLARE_GMREGISTRY(preserverendertarget)
DECLARE_GMREGISTRY(preserverendertarget_empty)
DECLARE_GMREGISTRY(rawtext)
DECLARE_GMREGISTRY(rect)
DECLARE_GMREGISTRY(rect_grad)
DECLARE_GMREGISTRY(retrofittedcubictriangles)
DECLARE_GMREGISTRY(roundjoinstrokes)
DECLARE_GMREGISTRY(strokedlines)
DECLARE_GMREGISTRY(strokefill)
DECLARE_GMREGISTRY(bug339297)
DECLARE_GMREGISTRY(bug339297_as_clip)
DECLARE_GMREGISTRY(bug6987)
DECLARE_GMREGISTRY(strokes_round)
DECLARE_GMREGISTRY(strokes_poly)
DECLARE_GMREGISTRY(strokes3)
DECLARE_GMREGISTRY(strokes_zoomed)
DECLARE_GMREGISTRY(zero_control_stroke)
DECLARE_GMREGISTRY(zeroPath)
DECLARE_GMREGISTRY(teenyStrokes)
DECLARE_GMREGISTRY(CubicStroke)
DECLARE_GMREGISTRY(zerolinestroke)
DECLARE_GMREGISTRY(quadcap)
DECLARE_GMREGISTRY(inner_join_geometry)
DECLARE_GMREGISTRY(skbug12244)
DECLARE_GMREGISTRY(texture_target_gl)
DECLARE_GMREGISTRY(texture_target_gl_preserve)
DECLARE_GMREGISTRY(transparentclear)
DECLARE_GMREGISTRY(verycomplexgrad)
DECLARE_GMREGISTRY(widebuttcaps)
DECLARE_GMREGISTRY(xfermodes2)
DECLARE_GMREGISTRY(trickycubicstrokes_roundcaps)
DECLARE_GMREGISTRY(lots_of_images)
DECLARE_GMREGISTRY(emptyfeather)
DECLARE_GMREGISTRY(largeclippedpath_evenodd_nested)
DECLARE_GMREGISTRY(hittest_nonZero)
DECLARE_GMREGISTRY(hittest_evenOdd)
DECLARE_GMREGISTRY(feather_polyshapes)
DECLARE_GMREGISTRY(largeclippedpath_clockwise)
DECLARE_GMREGISTRY(largeclippedpath_winding)
DECLARE_GMREGISTRY(largeclippedpath_evenodd)
DECLARE_GMREGISTRY(transparentclear_blendmode)
DECLARE_GMREGISTRY(emptystrokefeather)
DECLARE_GMREGISTRY(mandoline)
DECLARE_GMREGISTRY(emptystroke)
DECLARE_GMREGISTRY(trickycubicstrokes)
DECLARE_GMREGISTRY(texture_target_gl_preserve_lum)
DECLARE_GMREGISTRY(preserverendertarget_blendmode)
DECLARE_GMREGISTRY(largeclippedpath_winding_nested)
DECLARE_GMREGISTRY(largeclippedpath_clockwise_nested)
DECLARE_GMREGISTRY(trickycubicstrokes_feather)
DECLARE_GMREGISTRY(feathertext_roboto)
DECLARE_GMREGISTRY(feathertext_montserrat)

void gms_build_registry()
{
    CALL_GMREGISTRY(batchedconvexpaths)
    CALL_GMREGISTRY(batchedtriangulations)
    CALL_GMREGISTRY(bevel180strokes)
    CALL_GMREGISTRY(beziers)
    CALL_GMREGISTRY(bug615686)
    CALL_GMREGISTRY(cliprectintersections)
    CALL_GMREGISTRY(cliprects)
    CALL_GMREGISTRY(concavepaths)
    CALL_GMREGISTRY(convexpaths)
    CALL_GMREGISTRY(convex_lineonly_ths)
    CALL_GMREGISTRY(crbug_996140)
    CALL_GMREGISTRY(cubicpath)
    CALL_GMREGISTRY(cubicclosepath)
    CALL_GMREGISTRY(clippedcubic)
    CALL_GMREGISTRY(clippedcubic2)
    CALL_GMREGISTRY(bug5099)
    CALL_GMREGISTRY(bug6083)
    CALL_GMREGISTRY(degengrad)
    CALL_GMREGISTRY(dstreadshuffle)
    CALL_GMREGISTRY(emptyclear)
    CALL_GMREGISTRY(feather_shapes)
    CALL_GMREGISTRY(feather_corner)
    CALL_GMREGISTRY(feather_roundcorner)
    CALL_GMREGISTRY(feather_ellipse)
    CALL_GMREGISTRY(feather_cusp)
    CALL_GMREGISTRY(feather_strokes)
    CALL_GMREGISTRY(image)
    CALL_GMREGISTRY(image_aa_border)
    CALL_GMREGISTRY(image_lod)
    CALL_GMREGISTRY(interleavedfeather)
    CALL_GMREGISTRY(interleavedfillrule)
    CALL_GMREGISTRY(labyrinth_square)
    CALL_GMREGISTRY(labyrinth_round)
    CALL_GMREGISTRY(labyrinth_butt)
    CALL_GMREGISTRY(lots_of_grads_simple)
    CALL_GMREGISTRY(lots_of_grads_complex)
    CALL_GMREGISTRY(lots_of_grad_spans)
    CALL_GMREGISTRY(lots_of_grads_clipped)
    CALL_GMREGISTRY(lots_of_grads_mixed)
    CALL_GMREGISTRY(mesh)
    CALL_GMREGISTRY(mesh_ht_7)
    CALL_GMREGISTRY(mesh_ht_1)
    CALL_GMREGISTRY(mutating_fill_rule)
    CALL_GMREGISTRY(oval)
    CALL_GMREGISTRY(OverStroke)
    CALL_GMREGISTRY(parallelclips)
    CALL_GMREGISTRY(pathfill)
    CALL_GMREGISTRY(rotatedcubicpath)
    CALL_GMREGISTRY(bug7792)
    CALL_GMREGISTRY(path_stroke_clip_crbug1070835)
    CALL_GMREGISTRY(path_skbug_11859)
    CALL_GMREGISTRY(path_skbug_11886)
    CALL_GMREGISTRY(poly_nonZero)
    CALL_GMREGISTRY(poly_evenOdd)
    CALL_GMREGISTRY(poly_clockwise)
    CALL_GMREGISTRY(preserverendertarget)
    CALL_GMREGISTRY(preserverendertarget_empty)
    CALL_GMREGISTRY(rawtext)
    CALL_GMREGISTRY(rect)
    CALL_GMREGISTRY(rect_grad)
    CALL_GMREGISTRY(retrofittedcubictriangles)
    CALL_GMREGISTRY(roundjoinstrokes)
    CALL_GMREGISTRY(strokedlines)
    CALL_GMREGISTRY(strokefill)
    CALL_GMREGISTRY(bug339297)
    CALL_GMREGISTRY(bug339297_as_clip)
    CALL_GMREGISTRY(bug6987)
    CALL_GMREGISTRY(strokes_round)
    CALL_GMREGISTRY(strokes_poly)
    CALL_GMREGISTRY(strokes3)
    CALL_GMREGISTRY(strokes_zoomed)
    CALL_GMREGISTRY(zero_control_stroke)
    CALL_GMREGISTRY(zeroPath)
    CALL_GMREGISTRY(teenyStrokes)
    CALL_GMREGISTRY(CubicStroke)
    CALL_GMREGISTRY(zerolinestroke)
    CALL_GMREGISTRY(quadcap)
    CALL_GMREGISTRY(inner_join_geometry)
    CALL_GMREGISTRY(skbug12244)
    CALL_GMREGISTRY(texture_target_gl)
    CALL_GMREGISTRY(texture_target_gl_preserve)
    CALL_GMREGISTRY(transparentclear)
    CALL_GMREGISTRY(verycomplexgrad)
    CALL_GMREGISTRY(widebuttcaps)
    CALL_GMREGISTRY(xfermodes2)
    CALL_GMREGISTRY(trickycubicstrokes_roundcaps)
    CALL_GMREGISTRY(lots_of_images)
    CALL_GMREGISTRY(emptyfeather)
    CALL_GMREGISTRY(largeclippedpath_evenodd_nested)
    CALL_GMREGISTRY(hittest_nonZero)
    CALL_GMREGISTRY(hittest_evenOdd)
    CALL_GMREGISTRY(feather_polyshapes)
    CALL_GMREGISTRY(largeclippedpath_clockwise)
    CALL_GMREGISTRY(largeclippedpath_winding)
    CALL_GMREGISTRY(largeclippedpath_evenodd)
    CALL_GMREGISTRY(transparentclear_blendmode)
    CALL_GMREGISTRY(emptystrokefeather)
    CALL_GMREGISTRY(mandoline)
    CALL_GMREGISTRY(emptystroke)
    CALL_GMREGISTRY(trickycubicstrokes)
    CALL_GMREGISTRY(texture_target_gl_preserve_lum)
    CALL_GMREGISTRY(preserverendertarget_blendmode)
    CALL_GMREGISTRY(largeclippedpath_winding_nested)
    CALL_GMREGISTRY(largeclippedpath_clockwise_nested)
    CALL_GMREGISTRY(trickycubicstrokes_feather)
    CALL_GMREGISTRY(feathertext_roboto)
    CALL_GMREGISTRY(feathertext_montserrat)
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
    gm->run(&pixels);
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
    for (auto& registry : rivegm::RegistryList)
    {
        auto gm = registry_get(registry);
        if (!gm)
        {
            continue;
        }
        if (match.size() && !contains(registry.m_name, match))
        {
            continue; // This gm got filtered out by the '--match' argument.
        }
        if (!TestHarness::Instance().claimGMTest(registry.m_name))
        {
            continue; // A different process already drew this gm.
        }
        gm->onceBeforeDraw();

        dump_gm(gm.get(), registry.m_name);
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
#ifdef RIVE_ANDROID
        if (!rive_android_app_poll_once())
        {
            return;
        }
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

REGISTRY_HANDLE gms_get_registry_head() { return 0; }

REGISTRY_HANDLE gms_registry_get_next(REGISTRY_HANDLE position_handle)
{
    assert(position_handle >= 0);
    if (position_handle == rivegm::RegistryList.size() - 1)
        return INVALID_REGISTRY;
    return position_handle + 1;
}

bool gms_run_gm(REGISTRY_HANDLE position_handle)
{
    assert(position_handle >= 0);
    assert(position_handle < rivegm::RegistryList.size());
    const GMRegistry& position = rivegm::RegistryList[position_handle];

    auto gm = registry_get(position);
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

bool gms_registry_get_name(REGISTRY_HANDLE position_handle, std::string& name)
{
    assert(position_handle >= 0);
    assert(position_handle < rivegm::RegistryList.size());
    const GMRegistry& position = rivegm::RegistryList[position_handle];

    name = position.m_name;
    return true;
}

bool gms_registry_get_size(REGISTRY_HANDLE position_handle,
                           size_t& width,
                           size_t& height)
{
    assert(position_handle >= 0);
    assert(position_handle < rivegm::RegistryList.size());
    const GMRegistry& position = rivegm::RegistryList[position_handle];

    width = 0;
    height = 0;

    auto gm = registry_get(position);
    if (!gm)
    {
        return false;
    }

    width = gm->width();
    height = gm->height();

    return true;
}
int gms_main(int argc, const char* argv[])
#elif defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
int gms_ios_main(int argc, const char* argv[])
#elif defined(RIVE_ANDROID)
int rive_android_main(int argc, const char* const* argv)
#else
int main(int argc, const char* argv[])
#endif
{
#ifdef _WIN32
    // Cause stdout and stderr to print immediately without buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif

#ifndef RIVE_UNREAL // unreal calls this directly instead
    gms_build_registry();
#endif

    const char* match = "";
    bool interactive = false;
    auto backend = TestingWindow::Backend::gl;
    std::string gpuNameFilter;
    auto visibility = TestingWindow::Visibility::window;
    int pngThreads = 2;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--test_harness") == 0)
        {
            TestHarness::Instance().init(TCPClient::Connect(argv[++i]),
                                         pngThreads);
            continue;
        }
        if (is_arg(argv[i], "--output", "-o"))
        {
            TestHarness::Instance().init(std::filesystem::path(argv[++i]),
                                         pngThreads);
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
            backend = TestingWindow::ParseBackend(argv[++i], &gpuNameFilter);
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
#ifdef RIVE_ANDROID
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
    if (TestingWindow::IsGL(backend))
    {
        // Android can render directly to the main window in GL.
        // TOOD: add this support to TestingWindowAndroidVulkan as well.
        platformWindow = rive_android_app_wait_for_window();
        if (platformWindow != nullptr)
        {
            visibility = TestingWindow::Visibility::fullscreen;
        }
    }
#endif
    TestingWindow::Init(backend, visibility, gpuNameFilter, platformWindow);

    dumpGMs(std::string(match), interactive);

    TestingWindow::Destroy(); // Exercise our PLS teardown process now that
                              // we're done.

    TestHarness::Instance().shutdown();
    return 0;
}

#endif

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
#endif

using namespace rivegm;

static bool verbose = false;

static void dump_gm(GM* gm)
{
    uint32_t width = gm->width();
    uint32_t height = gm->height();
    TestingWindow::Get()->resize(width, height);
    std::vector<uint8_t> pixels;
    if (verbose)
    {
        printf("[gms] Running %s...\n", gm->name().c_str());
    }
    gm->run(&pixels);
    assert(pixels.size() == height * width * 4);
    if (TestHarness::Instance().initialized())
    {
        TestHarness::Instance().savePNG({
            .name = gm->name(),
            .width = width,
            .height = height,
            .pixels = std::move(pixels),
        });
    }
    if (verbose)
    {
        printf("[gms] Sent %s.png\n", gm->name().c_str());
    }
}

static bool contains(const std::string& str, const std::string& substr)
{
    auto pos = str.find(substr, 0);
    return pos < str.size();
}

static void dumpGMs(const std::string& match, bool interactive)
{
    for (auto head = rivegm::GMRegistry::head(); head; head = head->next())
    {
        auto gm = head->get()();
        if (!gm)
        {
            continue;
        }
        if (match.size() && !contains(gm->name(), match))
        {
            continue; // This gm got filtered out by the '--match' argument.
        }
        if (!TestHarness::Instance().claimGMTest(gm->name()))
        {
            continue; // A different process already drew this gm.
        }
        gm->onceBeforeDraw();

        dump_gm(gm.get());
        if (interactive)
        {
            // Wait for any key if in interactive mode.
            TestingWindow::Get()->getKey();
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

typedef const void* REGISTRY_HANDLE;

REGISTRY_HANDLE gms_get_registry_head() { return rivegm::GMRegistry::head(); }

REGISTRY_HANDLE gms_registry_get_next(REGISTRY_HANDLE position_handle)
{
    const GMRegistry* position =
        reinterpret_cast<const GMRegistry*>(position_handle);
    if (position == nullptr)
        return nullptr;
    return position->next();
}

bool gms_run_gm(REGISTRY_HANDLE gm_handle)
{
    const GMRegistry* position = reinterpret_cast<const GMRegistry*>(gm_handle);
    if (position == nullptr)
        return false;

    auto gm = position->get()();
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
    const GMRegistry* position =
        reinterpret_cast<const GMRegistry*>(position_handle);
    if (position == nullptr)
        return false;

    auto gm = position->get()();
    if (!gm)
    {
        return false;
    }

    name = gm->name();
    return true;
}

bool gms_registry_get_size(REGISTRY_HANDLE position_handle,
                           size_t& width,
                           size_t& height)
{
    const GMRegistry* position =
        reinterpret_cast<const GMRegistry*>(position_handle);
    if (position == nullptr)
        return false;

    width = 0;
    height = 0;

    auto gm = position->get()();
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
    if (!TestHarness::Instance().initialized())
    {
        // Make sure the testing window gets initialized on Android so we always
        // pipe stdout & stderr to the android log.
        TestHarness::Instance().init(std::filesystem::path("/sdcard/Pictures"),
                                     4);
        // When the app is launched with no test harness, presumably via tap or
        // some other automation process, always do verbose output.
        verbose = true;
    }
    if (TestingWindow::IsGL(backend))
    {
        // Android GMs can render directly to the main window in GL.
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

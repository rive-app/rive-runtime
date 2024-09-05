/*
 * Copyright 2022 Rive
 */

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "gm.hpp"
#include "gmutils.hpp"
#include "utils/no_op_renderer.hpp"
#include "skia/include/core/SkSurface.h"
#include "common/testing_window.hpp"
#include "common/test_harness.hpp"

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

static double bench_gm(GM* gm)
{
    auto info = SkImageInfo::MakeN32Premul(gm->width(), gm->height());
    auto surf = SkSurface::MakeRaster(info);

    // warm us up without timing
    const int WARM_UP_N = 3;
    for (int i = 0; i < WARM_UP_N; ++i)
    {
        rive::NoOpRenderer renderer;
        gm->draw(&renderer);
    }

    const int loops = gm->benchLoopCount();
    const double start = GetCurrSeconds();
    for (int i = 0; i < loops; ++i)
    {
        rive::NoOpRenderer renderer;
        gm->draw(&renderer);
    }
    return GetCurrSeconds() - start;
}

static bool contains(const std::string& str, const std::string& substr)
{
    auto pos = str.find(substr, 0);
    return pos < str.size();
}

static void dumpGMs(const std::string& match, bool bench, bool interactive)
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
        gm->setBenchMode(bench);
        gm->onceBeforeDraw();
        if (bench)
        {
            double dur = bench_gm(gm.get());
            printf("%s %g\n", gm->name().c_str(), dur);
        }
        else
        {
            dump_gm(gm.get());
        }
        if (interactive)
        {
            // Wait for any key if in interactive mode.
            TestingWindow::Get()->getKey();
        }
    }
}

static bool is_arg(const char arg[], const char target[], const char alt[] = nullptr)
{
    return !strcmp(arg, target) || (arg && !strcmp(arg, alt));
}

#if defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
int gms_ios_main(int argc, const char* argv[])
#elif defined(RIVE_ANDROID)
int rive_android_main(int argc, const char* const* argv, struct android_app*)
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
    bool bench = false;
    bool interactive = false;
    auto backend = TestingWindow::Backend::gl;
    std::string gpuNameFilter;
    auto visibility = TestingWindow::Visibility::window;
    int pngThreads = 2;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--test_harness") == 0)
        {
            TestHarness::Instance().init(TCPClient::Connect(argv[++i]), pngThreads);
            continue;
        }
        if (is_arg(argv[i], "--output", "-o"))
        {
            TestHarness::Instance().init(std::filesystem::path(argv[++i]), pngThreads);
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
        if (is_arg(argv[i], "--bench", "-B"))
        {
            bench = true;
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

    TestingWindow::Init(backend, visibility, gpuNameFilter);
    dumpGMs(std::string(match), bench, interactive);

    TestingWindow::Destroy(); // Exercise our PLS teardown process now that we're done.
    TestHarness::Instance().shutdown();
    return 0;
}

#endif

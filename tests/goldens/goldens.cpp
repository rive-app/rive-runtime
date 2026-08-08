/*
 * Copyright 2022 Rive
 */

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "goldens_shared.hpp"
#include "goldens_runner.hpp"
#include "common/tcp_client.hpp"
#include "common/rive_mgr.hpp"
#include "common/write_png_file.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef RIVE_ANDROID
#include "common/rive_android_app.hpp"
#endif

#ifdef __EMSCRIPTEN__
#include "common/rive_wasm_app.hpp"
#include <emscripten/emscripten.h>
#endif

constexpr static int kWindowTargetSize = 1600;

GoldensArguments s_args;

// RIVE_GOLDENS_ADVANCE=N advances N frames at sixty fps before rendering.
static void advanceScene(rive::Scene* scene)
{
    const char* a = goldens_getenv("RIVE_GOLDENS_ADVANCE");
    int frames = a ? atoi(a) : 0;
    if (frames <= 0)
    {
        scene->advanceAndApply(0);
        return;
    }
    for (int i = 0; i < frames; ++i)
        scene->advanceAndApply(1.0f / 60.0f);
}

void dumpPixelsAsPng(const char* rivName,
                     int windowWidth,
                     int windowHeight,
                     std::vector<uint8_t> pixels)
{
    assert(pixels.size() ==
           static_cast<size_t>(windowHeight) * windowWidth * 4);
    std::ostringstream imageName;
    imageName
        << std::filesystem::path(rivName).filename().stem().generic_string();
    if (s_args.rows() != 1 || s_args.cols() != 1)
    {
        imageName << '.' << s_args.cols() << 'x' << s_args.rows() << '.';
    }
    TestHarness::Instance().savePNG({
        .name = imageName.str(),
        .width = static_cast<uint32_t>(windowWidth),
        .height = static_cast<uint32_t>(windowHeight),
        .pixels = std::move(pixels),
    });
    if (s_args.verbose())
    {
        printf("[goldens] Sent %s\n",
               std::filesystem::path(imageName.str())
                   .replace_extension("png")
                   .generic_string()
                   .c_str());
    }
}

static bool render_and_dump_png(
    int cellSize,
    const char* rivName,
    rive::Scene* scene,
    rive::Artboard* artboard = nullptr,
    rive::cmd::DeferredSession* deferredSession = nullptr)
{
    // onceAfterGM can tear down the window between rivs, size every run.
    TestingWindow::Get()->resize(cellSize * s_args.cols(),
                                 cellSize * s_args.rows());

    if (s_args.verbose())
    {
        printf("[goldens] Running %s...\n", rivName);
    }
    try
    {
        const int frames = s_args.cols() * s_args.rows();
        const double duration = scene->durationSeconds();
        const double frameDuration = duration / frames;
        const rive::AABB cellBounds = rive::AABB(0, 0, cellSize, cellSize);

#if defined(WITH_RIVE_SCRIPTING) && defined(RIVE_CANVAS)
        // Deferred mode records the screen and Ore through the session, then
        // replays synchronously per grid cell. The cadence mirrors the
        // immediate path below so the output must be byte identical.
        if (deferredSession != nullptr && artboard != nullptr)
        {
            advanceScene(scene);
            rive::cmd::DeferredReplayer replayer;
            for (int y = 0; y < s_args.rows(); ++y)
            {
                for (int x = 0; x < s_args.cols(); ++x)
                {
                    bool first = (x | y) == 0;
                    if (!first)
                    {
                        scene->advanceAndApply(frameDuration);
                    }
                    deferredSession->recordOreReplayMarker();

                    auto screenRec = deferredSession->makeScreenRenderer();
                    screenRec->save();
                    screenRec->translate(x * cellSize, y * cellSize);
                    screenRec->align(rive::Fit::cover,
                                     rive::Alignment::center,
                                     cellBounds,
                                     scene->bounds());
                    artboard->drawInternal(screenRec.get());
                    screenRec->restore();

                    // Snapshot replay is the same path a threaded consumer
                    // takes.
                    rive::cmd::DeferredFrame frame =
                        rive::cmd::snapshotFrame(*deferredSession);
                    deferredSession->resetFrame();
                    auto sink = goldensFrameSink(first);
                    replayer.replayFrame(frame, sink);

                    bool last =
                        y == s_args.rows() - 1 && x == s_args.cols() - 1;
                    if (!last)
                    {
                        TestingWindow::Get()->endFrame();
                    }
                }
            }

            int windowWidth = s_args.cols() * cellSize;
            int windowHeight = s_args.rows() * cellSize;
            std::vector<uint8_t> pixels;
            TestingWindow::Get()->endFrame(&pixels);
            dumpPixelsAsPng(rivName,
                            windowWidth,
                            windowHeight,
                            std::move(pixels));
            return true;
        }
#endif

        // Render the scene in a grid.
        advanceScene(scene);
        auto renderer =
            TestingWindow::Get()->beginFrame({.clearColor = 0xffffffff});
        renderer->save();
        for (int y = 0; y < s_args.rows(); ++y)
        {
            for (int x = 0; x < s_args.cols(); ++x)
            {
                if ((x | y) != 0)
                {
                    TestingWindow::Get()->endFrame();
                    scene->advanceAndApply(frameDuration);
                    TestingWindow::Get()->beginFrame({.doClear = false});
                }

                renderer->save();

                renderer->translate(x * cellSize, y * cellSize);
                renderer->align(rive::Fit::cover,
                                rive::Alignment::center,
                                cellBounds,
                                scene->bounds());

                if (artboard != nullptr)
                {
                    artboard->drawInternal(renderer.get());
                }
                else
                {
                    scene->draw(renderer.get());
                }

                renderer->restore();
            }
        }
        renderer->restore();

        // Save the png.
        int windowWidth = s_args.cols() * cellSize;
        int windowHeight = s_args.rows() * cellSize;
        std::vector<uint8_t> pixels;
        TestingWindow::Get()->endFrame(&pixels);
        dumpPixelsAsPng(rivName, windowWidth, windowHeight, std::move(pixels));

        if (s_args.interactive())
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
    }
    catch (const char* msg)
    {
        fprintf(stderr, "%s: error: %s\n", rivName, msg);
        abort();
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "%s: error: %s\n", rivName, e.what());
        abort();
    }
    catch (...)
    {
        fprintf(stderr, "error rendering %s\n", rivName);
        abort();
    }
    return true;
}

static bool process_single_golden_file(const std::string file, int cellSize)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream.good())
    {
        throw "Bad file";
    }

    std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(stream), {});
#if defined(WITH_RIVE_SCRIPTING) && defined(RIVE_CANVAS)
    if (const char* n = goldens_getenv("RIVE_GOLDENS_BENCH"))
    {
        int iters = atoi(n);
        if (iters <= 0)
            iters = 200;
        run_benchmark(bytes,
                      s_args.artboard().c_str(),
                      s_args.stateMachine().c_str(),
                      iters);
        return true;
    }
#endif

    bool ok;
    {
        RIVLoader riv(bytes,
                      s_args.artboard().c_str(),
                      s_args.stateMachine().c_str());
        ok = render_and_dump_png(cellSize,
                                 file.c_str(),
                                 riv.stateMachine(),
                                 riv.artboard(),
                                 riv.deferredSession());
    }
    // Between-GM cleanup can tear the device down, so the loader and its
    // recorded resources must already be gone.
    TestingWindow::Get()->onceAfterGM();
    return ok;
}

static bool is_riv_file(const std::filesystem::path& file)
{
    return strcmp(file.extension().string().c_str(), ".riv") == 0;
}

bool GoldensRunner::parseArgs(int argc,
                              const char* const argv[],
                              FrameRunner::LaunchOptions& options)
{
    try
    {
        s_args.parse(argc, argv);
    }
    catch (const args::Completion&)
    {
        return false;
    }
    catch (const args::Help&)
    {
        return false;
    }
    catch (const args::ParseError&)
    {
        m_exitCode = 1;
        return false;
    }
    catch (args::ValidationError)
    {
        m_exitCode = 1;
        return false;
    }

    // The goldens have always defaulted to a windowed GL backend, like the gms.
    options.backend =
        s_args.backend().empty()
            ? TestingWindow::Backend::gl
            : TestingWindow::ParseBackend(s_args.backend().c_str(),
                                          &options.backendParams);

    // For determinism, default to always using synchronously-compiled
    // shaders
    options.backendParams.shaderCompilationMode =
        s_args.onlyUbershaders()
            ? rive::gpu::ShaderCompilationMode::onlyUbershaders
            : rive::gpu::ShaderCompilationMode::alwaysSynchronous;

    options.visibility = s_args.headless() ? TestingWindow::Visibility::headless
                                           : TestingWindow::Visibility::window;

    return true;
}

void GoldensRunner::init()
{
    if (!s_args.testHarness().empty())
    {
        TestHarness::Instance().init(
            TCPClient::Connect(s_args.testHarness().c_str()),
            s_args.pngThreads());
    }
    else
    {
        TestHarness::Instance().init(
            std::filesystem::path(s_args.output().c_str()),
            s_args.pngThreads());
    }
    TestHarness::Instance().setPNGCompression(
        s_args.fastPNG() ? PNGCompression::fast_rle : PNGCompression::compact);

    m_cellSize = kWindowTargetSize / std::max(s_args.cols(), s_args.rows());
    TestingWindow::Get()->resize(m_cellSize * s_args.cols(),
                                 m_cellSize * s_args.rows());

    // The .rivs either stream in from the harness, or we walk them off disk.
    m_fromTestHarness = TestHarness::Instance().hasTCPConnection();
    if (m_fromTestHarness)
    {
        return;
    }

    try
    {
#ifndef RIVE_REMOTE_ONLY
        const std::filesystem::path& srcPath =
            std::filesystem::path(s_args.src().c_str());
        if (is_riv_file(srcPath))
        {
            m_localFiles.push_back(s_args.src());
        }
        else
        {
            // Try to process every riv in the src path dir
            try
            {
                for (const std::filesystem::directory_entry& file :
                     std::filesystem::directory_iterator(s_args.src()))
                {
                    const std::filesystem::path& filePath = file.path();
                    if (is_riv_file(filePath))
                    {
                        m_localFiles.push_back(filePath.string());
                    }
                }
            }
            catch (...)
            {
                // Not a directory
                throw "Bad src path";
            }
        }
#else
        throw "Remote only goldens require a connection.";
#endif
    }
    catch (const char* msg)
    {
        fprintf(stderr, "error: %s\n", msg);
        m_exitCode = -1;
        m_localFiles.clear();
    }
}

bool GoldensRunner::doFrame()
{
    // One .riv per call, so a host that owns the main loop gets to tick between
    // them.
    try
    {
        if (m_fromTestHarness)
        {
            std::string rivName;
            std::vector<uint8_t> rivBytes;
            if (!TestHarness::Instance().fetchRivFile(rivName, rivBytes))
            {
                return false; // The server is done sending .rivs.
            }

            bool ok;
            {
                RIVLoader riv(rivBytes,
                              nullptr /*default artboard*/,
                              nullptr /*default state machine*/);
                ok = render_and_dump_png(m_cellSize,
                                         rivName.c_str(),
                                         riv.stateMachine(),
                                         riv.artboard(),
                                         riv.deferredSession());
            }
            // Between-GM cleanup can tear the device down, so the loader and
            // its recorded resources must already be gone.
            TestingWindow::Get()->onceAfterGM();
            return ok;
        }

        if (m_nextFile >= m_localFiles.size())
        {
            return false;
        }
        return process_single_golden_file(m_localFiles[m_nextFile++],
                                          m_cellSize);
    }
    catch (const char* msg)
    {
        fprintf(stderr, "error: %s\n", msg);
        m_exitCode = -1;
        return false;
    }
}

// Renders every golden and then tears the process down. The unreal tool widget
// instead ticks doFrame() itself, one .riv per engine frame, and owns the
// teardown.
#ifndef RIVE_UNREAL
static int goldens_run_to_completion(int argc, const char* const argv[])
{
#ifdef _WIN32
    // Cause stdout and stderr to print immediately without buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif

    GoldensRunner runner;
    FrameRunner::LaunchOptions options;
    if (!runner.parseArgs(argc, argv, options))
    {
        return runner.exitCode();
    }

    void* platformWindow = nullptr;
#if defined(RIVE_ANDROID) && !defined(RIVE_UNREAL)
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

    TestingWindow::Destroy(); // Exercise our PLS teardown process now that
                              // we're done.
    TestHarness::Instance().shutdown();
#ifdef __EMSCRIPTEN__
    EM_ASM(if (window && window.close) window.close(););
#endif
    return runner.exitCode();
}

// Unreal drives GoldensRunner directly; it has no entry point of its own here.
#if defined(EXTERN_TOOLS)
int goldens_main(int argc, const char* argv[])
#elif defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
int goldens_ios_main(int argc, const char* argv[])
#elif defined(RIVE_ANDROID)
int rive_android_main(int argc, const char* const* argv)
#elif defined(__EMSCRIPTEN__)
int rive_wasm_main(int argc, const char* const* argv)
#else
int main(int argc, const char* argv[])
#endif
{
    return goldens_run_to_completion(argc, argv);
}

#endif // !RIVE_UNREAL

#endif

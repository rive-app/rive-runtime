/*
 * Copyright 2022 Rive
 */

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "goldens_arguments.hpp"
#include "common/test_harness.hpp"
#include "common/tcp_client.hpp"
#include "common/rive_mgr.hpp"
#include "common/testing_window.hpp"
#include "common/write_png_file.hpp"
#include "rive/artboard.hpp"
#include "rive/renderer.hpp"
#include "rive/file.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/static_scene.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef RIVE_ANDROID
#include "common/rive_android_app.hpp"
#endif

constexpr static int kWindowTargetSize = 1600;

GoldensArguments s_args;

static bool render_and_dump_png(int cellSize,
                                const char* rivName,
                                rive::Scene* scene)
{
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

        // Render the scene in a grid.
        auto renderer =
            TestingWindow::Get()->beginFrame({.clearColor = 0xffffffff});
        renderer->save();
        scene->advanceAndApply(0);
        scene->advanceAndApply(0);
        for (int y = 0; y < s_args.rows(); ++y)
        {
            for (int x = 0; x < s_args.cols(); ++x)
            {
                if ((x | y) != 0)
                {
                    TestingWindow::Get()->endFrame();
                    TestingWindow::Get()->beginFrame({.doClear = false});
                    scene->advanceAndApply(frameDuration);
                }

                renderer->save();

                renderer->translate(x * cellSize, y * cellSize);
                renderer->align(rive::Fit::cover,
                                rive::Alignment::center,
                                cellBounds,
                                scene->bounds());
                scene->draw(renderer.get());

                renderer->restore();
            }
        }
        renderer->restore();

        // Save the png.
        int windowWidth = s_args.cols() * cellSize;
        int windowHeight = s_args.rows() * cellSize;
        std::vector<uint8_t> pixels;
        TestingWindow::Get()->endFrame(&pixels);
        assert(pixels.size() == windowHeight * windowWidth * 4);
        std::ostringstream imageName;

        imageName << std::filesystem::path(rivName)
                         .filename()
                         .stem()
                         .generic_string();
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
    }
    catch (const char* msg)
    {
        fprintf(stderr, "%s: error: %s\n", rivName, msg);
        abort();
    }
    catch (...)
    {
        fprintf(stderr, "error rendering %s\n", rivName);
        abort();
    }
    return true;
}

class RIVLoader
{
public:
    RIVLoader(const std::vector<uint8_t>& rivBytes,
              const char* artboardName,
              const char* stateMachineName)
    {
        m_file = rive::File::import(rivBytes, TestingWindow::Get()->factory());
        if (m_file == nullptr)
        {
            throw "Bad riv file";
        }
        if (artboardName != nullptr && artboardName[0] != '\0')
        {
            m_artboard = m_file->artboardNamed(artboardName);
        }
        else
        {
            m_artboard = m_file->artboardDefault();
        }
        if (m_artboard == nullptr)
        {
            throw "Can't load artboard";
        }

        // Bind the default view model instance
        m_viewModelInstance = m_file->createViewModelInstance(m_artboard.get());
        m_artboard->bindViewModelInstance(m_viewModelInstance);

        if (stateMachineName != nullptr && stateMachineName[0] != '\0')
        {
            m_scene = m_artboard->stateMachineNamed(stateMachineName);
        }
        else
        {
            m_scene = m_artboard->defaultStateMachine();
        }

        if (m_scene == nullptr)
        {
            // This is a riv without any state machines. Just draw the artboard.
            m_scene = std::make_unique<rive::StaticScene>(m_artboard.get());
        }

        if (m_scene != nullptr && m_viewModelInstance != nullptr)
        {
            m_scene->bindViewModelInstance(m_viewModelInstance);
        }
    }

    rive::Scene* stateMachine() const { return m_scene.get(); }

private:
    std::unique_ptr<rive::File> m_file;
    std::unique_ptr<rive::ArtboardInstance> m_artboard;
    std::unique_ptr<rive::Scene> m_scene;
    rive::rcp<rive::ViewModelInstance> m_viewModelInstance;
};

static bool process_single_golden_file(const std::string file, int cellSize)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream.good())
    {
        throw "Bad file";
    }

    RIVLoader riv(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        s_args.artboard().c_str(),
        s_args.stateMachine().c_str());
    return render_and_dump_png(cellSize, file.c_str(), riv.stateMachine());
}

static bool is_riv_file(const std::filesystem::path& file)
{
    return strcmp(file.extension().string().c_str(), ".riv") == 0;
}

#if defined(RIVE_UNREAL)
int goldens_main(int argc, const char* argv[])
#elif defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)
int goldens_ios_main(int argc, const char* argv[])
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

    try
    {
        s_args.parse(argc, argv);
        TestingWindow::BackendParams backendParams;
        auto backend =
            s_args.backend().empty()
                ? TestingWindow::Backend::gl
                : TestingWindow::ParseBackend(s_args.backend().c_str(),
                                              &backendParams);
        auto visibility = s_args.headless()
                              ? TestingWindow::Visibility::headless
                              : TestingWindow::Visibility::window;
        void* platformWindow = nullptr;
#if defined(RIVE_ANDROID) && !defined(RIVE_UNREAL)
        // Only render directly to the main window on GL. Vulkan is experiencing
        // device losses on Pixel 6 when we render to the main window.
        if (backend == TestingWindow::Backend::gl)
        {
            platformWindow = rive_android_app_wait_for_window();
            if (platformWindow != nullptr)
            {
                visibility = TestingWindow::Visibility::fullscreen;
            }
        }
#endif
        TestingWindow::Init(backend, backendParams, visibility, platformWindow);

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
            s_args.fastPNG() ? PNGCompression::fast_rle
                             : PNGCompression::compact);

        int cellSize =
            kWindowTargetSize / std::max(s_args.cols(), s_args.rows());
        int windowWidth = cellSize * s_args.cols();
        int windowHeight = cellSize * s_args.rows();
        TestingWindow::Get()->resize(windowWidth, windowHeight);

        // First check if the --src argument is a TCP server instead of a file.
        if (TestHarness::Instance().hasTCPConnection())
        {
            // Loop until the server is done sending .rivs.
            std::string rivName;
            std::vector<uint8_t> rivBytes;
            while (TestHarness::Instance().fetchRivFile(rivName, rivBytes))
            {
                RIVLoader riv(rivBytes,
                              nullptr /*default artboard*/,
                              nullptr /*default state machine*/);
                if (!render_and_dump_png(cellSize,
                                         rivName.c_str(),
                                         riv.stateMachine()))
                {
                    return 0;
                }
            }
        }
        else
        {
            const std::filesystem::path& srcPath =
                std::filesystem::path(s_args.src().c_str());
            if (is_riv_file(srcPath))
            {
                // Render a single .riv file.
                if (!process_single_golden_file(s_args.src().c_str(), cellSize))
                {
                    return 0;
                }
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
                            if (!process_single_golden_file(filePath.string(),
                                                            cellSize))
                            {
                                return 0;
                            }
                        }
                    }
                }
                catch (...)
                {
                    // Not a directory
                    throw "Bad src path";
                }
            }
        }
    }
    catch (const args::Completion&)
    {
        return 0;
    }
    catch (const args::Help&)
    {
        return 0;
    }
    catch (const args::ParseError&)
    {
        return 1;
    }
    catch (args::ValidationError)
    {
        return 1;
    }
    catch (const char* msg)
    {
        fprintf(stderr, "error: %s\n", msg);
        return -1;
    }

    TestingWindow::Destroy(); // Exercise our PLS teardown process now that
                              // we're done.
    TestHarness::Instance().shutdown();
    return 0;
}

#endif

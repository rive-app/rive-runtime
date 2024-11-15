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
constexpr static int kWindowTargetSize = 1600;

GoldensArguments s_args;

static void render_and_dump_png(int cellSize,
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
        auto renderer = TestingWindow::Get()->beginFrame(0xffffffff);
        renderer->save();
        scene->advanceAndApply(0);
        for (int y = 0; y < s_args.rows(); ++y)
        {
            for (int x = 0; x < s_args.cols(); ++x)
            {
                if ((x | y) != 0)
                {
                    TestingWindow::Get()->endFrame();
                    TestingWindow::Get()->beginFrame(0, false);
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
            TestingWindow::Get()->getKey();
        }
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
    }

    rive::Scene* stateMachine() const { return m_scene.get(); }

private:
    std::unique_ptr<rive::File> m_file;
    std::unique_ptr<rive::ArtboardInstance> m_artboard;
    std::unique_ptr<rive::Scene> m_scene;
};

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
        std::string gpuNameFilter;
        auto backend =
            s_args.backend().empty()
                ? TestingWindow::Backend::gl
                : TestingWindow::ParseBackend(s_args.backend().c_str(),
                                              &gpuNameFilter);
        auto visibility = s_args.headless()
                              ? TestingWindow::Visibility::headless
                              : TestingWindow::Visibility::window;
        TestingWindow::Init(backend, visibility, gpuNameFilter);

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
                render_and_dump_png(cellSize,
                                    rivName.c_str(),
                                    riv.stateMachine());
            }
        }
        else
        {
            // Render a single .riv file.
            std::ifstream stream(s_args.src().c_str(), std::ios::binary);
            if (!stream.good())
            {
                throw "Bad file";
            }
            RIVLoader riv(
                std::vector<uint8_t>(std::istreambuf_iterator<char>(stream),
                                     {}),
                s_args.artboard().c_str(),
                s_args.stateMachine().c_str());
            render_and_dump_png(cellSize,
                                s_args.src().c_str(),
                                riv.stateMachine());
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

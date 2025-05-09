/*
 * Copyright 2025 Rive
 */

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "common/testing_window.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/command_queue.hpp"
#include "rive/command_server.hpp"
#include <stdio.h>
#include <fstream>

using namespace rive;

std::string rivName;
std::vector<uint8_t> rivBytes;
auto backend =
#ifdef __APPLE__
    TestingWindow::Backend::metal;
#else
    TestingWindow::Backend::vk;
#endif
std::string gpuNameFilter;

class
{
public:
    void push(char key)
    {
        std::unique_lock lock(m_mutex);
        m_keys.push_back(key);
        m_cv.notify_one();
    }

    char pop()
    {
        std::unique_lock lock(m_mutex);
        while (m_keys.empty())
            m_cv.wait(lock);
        char key = m_keys.front();
        m_keys.pop_front();
        return key;
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<char> m_keys;
} keyQueue;

static void main_thread(void* ptr)
{
    rcp<CommandQueue> commandBuffer(reinterpret_cast<CommandQueue*>(ptr));

    FileHandle fileHandle = commandBuffer->loadFile(std::move(rivBytes));

    ArtboardHandle artboardHandle =
        commandBuffer->instantiateDefaultArtboard(fileHandle);

    StateMachineHandle stateMachineHandle =
        commandBuffer->instantiateDefaultStateMachine(artboardHandle);

    auto drawLoop = [artboardHandle,
                     stateMachineHandle](CommandServer* server) {
        ArtboardInstance* artboard =
            server->getArtboardInstance(artboardHandle);
        if (artboard == nullptr)
        {
            return;
        }

        StateMachineInstance* stateMachine =
            server->getStateMachineInstance(stateMachineHandle);
        if (stateMachine == nullptr)
        {
            return;
        }

        stateMachine->advanceAndApply(1.f / 120);

        std::unique_ptr<Renderer> renderer = TestingWindow::Get()->beginFrame({
            .clearColor = 0xff303030,
            .doClear = true,
        });

        renderer->save();

        uint32_t width = TestingWindow::Get()->width();
        uint32_t height = TestingWindow::Get()->height();

        // Draw the .riv.
        renderer->save();
        renderer->align(Fit::contain,
                        Alignment::center,
                        AABB(0, 0, width, height),
                        artboard->bounds());
        artboard->draw(renderer.get());
        renderer->restore();

        TestingWindow::Get()->endFrame();
    };

    DrawLoopHandle drawLoopHandle = commandBuffer->startDrawLoop(drawLoop);

    bool running = true;
    for (char key = '\0'; key != 'q'; key = keyQueue.pop())
    {
        if (key == 'p')
        {
            if (running)
            {
                commandBuffer->stopDrawLoop(drawLoopHandle);
                drawLoopHandle = RIVE_NULL_HANDLE;
            }
            else
            {
                drawLoopHandle = commandBuffer->startDrawLoop(drawLoop);
            }
            running = !running;
        }
    }

    commandBuffer->disconnect();
}

#ifdef RIVE_ANDROID
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

    for (int i = 0; i < argc; ++i)
    {
        if (strcmp(argv[i], "--backend") == 0 || strcmp(argv[i], "-b") == 0)
        {
            backend = TestingWindow::ParseBackend(argv[++i], &gpuNameFilter);
        }
        else if (argv[i][0] == '-' &&
                 argv[i][1] == 'b') // "-bvk" without a space.
        {
            backend = TestingWindow::ParseBackend(argv[i] + 2, &gpuNameFilter);
        }
        else
        {
            rivName = argv[i];
            std::ifstream rivStream(rivName, std::ios::binary);
            rivBytes =
                std::vector<uint8_t>(std::istreambuf_iterator<char>(rivStream),
                                     {});
        }
    }
    if (rivBytes.empty())
    {
        fprintf(stderr, "no .riv file specified");
        abort();
    }

    auto commandBuffer = make_rcp<CommandQueue>();
    std::thread mainThread(main_thread, safe_ref(commandBuffer.get()));

    TestingWindow::Init(backend,
                        TestingWindow::Visibility::window,
                        gpuNameFilter);

    // Since GLFW has to be used on the main thread, hack a "draw loop" that
    // polls GLFW inputs.
    commandBuffer->startDrawLoop([](CommandServer*) {
        TestingWindow::InputEventData inputEventData;
        while (TestingWindow::Get()->consumeInputEvent(inputEventData))
        {
            if (inputEventData.eventType == TestingWindow::InputEvent::KeyPress)
            {
                keyQueue.push(inputEventData.metadata.key);
            }
        }
        if (TestingWindow::Get()->shouldQuit())
        {
            keyQueue.push('q');
        }
    });

    CommandServer server(commandBuffer, TestingWindow::Get()->factory());
    server.serveUntilDisconnect();

    printf("\nShutting down\n");
    TestingWindow::Destroy();
    mainThread.join();
    return 0;
}

#endif

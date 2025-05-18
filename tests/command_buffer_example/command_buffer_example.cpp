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
#include <chrono>

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

std::atomic_bool running = true;
std::atomic_bool shouldDraw = true;

static void input_thread(void* ptr)
{
    rcp<CommandQueue> commandBuffer(reinterpret_cast<CommandQueue*>(ptr));

    for (char key = '\0'; key != 'q'; key = keyQueue.pop())
    {
        if (key == 'p')
        {
            shouldDraw.store(!shouldDraw);
        }
    }

    running.store(false);
    commandBuffer->disconnect();
}

// this is a seperate thread for testing, it could just as easily be in
// main_thread
static void draw_thread(void* ptr)
{
    rcp<CommandQueue> commandBuffer(reinterpret_cast<CommandQueue*>(ptr));

    FileHandle fileHandle = commandBuffer->loadFile(std::move(rivBytes));

    ArtboardHandle artboardHandle =
        commandBuffer->instantiateDefaultArtboard(fileHandle);

    StateMachineHandle stateMachineHandle =
        commandBuffer->instantiateDefaultStateMachine(artboardHandle);

    auto drawLoop = [artboardHandle,
                     stateMachineHandle](DrawKey drawKey,
                                         CommandServer* server) {
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

    auto drawKey = commandBuffer->createDrawKey();

    // 120 fps
    constexpr auto duration = std::chrono::microseconds(8330);
    auto lastFrame = std::chrono::high_resolution_clock::now();
    while (running.load())
    {
        std::this_thread::sleep_until(lastFrame + duration);
        lastFrame = std::chrono::high_resolution_clock::now();
        if (shouldDraw.load())
            commandBuffer->draw(drawKey, drawLoop);
    }
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
    std::thread inputThread(input_thread, safe_ref(commandBuffer.get()));
    std::thread drawThread(draw_thread, safe_ref(commandBuffer.get()));

    TestingWindow::Init(backend,
                        TestingWindow::Visibility::window,
                        gpuNameFilter);

    CommandServer server(commandBuffer, TestingWindow::Get()->factory());

    while (server.pollMessages())
    {
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

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    printf("\nShutting down\n");
    TestingWindow::Destroy();
    inputThread.join();
    drawThread.join();
    return 0;
}

#endif

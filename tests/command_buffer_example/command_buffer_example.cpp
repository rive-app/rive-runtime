/*
 * Copyright 2025 Rive
 */

// Don't compile this file as part of the "tests" project.
#ifndef TESTING

#include "common/testing_window.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/text/raw_text.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/command_queue.hpp"
#include "rive/command_server.hpp"
#include "rive/text/font_hb.hpp"
#include "assets/roboto_flex.ttf.hpp"
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

class SimpleFileListener : public CommandQueue::FileListener
{
public:
    virtual void onArtboardsListed(
        const FileHandle fileHandle,
        std::vector<std::string> artboardNames) override
    {
        // we can guarantee that this is the only listener that will receive
        // this callback, so it's fine to move the params
        m_artboardNames = std::move(artboardNames);
    }

    std::vector<std::string> m_artboardNames;
};

class SimpleArtboardListener : public CommandQueue::ArtboardListener
{
public:
    virtual void onStateMachinesListed(
        const ArtboardHandle fileHandle,
        std::vector<std::string> stateMachineNames) override
    {
        // we can guarantee that this is the only listener that will receive
        // this callback, so it's fine to move the params
        m_stateMachineNames = std::move(stateMachineNames);
    }

    std::vector<std::string> m_stateMachineNames;
};

class SimpleStateMachineListener : public CommandQueue::StateMachineListener
{};

std::atomic_bool running = true;
std::atomic_bool shouldDraw = true;

static void input_thread(rcp<CommandQueue> commandQueue)
{
    for (char key = '\0'; key != 'q'; key = keyQueue.pop())
    {
        if (key == 'p')
        {
            shouldDraw.store(!shouldDraw);
        }
    }

    running.store(false);
    commandQueue->disconnect();
}

// this is a seperate thread for testing, it could just as easily be in
// input_thread or the main thread
static void draw_thread(rcp<CommandQueue> commandQueue)
{
    SimpleFileListener fListener;
    SimpleArtboardListener aListener;
    SimpleStateMachineListener stmListener;

    FileHandle fileHandle =
        commandQueue->loadFile(std::move(rivBytes), &fListener);

    ArtboardHandle artboardHandle =
        commandQueue->instantiateDefaultArtboard(fileHandle, &aListener);

    StateMachineHandle stateMachineHandle =
        commandQueue->instantiateDefaultStateMachine(artboardHandle,
                                                     &stmListener);

    // TODO: This should really be decoded on the server thread. Do we want to
    // consider adding FontHandles?
    auto roboto = HBFont::Decode(assets::roboto_flex_ttf());

    auto drawLoop = [artboardHandle,
                     stateMachineHandle,
                     roboto,
                     &fListener,
                     &aListener](DrawKey drawKey, CommandServer* server) {
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

        auto factory = server->factory();

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

        renderer->save();
        renderer->translate(20, 0);

        auto black = factory->makeRenderPaint();
        black->color(0xffffffff);

        for (auto name : fListener.m_artboardNames)
        {
            auto paint = factory->makeRenderPaint();

            renderer->translate(0, 100);
            rive::RawText text(factory);
            text.maxWidth(400);
            text.sizing(rive::TextSizing::autoWidth);
            text.append(name + "\n", black, roboto, 72);
            text.render(renderer.get());
        }

        for (auto name : aListener.m_stateMachineNames)
        {
            auto paint = factory->makeRenderPaint();

            renderer->translate(0, 100);
            rive::RawText text(factory);
            text.maxWidth(400);
            text.sizing(rive::TextSizing::autoWidth);
            text.append(name + "\n", black, roboto, 72);
            text.render(renderer.get());
        }

        renderer->restore();

        TestingWindow::Get()->endFrame();
    };

    auto drawKey = commandQueue->createDrawKey();

    commandQueue->requestArtboardNames(fileHandle);
    commandQueue->requestStateMachineNames(artboardHandle);

    // 120 fps
    constexpr auto duration = std::chrono::microseconds(8330);
    auto lastFrame = std::chrono::high_resolution_clock::now();
    while (running.load())
    {
        std::this_thread::sleep_until(lastFrame + duration);
        lastFrame = std::chrono::high_resolution_clock::now();
        if (shouldDraw.load())
            commandQueue->draw(drawKey, drawLoop);

        // should happen in same thread as the listeners
        commandQueue->processMessages();
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

    auto commandQueue = make_rcp<CommandQueue>();
    std::thread inputThread(input_thread, commandQueue);
    std::thread drawThread(draw_thread, commandQueue);

    TestingWindow::Init(backend,
                        TestingWindow::Visibility::window,
                        gpuNameFilter);

    CommandServer server(commandQueue, TestingWindow::Get()->factory());

    while (server.processCommands())
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

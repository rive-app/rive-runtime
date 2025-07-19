/*
 * Copyright 2025 Rive
 */

#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/command_queue.hpp"
#include "rive/command_server.hpp"
#include "rive/file.hpp"
#include "common/render_context_null.hpp"
#include <fstream>

namespace rive
{
bool operator==(const ViewModelEnum& left, const ViewModelEnum& right)
{
    if (left.name != right.name ||
        left.enumerants.size() != right.enumerants.size())
        return false;

    for (int i = 0; i < left.enumerants.size(); ++i)
    {
        if (left.enumerants[i] != right.enumerants[i])
            return false;
    }

    return true;
}
bool operator!=(const ViewModelEnum& left, const ViewModelEnum& right)
{
    return !(left == right);
}
bool operator==(const PropertyData& l, const PropertyData& r)
{
    return l.name == r.name && l.type == r.type;
}
bool operator==(const CommandQueue::FileListener::ViewModelPropertyData& l,
                const CommandQueue::FileListener::ViewModelPropertyData& r)
{
    return l.name == r.name && l.type == r.type && l.metaData == r.metaData;
}
bool operator!=(const CommandQueue::FileListener::ViewModelPropertyData& l,
                const CommandQueue::FileListener::ViewModelPropertyData& r)
{
    return !(l == r);
}
} // namespace rive

template <typename t, size_t arraySize>
bool operator==(const std::vector<t>& left,
                const std::array<t, arraySize>& right)
{
    if (left.size() != arraySize)
        return false;

    for (int i = 0; i < left.size(); ++i)
    {
        if (left[i] != right[i])
            return false;
    }

    return true;
}

template <typename t>
bool operator==(const std::vector<t>& left, const std::vector<t>& right)
{
    if (left.size() != right.size())
        return false;

    for (int i = 0; i < left.size(); ++i)
    {
        if (left[i] != right[i])
            return false;
    }

    return true;
}

#include "catch.hpp"

using namespace rive;

static void server_thread(rcp<CommandQueue> commandQueue)
{
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();
    CommandServer server(std::move(commandQueue), nullContext.get());
    server.serveUntilDisconnect();
}

static void wait_for_server(CommandQueue* commandQueue)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool complete = false;
    std::unique_lock<std::mutex> lock(mutex);
    commandQueue->runOnce([&](CommandServer*) {
        std::unique_lock<std::mutex> serverLock(mutex);
        complete = true;
        cv.notify_one();
    });
    while (!complete)
        cv.wait(lock);
}

class TestPODStream : public RefCnt<TestPODStream>
{
public:
    static constexpr int MAG_NUMBER = 0x99;
    int m_number = MAG_NUMBER;
};

TEST_CASE("POD Stream RCP", "[PODStream]")
{
    PODStream stream;
    rcp<TestPODStream> t1 = make_rcp<TestPODStream>();
    TestPODStream* orig = t1.get();
    stream << t1;

    CHECK(t1.get() != nullptr);
    CHECK(t1.get() == orig);

    rcp<TestPODStream> t2;
    stream >> t2;
    CHECK(t2.get() == orig);
    CHECK(t2->m_number == TestPODStream::MAG_NUMBER);

    rcp<TestPODStream> t3;
    stream << t3;
    rcp<TestPODStream> t4;
    stream >> t4;
    CHECK(t4.get() == nullptr);
}

TEST_CASE("artboard management", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/two_artboards.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    commandQueue->runOnce([fileHandle](CommandServer* server) {
        CHECK(fileHandle != RIVE_NULL_HANDLE);
        CHECK(server->getFile(fileHandle) != nullptr);
    });

    ArtboardHandle artboardHandle1 =
        commandQueue->instantiateArtboardNamed(fileHandle, "One");
    ArtboardHandle artboardHandle2 =
        commandQueue->instantiateArtboardNamed(fileHandle, "Two");
    ArtboardHandle artboardHandle3 =
        commandQueue->instantiateArtboardNamed(fileHandle, "Three");
    commandQueue->runOnce([artboardHandle1, artboardHandle2, artboardHandle3](
                              CommandServer* server) {
        CHECK(artboardHandle1 != RIVE_NULL_HANDLE);
        CHECK(server->getArtboardInstance(artboardHandle1) != nullptr);
        CHECK(artboardHandle2 != RIVE_NULL_HANDLE);
        CHECK(server->getArtboardInstance(artboardHandle2) != nullptr);
        // An artboard named "Three" doesn't exist.
        CHECK(artboardHandle3 != RIVE_NULL_HANDLE);
        CHECK(server->getArtboardInstance(artboardHandle3) == nullptr);
    });

    // Deleting an invalid handle has no effect.
    commandQueue->deleteArtboard(artboardHandle3);
    commandQueue->deleteArtboard(artboardHandle2);
    commandQueue->runOnce([artboardHandle1, artboardHandle2, artboardHandle3](
                              CommandServer* server) {
        CHECK(server->getArtboardInstance(artboardHandle1) != nullptr);
        CHECK(server->getArtboardInstance(artboardHandle2) == nullptr);
        CHECK(server->getArtboardInstance(artboardHandle3) == nullptr);
    });

    // Deleting the file first should now delete the artboard as well.
    commandQueue->deleteFile(fileHandle);
    commandQueue->runOnce([fileHandle, artboardHandle1](CommandServer* server) {
        CHECK(server->getFile(fileHandle) == nullptr);
        CHECK(server->getArtboardInstance(artboardHandle1) == nullptr);
    });

    commandQueue->deleteArtboard(artboardHandle1);
    commandQueue->runOnce([artboardHandle1](CommandServer* server) {
        CHECK(server->getArtboardInstance(artboardHandle1) == nullptr);
    });

    commandQueue->disconnect();
    serverThread.join();
}

TEST_CASE("state machine management", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/multiple_state_machines.riv",
                         std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    ArtboardHandle artboardHandle =
        commandQueue->instantiateDefaultArtboard(fileHandle);
    commandQueue->runOnce([fileHandle, artboardHandle](CommandServer* server) {
        CHECK(fileHandle != RIVE_NULL_HANDLE);
        CHECK(server->getFile(fileHandle) != nullptr);
        CHECK(artboardHandle != RIVE_NULL_HANDLE);
        CHECK(server->getArtboardInstance(artboardHandle) != nullptr);
    });

    StateMachineHandle sm1 =
        commandQueue->instantiateStateMachineNamed(artboardHandle, "one");
    StateMachineHandle sm2 =
        commandQueue->instantiateStateMachineNamed(artboardHandle, "two");
    StateMachineHandle sm3 =
        commandQueue->instantiateStateMachineNamed(artboardHandle, "blahblah");
    commandQueue->runOnce([sm1, sm2, sm3](CommandServer* server) {
        CHECK(sm1 != RIVE_NULL_HANDLE);
        CHECK(server->getStateMachineInstance(sm1) != nullptr);
        CHECK(sm2 != RIVE_NULL_HANDLE);
        CHECK(server->getStateMachineInstance(sm2) != nullptr);
        // A state machine named "blahblah" doesn't exist.
        CHECK(sm3 != RIVE_NULL_HANDLE);
        CHECK(server->getStateMachineInstance(sm3) == nullptr);
    });

    commandQueue->deleteFile(fileHandle);
    commandQueue->deleteArtboard(artboardHandle);
    commandQueue->deleteStateMachine(sm1);
    commandQueue->runOnce(
        [fileHandle, artboardHandle, sm1, sm2](CommandServer* server) {
            CHECK(server->getFile(fileHandle) == nullptr);
            CHECK(server->getArtboardInstance(artboardHandle) == nullptr);
            CHECK(server->getStateMachineInstance(sm1) == nullptr);
            // Because of the dependencies, this should delete the statemachine
            // as well.
            CHECK(server->getStateMachineInstance(sm2) == nullptr);
        });

    commandQueue->deleteStateMachine(sm2);
    commandQueue->runOnce([sm2](CommandServer* server) {
        CHECK(server->getStateMachineInstance(sm2) == nullptr);
    });

    commandQueue->disconnect();
    serverThread.join();
}

TEST_CASE("default artboard & state machine", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/entry.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    ArtboardHandle artboardHandle =
        commandQueue->instantiateDefaultArtboard(fileHandle);
    StateMachineHandle smHandle =
        commandQueue->instantiateDefaultStateMachine(artboardHandle);
    commandQueue->runOnce([artboardHandle, smHandle](CommandServer* server) {
        rive::ArtboardInstance* artboard =
            server->getArtboardInstance(artboardHandle);
        REQUIRE(artboard != nullptr);
        CHECK(artboard->name() == "New Artboard");
        rive::StateMachineInstance* sm =
            server->getStateMachineInstance(smHandle);
        REQUIRE(sm != nullptr);
        CHECK(sm->name() == "State Machine 1");
    });

    // Using an empty string is the same as requesting the default.
    ArtboardHandle artboardHandle2 =
        commandQueue->instantiateArtboardNamed(fileHandle, "");
    StateMachineHandle smHandle2 =
        commandQueue->instantiateStateMachineNamed(artboardHandle2, "");
    commandQueue->runOnce([artboardHandle2, smHandle2](CommandServer* server) {
        rive::ArtboardInstance* artboard =
            server->getArtboardInstance(artboardHandle2);
        REQUIRE(artboard != nullptr);
        CHECK(artboard->name() == "New Artboard");
        rive::StateMachineInstance* sm =
            server->getStateMachineInstance(smHandle2);
        REQUIRE(sm != nullptr);
        CHECK(sm->name() == "State Machine 1");
    });

    commandQueue->disconnect();
    serverThread.join();
}

TEST_CASE("invalid handles", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/entry.riv", std::ios::binary);
    FileHandle goodFile = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    FileHandle badFile =
        commandQueue->loadFile(std::vector<uint8_t>(100 * 1024, 0));
    commandQueue->runOnce([goodFile, badFile](CommandServer* server) {
        CHECK(goodFile != RIVE_NULL_HANDLE);
        CHECK(server->getFile(goodFile) != nullptr);
        CHECK(badFile != RIVE_NULL_HANDLE);
        CHECK(server->getFile(badFile) == nullptr);
    });

    ArtboardHandle goodArtboard =
        commandQueue->instantiateArtboardNamed(goodFile, "New Artboard");
    ArtboardHandle badArtboard1 =
        commandQueue->instantiateDefaultArtboard(badFile);
    ArtboardHandle badArtboard2 =
        commandQueue->instantiateArtboardNamed(badFile, "New Artboard");
    ArtboardHandle badArtboard3 =
        commandQueue->instantiateArtboardNamed(goodFile, "blahblahblah");
    commandQueue->runOnce(
        [goodArtboard, badArtboard1, badArtboard2, badArtboard3](
            CommandServer* server) {
            CHECK(goodArtboard != RIVE_NULL_HANDLE);
            CHECK(server->getArtboardInstance(goodArtboard) != nullptr);
            CHECK(badArtboard1 != RIVE_NULL_HANDLE);
            CHECK(server->getArtboardInstance(badArtboard1) == nullptr);
            CHECK(badArtboard2 != RIVE_NULL_HANDLE);
            CHECK(server->getArtboardInstance(badArtboard2) == nullptr);
            CHECK(badArtboard3 != RIVE_NULL_HANDLE);
            CHECK(server->getArtboardInstance(badArtboard3) == nullptr);
        });

    StateMachineHandle goodSM =
        commandQueue->instantiateStateMachineNamed(goodArtboard,
                                                   "State Machine 1");
    StateMachineHandle badSM1 =
        commandQueue->instantiateStateMachineNamed(badArtboard2,
                                                   "State Machine 1");
    StateMachineHandle badSM2 =
        commandQueue->instantiateStateMachineNamed(goodArtboard,
                                                   "blahblahblah");
    StateMachineHandle badSM3 =
        commandQueue->instantiateDefaultStateMachine(badArtboard3);
    commandQueue->runOnce(
        [goodSM, badSM1, badSM2, badSM3](CommandServer* server) {
            CHECK(goodSM != RIVE_NULL_HANDLE);
            CHECK(server->getStateMachineInstance(goodSM) != nullptr);
            CHECK(badSM1 != RIVE_NULL_HANDLE);
            CHECK(server->getStateMachineInstance(badSM1) == nullptr);
            CHECK(badSM2 != RIVE_NULL_HANDLE);
            CHECK(server->getStateMachineInstance(badSM2) == nullptr);
            CHECK(badSM3 != RIVE_NULL_HANDLE);
            CHECK(server->getStateMachineInstance(badSM3) == nullptr);
        });

    commandQueue->deleteStateMachine(badSM3);
    commandQueue->deleteStateMachine(badSM2);
    commandQueue->deleteStateMachine(badSM1);
    commandQueue->deleteArtboard(badArtboard3);
    commandQueue->deleteArtboard(badArtboard2);
    commandQueue->deleteArtboard(badArtboard1);
    commandQueue->deleteFile(badFile);
    commandQueue->runOnce(
        [goodFile, goodArtboard, goodSM](CommandServer* server) {
            CHECK(server->getFile(goodFile) != nullptr);
            CHECK(server->getArtboardInstance(goodArtboard) != nullptr);
            CHECK(server->getStateMachineInstance(goodSM) != nullptr);
        });

    commandQueue->deleteStateMachine(goodSM);
    commandQueue->deleteArtboard(goodArtboard);
    commandQueue->deleteFile(goodFile);
    commandQueue->runOnce(
        [goodFile, goodArtboard, goodSM](CommandServer* server) {
            CHECK(server->getFile(goodFile) == nullptr);
            CHECK(server->getArtboardInstance(goodArtboard) == nullptr);
            CHECK(server->getStateMachineInstance(goodSM) == nullptr);
        });

    commandQueue->disconnect();
    serverThread.join();
}

TEST_CASE("draw loops", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::atomic_uint64_t frameNumber1 = 0, frameNumber2 = 0;
    std::atomic_uint64_t lastFrameNumber1, lastFrameNumber2;
    auto drawLoop1 = [&frameNumber1](DrawKey, CommandServer*) {
        ++frameNumber1;
    };
    auto drawLoop2 = [&frameNumber2](DrawKey, CommandServer*) {
        ++frameNumber2;
    };
    DrawKey loopHandle1 = commandQueue->createDrawKey();
    DrawKey loopHandle2 = commandQueue->createDrawKey();

    commandQueue->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 == 0);
        CHECK(frameNumber2 == 0);
        lastFrameNumber1 = frameNumber1.load();
        lastFrameNumber2 = frameNumber2.load();
    });

    commandQueue->draw(loopHandle1, drawLoop1);
    commandQueue->draw(loopHandle2, drawLoop2);

    do
    {
        wait_for_server(commandQueue.get());
    } while (frameNumber1 == lastFrameNumber1 ||
             frameNumber2 == lastFrameNumber2);

    commandQueue->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 > lastFrameNumber1);
        CHECK(frameNumber2 > lastFrameNumber2);
    });

    commandQueue->runOnce([&](CommandServer*) {
        lastFrameNumber1 = frameNumber1.load();
        lastFrameNumber2 = frameNumber2.load();
    });

    commandQueue->draw(loopHandle2, drawLoop2);

    do
    {
        wait_for_server(commandQueue.get());
    } while (frameNumber2 == lastFrameNumber2);
    commandQueue->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 == lastFrameNumber1);
        CHECK(frameNumber2 > lastFrameNumber2);
    });

    commandQueue->runOnce([&](CommandServer*) {
        lastFrameNumber1 = frameNumber1.load();
        lastFrameNumber2 = frameNumber2.load();
    });

    commandQueue->draw(loopHandle1, drawLoop1);

    do
    {
        wait_for_server(commandQueue.get());
    } while (frameNumber1 == lastFrameNumber1);
    commandQueue->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 > lastFrameNumber1);
        CHECK(frameNumber2 == lastFrameNumber2);
    });

    commandQueue->runOnce([&](CommandServer*) {
        lastFrameNumber1 = frameNumber1.load();
        lastFrameNumber2 = frameNumber2.load();
    });

    for (int i = 0; i < 10; ++i)
    {
        wait_for_server(commandQueue.get());
    }
    commandQueue->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 == lastFrameNumber1);
        CHECK(frameNumber2 == lastFrameNumber2);
    });

    commandQueue->draw(loopHandle1, drawLoop1);
    commandQueue->draw(loopHandle2, drawLoop2);

    do
    {
        wait_for_server(commandQueue.get());
    } while (frameNumber1 == lastFrameNumber1 ||
             frameNumber2 == lastFrameNumber2);
    commandQueue->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 > lastFrameNumber1);
        CHECK(frameNumber2 > lastFrameNumber2);
    });

    // Leave the draw loops running; test tearing down the command buffer with
    // active draw loops.
    commandQueue->disconnect();
    serverThread.join();
}

TEST_CASE("wait for server race condition", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    auto lambda = [](CommandServer*) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    };
    auto drawLambda = [](DrawKey, CommandServer*) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    };

    commandQueue->runOnce(lambda);

    commandQueue->draw(reinterpret_cast<DrawKey>(0), drawLambda);
    commandQueue->draw(reinterpret_cast<DrawKey>(1), drawLambda);
    commandQueue->draw(reinterpret_cast<DrawKey>(2), drawLambda);
    commandQueue->draw(reinterpret_cast<DrawKey>(3), drawLambda);
    commandQueue->draw(reinterpret_cast<DrawKey>(4), drawLambda);

    for (size_t i = 0; i < 100; ++i)

    {
        commandQueue->draw(reinterpret_cast<DrawKey>(i), drawLambda);
        commandQueue->runOnce(lambda);
    }
    for (size_t i = 0; i < 10; ++i)

    {
        wait_for_server(commandQueue.get());
    }

    commandQueue->disconnect();
    serverThread.join();
}

TEST_CASE("stopMesssages command", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();
    CommandServer server(commandQueue, nullContext.get());

    int test = 0;

    commandQueue->runOnce([&](CommandServer*) { ++test; });
    commandQueue->testing_commandLoopBreak();

    for (int i = 0; i < 10; i++)
    {
        commandQueue->runOnce([&](CommandServer*) { ++test; });
        if (i == 5)
            commandQueue->testing_commandLoopBreak();
    }

    server.processCommands();

    CHECK(test == 1);
    server.processCommands();
    CHECK(test == 7);
    server.processCommands();
    CHECK(test == 11);

    commandQueue->disconnect();
}

TEST_CASE("draw happens once per poll", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();
    CommandServer server(commandQueue, nullContext.get());
    int test = 0;

    auto drawLamda = [&test](DrawKey, CommandServer*) { ++test; };

    commandQueue->draw(0, drawLamda);
    commandQueue->draw(0, drawLamda);
    commandQueue->draw(0, drawLamda);
    commandQueue->draw(0, drawLamda);
    commandQueue->draw(0, drawLamda);

    server.processCommands();

    CHECK(test == 1);
    commandQueue->draw(0, drawLamda);
    server.processCommands();
    CHECK(test == 2);
    server.processCommands();
    CHECK(test == 2);

    commandQueue->disconnect();
}

TEST_CASE("disconnect", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();
    CommandServer server(commandQueue, nullContext.get());
    CHECK(!server.getWasDisconnected());

    CHECK(server.processCommands());
    commandQueue->disconnect();
    CHECK(!server.processCommands());
}

TEST_CASE("global asset set / remove", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();

    CommandServer server(commandQueue, nullContext.get());

    std::ifstream imageStream("assets/batdude.png", std::ios::binary);

    auto imageHandle = commandQueue->decodeImage(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(imageStream), {}));

    commandQueue->addGlobalImageAsset("image", imageHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(server->testing_globalImageNamed("image") == imageHandle);
    });

    commandQueue->removeGlobalImageAsset("image");
    commandQueue->removeGlobalFontAsset("font");

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalImageContains("image"));
    });

    // should not add nullptr or invalid values to maps
    commandQueue->addGlobalImageAsset("image", nullptr);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalImageContains("image"));
    });

    auto badImageHandle =
        commandQueue->decodeImage(std::vector<uint8_t>(1024, 0));

    commandQueue->addGlobalImageAsset("image", badImageHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalImageContains("image"));
    });

    commandQueue->removeGlobalImageAsset("blah");
    commandQueue->removeGlobalFontAsset("blah");
    commandQueue->removeGlobalAudioAsset("blah");

    commandQueue->addGlobalImageAsset("image", imageHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(server->testing_globalImageNamed("image") == imageHandle);
    });

    commandQueue->deleteImage(imageHandle);

    // These should be removed automatically
    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalImageContains("image"));
    });

#ifdef WITH_RIVE_AUDIO
    std::ifstream audioStream("assets/audio/what.wav", std::ios::binary);

    auto audioHandle = commandQueue->decodeAudio(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(audioStream), {}));

    commandQueue->addGlobalAudioAsset("audio", audioHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(server->testing_globalAudioNamed("audio") == audioHandle);
    });

    commandQueue->removeGlobalAudioAsset("audio");

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalAudioContains("audio"));
    });

    // should not add nullptr or invalid values to maps
    commandQueue->addGlobalAudioAsset("audio", nullptr);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalAudioContains("audio"));
    });

    auto badAudioHandle =
        commandQueue->decodeAudio(std::vector<uint8_t>(1024, 0));

    commandQueue->addGlobalAudioAsset("audio", badAudioHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalAudioContains("audio"));
    });

    commandQueue->addGlobalAudioAsset("audio", audioHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(server->testing_globalAudioNamed("audio") == audioHandle);
    });

    commandQueue->deleteAudio(audioHandle);

    // These should be removed automatically
    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalImageContains("image"));
        CHECK(!server->testing_globalFontContains("font"));
        CHECK(!server->testing_globalAudioContains("audio"));
    });
#endif

#ifdef WITH_RIVE_TEXT
    std::ifstream fontStream("assets/fonts/OpenSans-Italic.ttf",
                             std::ios::binary);

    auto fontHandle = commandQueue->decodeFont(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(fontStream), {}));

    commandQueue->addGlobalFontAsset("font", fontHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(server->testing_globalFontNamed("font") == fontHandle);
    });

    commandQueue->removeGlobalFontAsset("font");

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalFontContains("font"));
    });

    // should not add nullptr or invalid values to maps
    commandQueue->addGlobalFontAsset("font", nullptr);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalFontContains("font"));
    });

    auto badFontHandle =
        commandQueue->decodeFont(std::vector<uint8_t>(1024, 0));

    commandQueue->addGlobalFontAsset("font", badFontHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalFontContains("font"));
    });

    commandQueue->addGlobalFontAsset("font", fontHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(server->testing_globalFontNamed("font") == fontHandle);
    });

    commandQueue->deleteFont(fontHandle);

    // These should be removed automatically
    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(!server->testing_globalFontContains("font"));
    });
#endif

    server.processCommands();
    commandQueue->disconnect();
}

TEST_CASE("View Models", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    auto bviewModel =
        commandQueue->instantiateBlankViewModelInstance(fileHandle, "Test All");
    auto dviewModel =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle,
                                                          "Test All");
    auto nviewModel =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Test All",
                                                        "Test Alternate");
    auto neviewModel =
        commandQueue->referenceNestedViewModelInstance(bviewModel,
                                                       "Test Nested");
    commandQueue->insertViewModelInstanceListViewModel(bviewModel,
                                                       "Test List",
                                                       neviewModel,
                                                       0);
    auto listViewModel =
        commandQueue->referenceListViewModelInstance(bviewModel,
                                                     "Test List",
                                                     0);
    commandQueue->runOnce([bviewModel,
                           dviewModel,
                           nviewModel,
                           neviewModel,
                           listViewModel](CommandServer* server) {
        CHECK(server->getViewModelInstance(bviewModel) != nullptr);
        CHECK(server->getViewModelInstance(dviewModel) != nullptr);
        CHECK(server->getViewModelInstance(nviewModel) != nullptr);
        CHECK(server->getViewModelInstance(neviewModel) != nullptr);
        CHECK(server->getViewModelInstance(listViewModel) != nullptr);
        CHECK(server->getViewModelInstance(listViewModel) ==
              server->getViewModelInstance(neviewModel));
        auto list =
            server->getViewModelInstance(bviewModel)->propertyList("Test List");
        CHECK(list != nullptr);
        CHECK(list->instanceAt(0) ==
              server->getViewModelInstance(listViewModel));
    });

    commandQueue->removeViewModelInstanceListViewModel(bviewModel,
                                                       "Test List",
                                                       neviewModel);

    commandQueue->runOnce([bviewModel](CommandServer* server) {
        auto list =
            server->getViewModelInstance(bviewModel)->propertyList("Test List");
        CHECK(list != nullptr);
        CHECK(list->size() == 0);
    });

    auto bbviewModel =
        commandQueue->instantiateBlankViewModelInstance(fileHandle, "Blah");
    auto bdviewModel =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle, "Blah");
    auto bnviewModel =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Blah",
                                                        "Blah");
    auto bnnviewModel =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Blah",
                                                        "Test Alternate");
    auto bnbviewModel =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Test All",
                                                        "Blah");

    commandQueue->removeViewModelInstanceListViewModel(bviewModel,
                                                       "Blah",
                                                       neviewModel);

    commandQueue->removeViewModelInstanceListViewModel(bviewModel,
                                                       "Test List",
                                                       bnbviewModel);
    auto badPathNeviewModel =
        commandQueue->referenceNestedViewModelInstance(bviewModel, "Blah");

    auto badNeviewModel =
        commandQueue->referenceNestedViewModelInstance(bnnviewModel,
                                                       "Test Nested");

    auto badListViewModel =
        commandQueue->referenceListViewModelInstance(bviewModel, "Blah", 0);

    auto badListViewModel2 =
        commandQueue->referenceListViewModelInstance(bviewModel,
                                                     "Test List",
                                                     5);

    commandQueue->runOnce([bbviewModel,
                           bdviewModel,
                           bnviewModel,
                           bnnviewModel,
                           bnbviewModel,
                           badPathNeviewModel,
                           badNeviewModel,
                           badListViewModel,
                           badListViewModel2](CommandServer* server) {
        CHECK(server->getViewModelInstance(bbviewModel) == nullptr);
        CHECK(server->getViewModelInstance(bdviewModel) == nullptr);
        CHECK(server->getViewModelInstance(bnviewModel) == nullptr);
        CHECK(server->getViewModelInstance(bnnviewModel) == nullptr);
        CHECK(server->getViewModelInstance(bnbviewModel) == nullptr);
        CHECK(server->getViewModelInstance(badPathNeviewModel) == nullptr);
        CHECK(server->getViewModelInstance(badNeviewModel) == nullptr);
        CHECK(server->getViewModelInstance(badListViewModel) == nullptr);
        CHECK(server->getViewModelInstance(badListViewModel2) == nullptr);
    });

    auto artboard =
        commandQueue->instantiateArtboardNamed(fileHandle, "Test Artboard");

    auto abviewModel =
        commandQueue->instantiateBlankViewModelInstance(fileHandle, artboard);
    auto adviewModel =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle, artboard);
    auto anviewModel =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        artboard,
                                                        "Test Alternate");

    commandQueue->runOnce(
        [abviewModel, adviewModel, anviewModel](CommandServer* server) {
            CHECK(server->getViewModelInstance(abviewModel) != nullptr);
            CHECK(server->getViewModelInstance(adviewModel) != nullptr);
            CHECK(server->getViewModelInstance(anviewModel) != nullptr);
        });

    auto badArtboard =
        commandQueue->instantiateArtboardNamed(fileHandle, "Blah");

    auto babviewModel =
        commandQueue->instantiateBlankViewModelInstance(fileHandle,
                                                        badArtboard);
    auto badviewModel =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle,
                                                          badArtboard);
    auto banviewModel =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        badArtboard,
                                                        "Test Alternate");

    commandQueue->runOnce(
        [babviewModel, badviewModel, banviewModel](CommandServer* server) {
            CHECK(server->getViewModelInstance(babviewModel) == nullptr);
            CHECK(server->getViewModelInstance(badviewModel) == nullptr);
            CHECK(server->getViewModelInstance(banviewModel) == nullptr);
        });

    commandQueue->deleteViewModelInstance(badNeviewModel);
    commandQueue->deleteViewModelInstance(bviewModel);

    commandQueue->runOnce([neviewModel](CommandServer* server) {
        CHECK(server->getViewModelInstance(neviewModel) != nullptr);
    });

    commandQueue->deleteViewModelInstance(neviewModel);
    commandQueue->runOnce([neviewModel](CommandServer* server) {
        CHECK(server->getViewModelInstance(neviewModel) == nullptr);
    });

    commandQueue->disconnect();
    serverThread.join();
}

class ViewModelListedListenerCallback : public CommandQueue::FileListener
{
public:
    virtual void onViewModelsListed(
        const FileHandle fileHandle,
        uint64_t requestId,
        std::vector<std::string> viewModelNames) override
    {
        CHECK(requestId == m_requestId);
        CHECK(m_fileHandle == fileHandle);
        CHECK(viewModelNames.size() == std::size(m_expectedViewModelNames));

        for (int i = 0; i < std::size(m_expectedViewModelNames); ++i)
        {
            CHECK(viewModelNames[i] == m_expectedViewModelNames[i]);
        }

        m_hasCallback = true;
    }

    std::array<std::string, 6> m_expectedViewModelNames = {"Empty VM",
                                                           "Test All",
                                                           "Nested VM",
                                                           "State Transition",
                                                           "Alternate VM",
                                                           "Test Slash"};
    bool m_hasCallback = false;
    FileHandle m_fileHandle = RIVE_NULL_HANDLE;
    uint64_t m_requestId;
};

TEST_CASE("View Model Listed Listener", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);
    {
        ViewModelListedListenerCallback listener;

        std::ifstream stream("assets/data_bind_test_cmdq.riv",
                             std::ios::binary);
        FileHandle fileHandle = commandQueue->loadFile(
            std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
            &listener);

        listener.m_requestId = 2;
        listener.m_fileHandle = fileHandle;

        commandQueue->requestViewModelNames(fileHandle, listener.m_requestId);

        wait_for_server(commandQueue.get());

        commandQueue->processMessages();

        CHECK(listener.m_hasCallback);
    }

    {
        ViewModelListedListenerCallback listener;

        FileHandle fileHandle =
            commandQueue->loadFile(std::vector<uint8_t>(1024 * 1024, {}),
                                   &listener);

        listener.m_requestId = 2;
        listener.m_fileHandle = fileHandle;

        commandQueue->requestViewModelNames(fileHandle, listener.m_requestId);

        wait_for_server(commandQueue.get());

        commandQueue->processMessages();

        CHECK(!listener.m_hasCallback);
    }

    commandQueue->disconnect();
    serverThread.join();
}

class TestViewModelFileListener : public CommandQueue::FileListener
{
public:
    virtual void onViewModelInstanceNamesListed(
        const FileHandle handle,
        uint64_t requestId,
        std::string viewModelName,
        std::vector<std::string> instanceNames) override
    {
        CHECK(requestId == m_instanceRequestId);
        CHECK(m_handle == handle);
        CHECK(m_viewModelName == viewModelName);
        CHECK(instanceNames.size() == std::size(m_expectedInstanceNames));

        for (int i = 0; i < std::size(m_expectedInstanceNames); ++i)
        {
            CHECK(instanceNames[i] == m_expectedInstanceNames[i]);
        }

        m_hasInstanceCallback = true;
    }

    virtual void onViewModelPropertiesListed(
        const FileHandle handle,
        uint64_t requestId,
        std::string viewModelName,
        std::vector<CommandQueue::FileListener::ViewModelPropertyData>
            properties) override
    {
        CHECK(requestId == m_propertyRequestId);
        CHECK(m_handle == handle);
        CHECK(m_viewModelName == viewModelName);
        CHECK(properties.size() == std::size(m_expectedProperties));

        for (int i = 0; i < std::size(m_expectedProperties); ++i)
        {
            CHECK(properties[i] == m_expectedProperties[i]);
        }

        m_hasPropertyCallback = true;
    }

    std::array<std::string, 2> m_expectedInstanceNames = {"Test Default",
                                                          "Test Alternate"};
    std::array<CommandQueue::FileListener::ViewModelPropertyData, 10>
        m_expectedProperties = {
            CommandQueue::FileListener::ViewModelPropertyData{
                DataType::artboard,
                "Test Artboard"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::list,
                                                              "Test List"},
            CommandQueue::FileListener::ViewModelPropertyData{
                DataType::assetImage,
                "Test Image"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::number,
                                                              "Test Num"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::string,
                                                              "Test String"},
            CommandQueue::FileListener::ViewModelPropertyData{
                DataType::enumType,
                "Test Enum",
                "Test Enum Values"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::boolean,
                                                              "Test Bool"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::color,
                                                              "Test Color"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::trigger,
                                                              "Test Trigger"},
            CommandQueue::FileListener::ViewModelPropertyData{
                DataType::viewModel,
                "Test Nested"}};

    bool m_hasInstanceCallback = false;
    bool m_hasPropertyCallback = false;
    FileHandle m_handle = RIVE_NULL_HANDLE;
    std::string m_viewModelName = "Test All";
    uint64_t m_instanceRequestId = 2;
    uint64_t m_propertyRequestId = 3;
};

TEST_CASE("View Model Listener", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);
    {
        TestViewModelFileListener listener;

        std::ifstream stream("assets/data_bind_test_cmdq.riv",
                             std::ios::binary);
        listener.m_handle = commandQueue->loadFile(
            std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
            &listener);

        commandQueue->requestViewModelInstanceNames(
            listener.m_handle,
            listener.m_viewModelName,
            listener.m_instanceRequestId);
        commandQueue->requestViewModelPropertyDefinitions(
            listener.m_handle,
            listener.m_viewModelName,
            listener.m_propertyRequestId);

        wait_for_server(commandQueue.get());

        commandQueue->processMessages();

        CHECK(listener.m_hasInstanceCallback);
        CHECK(listener.m_hasPropertyCallback);
    }

    {
        TestViewModelFileListener listener;

        listener.m_handle =
            commandQueue->loadFile(std::vector<uint8_t>(1024 * 1024, {}));

        commandQueue->requestViewModelInstanceNames(
            listener.m_handle,
            listener.m_viewModelName,
            listener.m_instanceRequestId);
        commandQueue->requestViewModelPropertyDefinitions(
            listener.m_handle,
            listener.m_viewModelName,
            listener.m_propertyRequestId);

        wait_for_server(commandQueue.get());

        commandQueue->processMessages();

        CHECK(!listener.m_hasInstanceCallback);
        CHECK(!listener.m_hasPropertyCallback);
    }

    commandQueue->disconnect();
    serverThread.join();
}

class TestViewModelInstanceListener
    : public CommandQueue::ViewModelInstanceListener
{
public:
    virtual void onViewModelDeleted(const ViewModelInstanceHandle handle,
                                    uint64_t requestId) override
    {
        CHECK(requestId == m_deleteRequestId);
        CHECK(m_handle == handle);
        m_hasDeleteCallback = true;
    }

    bool m_hasDeleteCallback = false;

    ViewModelInstanceHandle m_handle = RIVE_NULL_HANDLE;
    uint64_t m_deleteRequestId = std::rand();
};

TEST_CASE("View Model Instance Listener", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    TestViewModelInstanceListener bListener;
    TestViewModelInstanceListener dListener;
    TestViewModelInstanceListener nListener;

    TestViewModelInstanceListener badListener;

    TestViewModelInstanceListener aListener;
    TestViewModelInstanceListener adListener;
    TestViewModelInstanceListener anListener;
    TestViewModelInstanceListener badAListener;

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    bListener.m_handle =
        commandQueue->instantiateBlankViewModelInstance(fileHandle,
                                                        "Test All",
                                                        &bListener);
    dListener.m_handle =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle,
                                                          "Test All",
                                                          &dListener);
    nListener.m_handle =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Test All",
                                                        "Test Alternate",
                                                        &nListener);

    badListener.m_handle =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Blah",
                                                        "Blah",
                                                        &badListener);

    auto artboard =
        commandQueue->instantiateArtboardNamed(fileHandle, "Test Artboard");

    auto badArtboard =
        commandQueue->instantiateArtboardNamed(fileHandle, "Blah");

    aListener.m_handle =
        commandQueue->instantiateBlankViewModelInstance(fileHandle,
                                                        artboard,
                                                        &aListener);
    adListener.m_handle =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle,
                                                          artboard,
                                                          &adListener);
    anListener.m_handle =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        artboard,
                                                        "Test Alternate",
                                                        &anListener);

    badAListener.m_handle =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        badArtboard,
                                                        "Test Alternate",
                                                        &badAListener);

    commandQueue->deleteViewModelInstance(bListener.m_handle,
                                          bListener.m_deleteRequestId);
    commandQueue->deleteViewModelInstance(dListener.m_handle,
                                          dListener.m_deleteRequestId);
    commandQueue->deleteViewModelInstance(nListener.m_handle,
                                          nListener.m_deleteRequestId);
    commandQueue->deleteViewModelInstance(badListener.m_handle,
                                          badListener.m_deleteRequestId);

    commandQueue->deleteViewModelInstance(aListener.m_handle,
                                          aListener.m_deleteRequestId);
    commandQueue->deleteViewModelInstance(adListener.m_handle,
                                          adListener.m_deleteRequestId);
    commandQueue->deleteViewModelInstance(anListener.m_handle,
                                          anListener.m_deleteRequestId);
    commandQueue->deleteViewModelInstance(badAListener.m_handle,
                                          badAListener.m_deleteRequestId);

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(bListener.m_hasDeleteCallback);
    CHECK(dListener.m_hasDeleteCallback);
    CHECK(nListener.m_hasDeleteCallback);
    CHECK(badListener.m_hasDeleteCallback);

    CHECK(aListener.m_hasDeleteCallback);
    CHECK(adListener.m_hasDeleteCallback);
    CHECK(anListener.m_hasDeleteCallback);
    CHECK(badAListener.m_hasDeleteCallback);

    commandQueue->disconnect();
    serverThread.join();
}

TEST_CASE("External Resources", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    rcp<RenderImage> externalImage = nullptr;
    rcp<AudioSource> externalAudio = nullptr;
    rcp<Font> externalFont = nullptr;

    commandQueue->runOnce([&externalImage, &externalAudio, &externalFont](
                              CommandServer* server) {
        std::ifstream imageStream("assets/batdude.png", std::ios::binary);
        std::vector<uint8_t> imageStreamData(
            std::istreambuf_iterator<char>(imageStream),
            {});

        std::ifstream audioStream("assets/audio/what.wav", std::ios::binary);
        std::vector<uint8_t> audioStreamData(
            std::istreambuf_iterator<char>(audioStream),
            {});

        std::ifstream fontStream("assets/fonts/OpenSans-Italic.ttf",
                                 std::ios::binary);
        std::vector<uint8_t> fontStreamData(
            std::istreambuf_iterator<char>(fontStream),
            {});

        auto factory = server->factory();

        externalImage = factory->decodeImage(imageStreamData);
        externalAudio = factory->decodeAudio(audioStreamData);
        externalFont = factory->decodeFont(fontStreamData);
    });

    wait_for_server(commandQueue.get());

    CHECK(externalImage);
    CHECK(externalAudio);
    CHECK(externalFont);

    RenderImageHandle externalImageHandle =
        commandQueue->addExternalImage(externalImage);
    AudioSourceHandle externalAudioHandle =
        commandQueue->addExternalAudio(externalAudio);
    FontHandle externalFontHandle = commandQueue->addExternalFont(externalFont);

    commandQueue->runOnce([externalImageHandle,
                           externalImage,
                           externalAudioHandle,
                           externalAudio,
                           externalFontHandle,
                           externalFont](CommandServer* server) {
        auto image = server->getImage(externalImageHandle);
        CHECK(image == externalImage.get());
        auto audio = server->getAudioSource(externalAudioHandle);
        CHECK(audio == externalAudio.get());
        auto font = server->getFont(externalFontHandle);
        CHECK(font == externalFont.get());
    });

    commandQueue->deleteImage(externalImageHandle);
    commandQueue->deleteAudio(externalAudioHandle);
    commandQueue->deleteFont(externalFontHandle);

    commandQueue->runOnce([externalImageHandle,
                           externalAudioHandle,
                           externalFontHandle](CommandServer* server) {
        auto image = server->getImage(externalImageHandle);
        CHECK(image == nullptr);
        auto audio = server->getAudioSource(externalAudioHandle);
        CHECK(audio == nullptr);
        auto font = server->getFont(externalFontHandle);
        CHECK(font == nullptr);
    });

    commandQueue->disconnect();
    serverThread.join();
}

TEST_CASE("RenderImage", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/batdude.png", std::ios::binary);
    RenderImageHandle imageHandle = commandQueue->decodeImage(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    RenderImageHandle badImageHandle =
        commandQueue->decodeImage(std::vector<uint8_t>(1024, {}));

    commandQueue->runOnce([imageHandle, badImageHandle](CommandServer* server) {
        auto image = server->getImage(imageHandle);
        CHECK(image != nullptr);
        auto badImage = server->getImage(badImageHandle);
        CHECK(badImage == nullptr);
    });

    commandQueue->deleteImage(imageHandle);
    commandQueue->deleteImage(badImageHandle);

    commandQueue->runOnce([imageHandle, badImageHandle](CommandServer* server) {
        auto image = server->getImage(imageHandle);
        CHECK(image == nullptr);
        auto badImage = server->getImage(badImageHandle);
        CHECK(badImage == nullptr);
    });

    commandQueue->disconnect();
    serverThread.join();
}
#ifdef WITH_RIVE_AUDIO
class AudioSourceDeletedListener : public CommandQueue::AudioSourceListener
{
public:
    virtual void onAudioSourceDeleted(const AudioSourceHandle handle,
                                      uint64_t requestId)
    {
        CHECK(m_handle == handle);
        CHECK(m_requestId == requestId);
        CHECK(!m_hasCallback);
        m_hasCallback = true;
    }

    AudioSourceHandle m_handle;
    bool m_hasCallback = false;
    uint64_t m_requestId = 0x10;
};

TEST_CASE("AudioSource", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/audio/what.wav", std::ios::binary);

    AudioSourceDeletedListener listener;

    AudioSourceHandle audioHandle = commandQueue->decodeAudio(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        &listener,
        10);
    listener.m_handle = audioHandle;
    AudioSourceHandle badAudioHandle =
        commandQueue->decodeAudio(std::vector<uint8_t>(1024, {}));

    commandQueue->runOnce([audioHandle, badAudioHandle](CommandServer* server) {
        auto audio = server->getAudioSource(audioHandle);
        CHECK(audio != nullptr);
        auto badAudio = server->getAudioSource(badAudioHandle);
        CHECK(badAudio == nullptr);
    });

    commandQueue->deleteAudio(audioHandle, listener.m_requestId);
    commandQueue->deleteAudio(badAudioHandle);

    commandQueue->runOnce([audioHandle, badAudioHandle](CommandServer* server) {
        auto audio = server->getAudioSource(audioHandle);
        CHECK(audio == nullptr);
        auto badAudio = server->getAudioSource(badAudioHandle);
        CHECK(badAudio == nullptr);
    });

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(listener.m_hasCallback);

    commandQueue->disconnect();
    serverThread.join();
}
#endif

#if WITH_RIVE_TEXT

class FontDeletedListener : public CommandQueue::FontListener
{
public:
    virtual void onFontDeleted(const FontHandle handle, uint64_t requestId)
    {
        CHECK(m_handle == handle);
        CHECK(m_requestId == requestId);
        CHECK(!m_hasCallback);
        m_hasCallback = true;
    }

    FontHandle m_handle;
    bool m_hasCallback = false;
    uint64_t m_requestId = 0x10;
};

TEST_CASE("Font", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    FontDeletedListener listener;

    std::ifstream stream("assets/fonts/OpenSans-Italic.ttf", std::ios::binary);
    FontHandle fontHandle = commandQueue->decodeFont(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        &listener,
        10);
    listener.m_handle = fontHandle;
    FontHandle badFontHandle =
        commandQueue->decodeFont(std::vector<uint8_t>(1024, {}));

    commandQueue->runOnce([fontHandle, badFontHandle](CommandServer* server) {
        auto font = server->getFont(fontHandle);
        CHECK(font != nullptr);
        auto badFont = server->getFont(badFontHandle);
        CHECK(badFont == nullptr);
    });

    commandQueue->deleteFont(fontHandle, listener.m_requestId);
    commandQueue->deleteFont(badFontHandle);

    commandQueue->runOnce([fontHandle, badFontHandle](CommandServer* server) {
        auto font = server->getFont(fontHandle);
        CHECK(font == nullptr);
        auto badFont = server->getFont(badFontHandle);
        CHECK(badFont == nullptr);
    });

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(listener.m_hasCallback);

    commandQueue->disconnect();
    serverThread.join();
}
#endif

namespace rive
{
bool operator==(const CommandQueue::ViewModelInstanceData& l,
                const CommandQueue::ViewModelInstanceData& r)

{
    bool ret = l.metaData == r.metaData;
    switch (l.metaData.type)
    {
        case DataType::boolean:
            ret &= l.boolValue == r.boolValue;
            break;
        case DataType::number:
            ret &= l.numberValue == r.numberValue;
            break;
        case DataType::color:
            ret &= l.colorValue == r.colorValue;
            break;
        case DataType::string:
        case DataType::enumType:
            ret &= l.stringValue == r.stringValue;
            break;
        default:
            break;
    }

    return ret;
}
} // namespace rive

class ViewModelPropertyListener : public CommandQueue::ViewModelInstanceListener
{
public:
    virtual void onViewModelInstanceError(const ViewModelInstanceHandle handle,
                                          uint64_t requestId,
                                          std::string error) override
    {
        CHECK(handle == m_handle);
        CHECK(error.size());
        ++m_receivedErrors;
    }
    virtual void onViewModelDeleted(const ViewModelInstanceHandle handle,
                                    uint64_t requestId) override
    {
        CHECK(handle == m_handle);
        CHECK(!n_wasDeleted);
        n_wasDeleted = true;
    }

    virtual void onViewModelDataReceived(
        const ViewModelInstanceHandle handle,
        uint64_t requestId,
        CommandQueue::ViewModelInstanceData data) override
    {
        // the callback order should be garunteed.
        // so getting these in the order they are requested should work
        CHECK(m_expectedData.size());
        CHECK(m_expectedRequestIds.size());
        auto expectedData = m_expectedData.front();
        m_expectedData.pop_front();
        auto expectedRequestId = m_expectedRequestIds.front();
        m_expectedRequestIds.pop_front();

        CHECK(handle == m_handle);
        CHECK(data == expectedData);
        CHECK(requestId == expectedRequestId);
    }

    void pushExpectation(CommandQueue* queue, std::string name, float value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceNumber(m_handle, name, value, m_requestIdx);
        queue->runOnce([handle = m_handle, name, value](CommandServer* server) {
            auto instance = server->getViewModelInstance(handle);
            CHECK(instance != nullptr);
            auto property = instance->propertyNumber(name);
            CHECK(property != nullptr);
            CHECK(property->value() == value);
        });
        m_expectedData.push_back(
            {.metaData = {DataType::number, name}, .numberValue = value});
        m_expectedRequestIds.push_back(m_requestIdx);
        queue->requestViewModelInstanceNumber(m_handle, name, m_requestIdx);
    }

    void pushExpectation(CommandQueue* queue, std::string name, ColorInt value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceColor(m_handle, name, value, m_requestIdx);
        queue->runOnce([handle = m_handle, name, value](CommandServer* server) {
            auto instance = server->getViewModelInstance(handle);
            CHECK(instance != nullptr);
            auto property = instance->propertyColor(name);
            CHECK(property != nullptr);
            CHECK(property->value() == value);
        });
        m_expectedData.push_back(
            {.metaData = {DataType::color, name}, .colorValue = value});
        m_expectedRequestIds.push_back(m_requestIdx);
        queue->requestViewModelInstanceColor(m_handle, name, m_requestIdx);
    }

    void pushExpectation(CommandQueue* queue, std::string name, bool value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceBool(m_handle, name, value, m_requestIdx);
        queue->runOnce([handle = m_handle, name, value](CommandServer* server) {
            auto instance = server->getViewModelInstance(handle);
            CHECK(instance != nullptr);
            auto property = instance->propertyBoolean(name);
            CHECK(property != nullptr);
            CHECK(property->value() == value);
        });
        m_expectedData.push_back(
            {.metaData = {DataType::boolean, name}, .boolValue = value});
        m_expectedRequestIds.push_back(m_requestIdx);
        queue->requestViewModelInstanceBool(m_handle, name, m_requestIdx);
    }

    void pushExpectation(CommandQueue* queue,
                         std::string name,
                         ViewModelInstanceHandle value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceNestedViewModel(m_handle,
                                                   name,
                                                   value,
                                                   m_requestIdx);
        queue->runOnce([handle = m_handle, name, value](CommandServer* server) {
            auto instance = server->getViewModelInstance(handle);
            auto nested = server->getViewModelInstance(value);
            CHECK(instance != nullptr);
            CHECK(nested != nullptr);
            auto property = instance->propertyViewModel(name);
            CHECK(property != nullptr);
            CHECK(property == nested);
        });

        // There is no requesting for nested view models
    }

    void pushStringExpectation(CommandQueue* queue,
                               std::string name,
                               std::string value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceString(m_handle, name, value, m_requestIdx);
        queue->runOnce([handle = m_handle, name, value](CommandServer* server) {
            auto instance = server->getViewModelInstance(handle);
            CHECK(instance != nullptr);
            auto property = instance->propertyString(name);
            CHECK(property != nullptr);
            CHECK(property->value() == value);
        });
        m_expectedData.push_back(
            {.metaData = {DataType::string, name}, .stringValue = value});
        m_expectedRequestIds.push_back(m_requestIdx);
        queue->requestViewModelInstanceString(m_handle, name, m_requestIdx);
    }

    void pushEnumExpectation(CommandQueue* queue,
                             std::string name,
                             std::string value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceEnum(m_handle, name, value, m_requestIdx);
        queue->runOnce([handle = m_handle, name, value](CommandServer* server) {
            auto instance = server->getViewModelInstance(handle);
            CHECK(instance != nullptr);
            auto property = instance->propertyEnum(name);
            CHECK(property != nullptr);
            CHECK(property->value() == value);
        });
        m_expectedData.push_back(
            {.metaData = {DataType::enumType, name}, .stringValue = value});
        m_expectedRequestIds.push_back(m_requestIdx);
        queue->requestViewModelInstanceEnum(m_handle, name, m_requestIdx);
    }

    void pushBadExpectation(CommandQueue* queue, std::string name, float value)
    {
        queue->setViewModelInstanceNumber(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadExpectation(CommandQueue* queue,
                            std::string name,
                            ColorInt value)
    {
        queue->setViewModelInstanceColor(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadExpectation(CommandQueue* queue, std::string name, bool value)
    {
        queue->setViewModelInstanceBool(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadExpectation(CommandQueue* queue,
                            std::string name,
                            ViewModelInstanceHandle value)
    {
        queue->setViewModelInstanceNestedViewModel(m_handle,
                                                   name,
                                                   value,
                                                   m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadStringExpectation(CommandQueue* queue,
                                  std::string name,
                                  std::string value)
    {
        queue->setViewModelInstanceString(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadEnumExpectation(CommandQueue* queue,
                                std::string name,
                                std::string value)
    {
        queue->setViewModelInstanceEnum(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    // one data and id per callback
    std::deque<CommandQueue::ViewModelInstanceData> m_expectedData;
    std::deque<uint64_t> m_expectedRequestIds;
    // all of the handles should be the same
    ViewModelInstanceHandle m_handle;
    bool n_wasDeleted = false;

    uint64_t m_requestIdx = 1;

    uint64_t m_expectedErrors = 0;
    uint64_t m_receivedErrors = 0;
};

TEST_CASE("View Model Property Set/Get", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    ViewModelPropertyListener tester;

    auto artboardHandle = commandQueue->instantiateDefaultArtboard(fileHandle);
    tester.m_handle =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle,
                                                          artboardHandle,
                                                          &tester);
    auto blankHandle =
        commandQueue->instantiateBlankViewModelInstance(fileHandle,
                                                        "Nested VM");
    auto alternateHandle =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Nested VM",
                                                        "Alternate Nested");

    tester.pushExpectation(commandQueue.get(), "Test Bool", true);
    tester.pushExpectation(commandQueue.get(), "Test Num", 10.0f);
    tester.pushExpectation(commandQueue.get(),
                           "Test Nested/Nested Number",
                           10.0f);
    tester.pushExpectation(commandQueue.get(), "Test Nested", blankHandle);
    tester.pushExpectation(commandQueue.get(),
                           "Test Nested/Nested Number",
                           10.0f);
    commandQueue->runOnce([rootHandle = tester.m_handle,
                           blankHandle](CommandServer* server) {
        auto root = server->getViewModelInstance(rootHandle);
        auto nested = server->getViewModelInstance(blankHandle);
        CHECK(root != nullptr);
        CHECK(nested != nullptr);
        auto rootProperty = root->propertyNumber("Test Nested/Nested Number");
        CHECK(rootProperty != nullptr);
        CHECK(rootProperty->value() == 10.0f);
        auto nestedProperty = nested->propertyNumber("Nested Number");
        CHECK(nestedProperty != nullptr);
        CHECK(nestedProperty->value() == 10.0f);
    });
    tester.pushExpectation(commandQueue.get(),
                           "Test Color",
                           rive::colorARGB(255, 255, 0, 0));
    tester.pushEnumExpectation(commandQueue.get(), "Test Enum", "Value 2");
    tester.pushStringExpectation(commandQueue.get(),
                                 "Test String",
                                 "Some String");

    // Images don't have a "get" equivalent so we test it with a run once
    // directly.
    std::ifstream imageStream("assets/batdude.png", std::ios::binary);
    auto imageHandle = commandQueue->decodeImage(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(imageStream), {}));
    commandQueue->setViewModelInstanceImage(tester.m_handle,
                                            "Test Image",
                                            imageHandle);

    commandQueue->runOnce(
        [imageHandle, handle = tester.m_handle](CommandServer* server) {
            auto image = server->getImage(imageHandle);
            CHECK(image != nullptr);
            auto viewModel = server->getViewModelInstance(handle);
            CHECK(viewModel != nullptr);
            auto imageProperty = viewModel->propertyImage("Test Image");
            CHECK(imageProperty != nullptr);
            CHECK(imageProperty->testing_value() == image);
        });

    // Same for artboards.
    commandQueue->setViewModelInstanceArtboard(tester.m_handle,
                                               "Test Artboard",
                                               artboardHandle);

    commandQueue->runOnce([artboardHandle,
                           handle = tester.m_handle](CommandServer* server) {
        auto artboard = server->getArtboardInstance(artboardHandle);
        CHECK(artboard != nullptr);
        auto viewModel = server->getViewModelInstance(handle);
        CHECK(viewModel != nullptr);
        auto artboardProperty = viewModel->propertyArtboard("Test Artboard");
        CHECK(artboardProperty != nullptr);
        CHECK(artboardProperty->testing_value() == artboard);
    });

    auto badImageHandle =
        commandQueue->decodeImage(std::vector<uint8_t>(1024 * 1024, {}));
    commandQueue->setViewModelInstanceImage(tester.m_handle,
                                            "Test Image",
                                            badImageHandle);
    ++tester.m_expectedErrors;

    auto badArtboardHandle =
        commandQueue->instantiateArtboardNamed(fileHandle, "Blah");
    commandQueue->setViewModelInstanceArtboard(tester.m_handle,
                                               "Test Artboard",
                                               badArtboardHandle);
    ++tester.m_expectedErrors;

    commandQueue->runOnce([imageHandle,
                           badImageHandle,
                           handle = tester.m_handle](CommandServer* server) {
        auto image = server->getImage(imageHandle);
        auto badImage = server->getImage(badImageHandle);
        CHECK(image != nullptr);
        CHECK(badImage == nullptr);
        auto viewModel = server->getViewModelInstance(handle);
        CHECK(viewModel != nullptr);
        auto imageProperty = viewModel->propertyImage("Test Image");
        CHECK(imageProperty != nullptr);
        CHECK(imageProperty->testing_value() == image);
    });

    commandQueue->setViewModelInstanceImage(tester.m_handle,
                                            "Blah",
                                            imageHandle);
    // Account for bad image request.
    ++tester.m_expectedErrors;

    commandQueue->setViewModelInstanceArtboard(tester.m_handle,
                                               "Blah",
                                               artboardHandle);
    // Account for bad artboard request.
    ++tester.m_expectedErrors;

    commandQueue->runOnce(
        [imageHandle, handle = tester.m_handle](CommandServer* server) {
            auto image = server->getImage(imageHandle);
            CHECK(image != nullptr);
            auto viewModel = server->getViewModelInstance(handle);
            CHECK(viewModel != nullptr);
            auto imageProperty = viewModel->propertyImage("Test Image");
            CHECK(imageProperty != nullptr);
            CHECK(imageProperty->testing_value() == image);
        });

    // We should set / get in order as it goes through the list.
    for (int i = 0; i < 10; ++i)
    {
        tester.pushExpectation(commandQueue.get(),
                               "Test Bool",
                               static_cast<bool>(i % 2));
        tester.pushExpectation(commandQueue.get(),
                               "Test Num",
                               static_cast<float>(i));
        tester.pushExpectation(commandQueue.get(),
                               "Test Nested",
                               i % 2 ? blankHandle : alternateHandle);
        tester.pushExpectation(commandQueue.get(),
                               "Test Color",
                               rive::colorARGB(i, i, i, i));
        tester.pushEnumExpectation(commandQueue.get(),
                                   "Test Enum",
                                   i % 2 ? "Value 2" : "Value 1");
        tester.pushStringExpectation(commandQueue.get(),
                                     "Test String",
                                     std::to_string(i));
    }

    // Bad values

    commandQueue->deleteViewModelInstance(blankHandle);
    commandQueue->deleteViewModelInstance(alternateHandle);

    // Good property path bad value
    tester.pushBadEnumExpectation(commandQueue.get(), "Test Enum", "Blah");
    tester.pushBadExpectation(commandQueue.get(), "Test Nested", blankHandle);

    // Bad everything
    tester.pushBadExpectation(commandQueue.get(), "Blah", true);
    tester.pushBadExpectation(commandQueue.get(), "Blah", 10.0f);
    tester.pushBadExpectation(commandQueue.get(), "Blah", alternateHandle);
    tester.pushBadExpectation(commandQueue.get(),
                              "Blah",
                              rive::colorARGB(255, 255, 0, 0));
    tester.pushBadEnumExpectation(commandQueue.get(), "Blah", "Value 2");
    tester.pushBadStringExpectation(commandQueue.get(), "Blah", "Some String");

    // Delete the instance, the callback should continue fine and a delete
    // should be received
    commandQueue->deleteViewModelInstance(tester.m_handle);

    // Call sets on deleted handle
    tester.pushBadExpectation(commandQueue.get(), "Test Bool", true);
    tester.pushBadExpectation(commandQueue.get(), "Test Num", 10.0f);
    tester.pushBadExpectation(commandQueue.get(),
                              "Test Color",
                              rive::colorARGB(255, 255, 0, 0));
    tester.pushBadEnumExpectation(commandQueue.get(), "Test Enum", "Value 2");
    tester.pushBadStringExpectation(commandQueue.get(),
                                    "Test String",
                                    "Some String");

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    // We expect there should be no expected values left
    CHECK(tester.m_expectedData.size() == 0);
    CHECK(tester.m_expectedRequestIds.size() == 0);

    CHECK(tester.m_expectedErrors == tester.m_receivedErrors);

    // We should have received the deleted event
    CHECK(tester.n_wasDeleted);

    commandQueue->disconnect();
    serverThread.join();
}

class ViewModelPropertySubscriptionListener
    : public CommandQueue::ViewModelInstanceListener
{
public:
    virtual void onViewModelInstanceError(const ViewModelInstanceHandle handle,
                                          uint64_t requestId,
                                          std::string error) override
    {
        CHECK(handle == m_handle);
        CHECK(error.size());
        ++m_receivedErrors;
    }

    virtual void onViewModelDeleted(const ViewModelInstanceHandle handle,
                                    uint64_t requestId) override
    {
        CHECK(handle == m_handle);
        CHECK(!n_wasDeleted);
        n_wasDeleted = true;
    }

    virtual void onViewModelDataReceived(
        const ViewModelInstanceHandle handle,
        uint64_t requestId,
        CommandQueue::ViewModelInstanceData data) override
    {
        // We only get one sub callback per value, so instead of a dequeue we
        // use a map of names to values that we expect, it should always be the
        // last value set.
        CHECK(m_expectedData.size());
        auto itr = m_expectedData.find(data.metaData.name);
        CHECK(itr != m_expectedData.end());
        auto& expectedData = itr->second;

        CHECK(handle == m_handle);
        CHECK(data == expectedData);

        ++m_receivedCallbacks;
    }

    void pushTriggerExpectation(CommandQueue* queue, std::string name)
    {
        ++m_requestIdx;
        m_expectedData[name] = {.metaData = {DataType::trigger, name}};
        queue->fireViewModelTrigger(m_handle, name, m_requestIdx);
    }

    void pushExpectation(CommandQueue* queue, std::string name, float value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceNumber(m_handle, name, value, m_requestIdx);
        m_expectedData[name] = {.metaData = {DataType::number, name},
                                .numberValue = value};
    }

    void pushExpectation(CommandQueue* queue, std::string name, ColorInt value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceColor(m_handle, name, value, m_requestIdx);
        m_expectedData[name] = {.metaData = {DataType::color, name},
                                .colorValue = value};
    }

    void pushExpectation(CommandQueue* queue, std::string name, bool value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceBool(m_handle, name, value, m_requestIdx);
        m_expectedData[name] = {.metaData = {DataType::boolean, name},
                                .boolValue = value};
    }

    void pushExpectation(CommandQueue* queue,
                         std::string name,
                         ViewModelInstanceHandle value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceNestedViewModel(m_handle,
                                                   name,
                                                   value,
                                                   m_requestIdx);
        // there is no subscription for view models, you must subscribe to the
        // nested property instead
    }

    void pushListExpectation(CommandQueue* queue,
                             std::string name,
                             ViewModelInstanceHandle value)
    {
        ++m_requestIdx;
        queue->appendViewModelInstanceListViewModel(m_handle,
                                                    name,
                                                    value,
                                                    m_requestIdx);
        // there is no value for view models list subscription callbacks
        m_expectedData[name] = {.metaData = {DataType::viewModel, name}};
    }

    void pushStringExpectation(CommandQueue* queue,
                               std::string name,
                               std::string value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceString(m_handle, name, value, m_requestIdx);
        m_expectedData[name] = {.metaData = {DataType::string, name},
                                .stringValue = value};
    }

    void pushEnumExpectation(CommandQueue* queue,
                             std::string name,
                             std::string value)
    {
        ++m_requestIdx;
        queue->setViewModelInstanceEnum(m_handle, name, value, m_requestIdx);
        m_expectedData[name] = {.metaData = {DataType::enumType, name},
                                .stringValue = value};
    }

    void pushExpectation(CommandQueue* queue,
                         std::string name,
                         RenderImageHandle value)
    {
        queue->setViewModelInstanceImage(m_handle, "Test Image", value);
        // no value for image subscriptions
        m_expectedData[name] = {.metaData = {DataType::assetImage, name}};
    }

    void pushBadExpectation(CommandQueue* queue, std::string name, float value)
    {
        queue->setViewModelInstanceNumber(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadExpectation(CommandQueue* queue,
                            std::string name,
                            ColorInt value)
    {
        queue->setViewModelInstanceColor(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadExpectation(CommandQueue* queue, std::string name, bool value)
    {
        queue->setViewModelInstanceBool(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadExpectation(CommandQueue* queue,
                            std::string name,
                            RenderImageHandle value)
    {
        queue->setViewModelInstanceImage(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadExpectation(CommandQueue* queue,
                            std::string name,
                            ViewModelInstanceHandle value)
    {
        queue->setViewModelInstanceNestedViewModel(m_handle,
                                                   name,
                                                   value,
                                                   m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadStringExpectation(CommandQueue* queue,
                                  std::string name,
                                  std::string value)
    {
        queue->setViewModelInstanceString(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadEnumExpectation(CommandQueue* queue,
                                std::string name,
                                std::string value)
    {
        queue->setViewModelInstanceEnum(m_handle, name, value, m_requestIdx);
        ++m_expectedErrors;
    }

    void pushBadTriggerExpectation(CommandQueue* queue, std::string name)
    {
        queue->fireViewModelTrigger(m_handle, name);
        ++m_expectedErrors;
    }

    // One data and id per callback.
    std::unordered_map<std::string, CommandQueue::ViewModelInstanceData>
        m_expectedData;
    // All of the handles should be the same.
    ViewModelInstanceHandle m_handle;
    bool n_wasDeleted = false;

    uint64_t m_requestIdx = 1;
    int m_receivedCallbacks = 0;

    size_t m_expectedErrors = 0;
    size_t m_receivedErrors = 0;
};

TEST_CASE("View Model Property Subscriptions", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();
    // The subscriptions happens once at the end of processCommands, so it's not
    // order dependent, that makes it more difficult to test. To get around
    // this, we just make the server on the same thread. Different tests will be
    // used for testing async.
    CommandServer server(commandQueue, nullContext.get());

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    ViewModelPropertySubscriptionListener tester;

    auto artboardHandle = commandQueue->instantiateDefaultArtboard(fileHandle);
    tester.m_handle =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle,
                                                          artboardHandle,
                                                          &tester);

    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test Nested/Nested Number",
                                               DataType::number);
    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test Bool",
                                               DataType::boolean);
    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test Num",
                                               DataType::number);
    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test Color",
                                               DataType::color);
    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test Enum",
                                               DataType::enumType);
    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test String",
                                               DataType::string);
    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test Trigger",
                                               DataType::trigger);
    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test List",
                                               DataType::list);
    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test Image",
                                               DataType::assetImage);

    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Bad property",
                                               DataType::assetImage);
    ++tester.m_expectedErrors;

    // bad type
    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test Image",
                                               DataType::integer);
    ++tester.m_expectedErrors;

    commandQueue->runOnce([](CommandServer* server) {
        auto subs = server->testing_getSubsciptions();
        CHECK(subs.size() == 9);
    });

    auto blankHandle =
        commandQueue->instantiateBlankViewModelInstance(fileHandle,
                                                        "Nested VM");
    auto alternateHandle =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Nested VM",
                                                        "Alternate Nested");

    tester.pushExpectation(commandQueue.get(), "Test Bool", true);
    tester.pushExpectation(commandQueue.get(), "Test Num", 10.0f);
    tester.pushExpectation(commandQueue.get(),
                           "Test Nested/Nested Number",
                           10.0f);
    tester.pushExpectation(commandQueue.get(), "Test Nested", blankHandle);
    tester.pushExpectation(commandQueue.get(),
                           "Test Nested/Nested Number",
                           10.0f);

    tester.pushExpectation(commandQueue.get(),
                           "Test Color",
                           rive::colorARGB(255, 255, 0, 0));
    tester.pushEnumExpectation(commandQueue.get(), "Test Enum", "Value 2");
    tester.pushStringExpectation(commandQueue.get(),
                                 "Test String",
                                 "Some String");

    tester.pushTriggerExpectation(commandQueue.get(), "Test Trigger");

    std::ifstream imageStream("assets/batdude.png", std::ios::binary);
    auto imageHandle = commandQueue->decodeImage(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(imageStream), {}));
    tester.pushExpectation(commandQueue.get(), "Test Image", imageHandle);

    for (int i = 0; i < 10; ++i)
    {
        tester.pushExpectation(commandQueue.get(),
                               "Test Bool",
                               static_cast<bool>(i % 2));
        tester.pushExpectation(commandQueue.get(),
                               "Test Num",
                               static_cast<float>(i));
        tester.pushExpectation(commandQueue.get(),
                               "Test Nested",
                               i % 2 ? blankHandle : alternateHandle);
        tester.pushExpectation(commandQueue.get(),
                               "Test Color",
                               rive::colorARGB(i, i, i, i));
        tester.pushEnumExpectation(commandQueue.get(),
                                   "Test Enum",
                                   i % 2 ? "Value 2" : "Value 1");
        tester.pushStringExpectation(commandQueue.get(),
                                     "Test String",
                                     std::to_string(i));
    }

    // Bad values

    auto badImageHandle =
        commandQueue->decodeImage(std::vector<uint8_t>(1024 * 1024, {}));

    tester.pushBadExpectation(commandQueue.get(), "Test Image", badImageHandle);

    commandQueue->deleteViewModelInstance(blankHandle);
    commandQueue->deleteViewModelInstance(alternateHandle);

    // Good property path bad value
    tester.pushBadEnumExpectation(commandQueue.get(), "Test Enum", "Blah");
    tester.pushBadExpectation(commandQueue.get(), "Test Nested", blankHandle);

    // Bad everything
    tester.pushBadExpectation(commandQueue.get(), "Blah", true);
    tester.pushBadExpectation(commandQueue.get(), "Blah", 10.0f);
    tester.pushBadExpectation(commandQueue.get(), "Blah", alternateHandle);
    tester.pushBadExpectation(commandQueue.get(),
                              "Blah",
                              rive::colorARGB(255, 255, 0, 0));
    tester.pushBadEnumExpectation(commandQueue.get(), "Blah", "Value 2");
    tester.pushBadStringExpectation(commandQueue.get(), "Blah", "Some String");

    tester.pushBadTriggerExpectation(commandQueue.get(), "Blah");

    // Bad handle for trigger test.
    commandQueue->fireViewModelTrigger(0, "Blah");

    server.processCommands();
    commandQueue->processMessages();

    // Once the handle is delted subscriptions will stop, so we do this after
    // processMessages.
    commandQueue->deleteViewModelInstance(tester.m_handle);

    // Call sets on deleted handle
    tester.pushBadExpectation(commandQueue.get(), "Test Bool", true);
    tester.pushBadExpectation(commandQueue.get(), "Test Num", 10.0f);
    tester.pushBadExpectation(commandQueue.get(),
                              "Test Color",
                              rive::colorARGB(255, 255, 0, 0));
    tester.pushBadEnumExpectation(commandQueue.get(), "Test Enum", "Value 2");
    tester.pushBadStringExpectation(commandQueue.get(),
                                    "Test String",
                                    "Some String");
    server.processCommands();
    commandQueue->processMessages();

    // We should have received the deleted event.
    CHECK(tester.n_wasDeleted);
    // We should have received the same number of callbacks as entries in the
    // map.
    CHECK(tester.m_receivedCallbacks == tester.m_expectedData.size());

    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Test Nested/Nested Number",
                                                 DataType::number);
    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Test Bool",
                                                 DataType::boolean);
    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Test Num",
                                                 DataType::number);
    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Test Color",
                                                 DataType::color);
    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Test Enum",
                                                 DataType::enumType);
    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Test String",
                                                 DataType::string);
    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Test Trigger",
                                                 DataType::trigger);
    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Test List",
                                                 DataType::list);
    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Test Image",
                                                 DataType::assetImage);
    // Unsub something that doesn't exist.
    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Blah",
                                                 DataType::boolean);
    // Note we don't increment tester.m_expectedErrors because unsubing
    // something invalid is ok to do, we just ignore it.

    commandQueue->runOnce([](CommandServer* server) {
        auto subs = server->testing_getSubsciptions();
        CHECK(subs.empty());
    });

    server.processCommands();
    commandQueue->processMessages();

    CHECK(tester.m_receivedErrors == tester.m_expectedErrors);

    commandQueue->disconnect();
}

class AsyncSubListener : public CommandQueue::ViewModelInstanceListener
{
public:
    virtual void onViewModelDataReceived(
        const ViewModelInstanceHandle handle,
        uint64_t requestId,
        CommandQueue::ViewModelInstanceData data) override
    {
        CHECK(handle == m_handle);
        CHECK(!m_hasCallback);
        m_hasCallback = true;
        // we are just expecting the one sub so the value should be correct
        CHECK(data.numberValue == 10);
    }

    ViewModelInstanceHandle m_handle;
    bool m_hasCallback = false;
};

// The above tests are to check that all subscription types work and that values
// come through correctly This is just checking to make sure subscriptions work
// while the server is in a seperate thread but we don't care about the exact
// callback happening since that is tested above

TEST_CASE("View Model Property Async Subscriptions", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    AsyncSubListener tester;

    auto artboardHandle = commandQueue->instantiateDefaultArtboard(fileHandle);
    tester.m_handle =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle,
                                                          artboardHandle,
                                                          &tester);

    commandQueue->subscribeToViewModelProperty(tester.m_handle,
                                               "Test Num",
                                               DataType::number);

    commandQueue->setViewModelInstanceNumber(tester.m_handle, "Test Num", 10);

    commandQueue->runOnce([](CommandServer* server) {
        auto subs = server->testing_getSubsciptions();
        CHECK(subs.size() == 1);
    });

    commandQueue->testing_commandLoopBreak();

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(tester.m_hasCallback);

    commandQueue->unsubscribeToViewModelProperty(tester.m_handle,
                                                 "Test Num",
                                                 DataType::number);

    commandQueue->runOnce([](CommandServer* server) {
        auto subs = server->testing_getSubsciptions();
        CHECK(subs.empty());
    });

    commandQueue->disconnect();
    serverThread.join();
}

class ListViewModelPropertyListener
    : public CommandQueue::ViewModelInstanceListener
{
public:
    ListViewModelPropertyListener(rcp<CommandQueue> queue) :
        m_queue(std::move(queue))
    {}

    virtual void onViewModelListSizeReceived(
        const ViewModelInstanceHandle handle,
        uint64_t requestId,
        std::string path,
        size_t size) override
    {
        CHECK(handle == m_handle);
        CHECK(path == m_path);
        CHECK(size == m_expectedSize);
        CHECK(requestId == m_requestIdx);
        m_receivedSize = true;
    }

    virtual void onViewModelDeleted(const ViewModelInstanceHandle handle,
                                    uint64_t requestId) override
    {
        CHECK(handle == m_handle);
        CHECK(!n_wasDeleted);
        n_wasDeleted = true;
    }

    void pushRequestExpectation(std::string path, size_t expectedSize)
    {
        m_path = path;
        m_expectedSize = expectedSize;
        ++m_requestIdx;
        m_receivedSize = false;
        m_queue->requestViewModelInstanceListSize(m_handle,
                                                  m_path,
                                                  m_requestIdx);

        wait_for_server(m_queue.get());
        m_queue->processMessages();
        CHECK(m_receivedSize);
    }

    void pushExpectation(std::string name,
                         ViewModelInstanceHandle expectedValueAtIndex1,
                         ViewModelInstanceHandle expectedValueAtIndex2,
                         int index,
                         int index2)
    {
        m_queue->swapViewModelInstanceListValues(m_handle, name, index, index2);
        m_queue->runOnce([handle = m_handle,
                          name,
                          expectedValueAtIndex1,
                          expectedValueAtIndex2,
                          index,
                          index2](CommandServer* server) {
            auto instance = server->getViewModelInstance(handle);
            CHECK(instance != nullptr);
            auto value1Instance =
                server->getViewModelInstance(expectedValueAtIndex1);
            CHECK(value1Instance != nullptr);
            auto value2Instance =
                server->getViewModelInstance(expectedValueAtIndex2);
            CHECK(value2Instance != nullptr);
            auto property = instance->propertyList(name);
            CHECK(property != nullptr);
            CHECK(property->instanceAt(index) == value1Instance);
            CHECK(property->instanceAt(index2) == value2Instance);
        });
    }

    void pushExpectation(std::string name,
                         ViewModelInstanceHandle value,
                         int index)
    {
        m_queue->insertViewModelInstanceListViewModel(m_handle,
                                                      name,
                                                      value,
                                                      index);
        m_queue->runOnce(
            [handle = m_handle, name, value, index](CommandServer* server) {
                auto instance = server->getViewModelInstance(handle);
                CHECK(instance != nullptr);
                auto property = instance->propertyList(name);
                CHECK(property != nullptr);
                auto valueModel = server->getViewModelInstance(value);
                CHECK(property->instanceAt(index) == valueModel);
            });
    }

    void pushExpectation(std::string name, ViewModelInstanceHandle value)
    {
        m_queue->appendViewModelInstanceListViewModel(m_handle, name, value);
        m_queue->runOnce(
            [handle = m_handle, name, value](CommandServer* server) {
                auto instance = server->getViewModelInstance(handle);
                CHECK(instance != nullptr);
                auto property = instance->propertyList(name);
                CHECK(property != nullptr);
                auto valueModel = server->getViewModelInstance(value);
                CHECK(property->instanceAt(static_cast<int>(property->size()) -
                                           1) == valueModel);
            });
    }

    void pushBadExpectation(std::string name,
                            ViewModelInstanceHandle value,
                            int index)
    {
        m_queue->insertViewModelInstanceListViewModel(m_handle,
                                                      name,
                                                      value,
                                                      index);
    }

    void pushBadExpectation(std::string name, ViewModelInstanceHandle value)
    {
        m_queue->appendViewModelInstanceListViewModel(m_handle, name, value);
    }

    void pushBadExpectation(std::string name, int index, int index2)
    {
        m_queue->swapViewModelInstanceListValues(m_handle, name, index, index2);
    }

    void pushBadRequestExpectation(std::string name)
    {
        m_receivedSize = false;
        m_queue->requestViewModelInstanceListSize(m_handle, name);

        wait_for_server(m_queue.get());
        m_queue->processMessages();
        CHECK(!m_receivedSize);
    }

    rcp<CommandQueue> m_queue;
    ViewModelInstanceHandle m_handle;
    bool n_wasDeleted = false;
    // size callback test values

    std::string m_path;
    size_t m_expectedSize;
    uint64_t m_requestIdx = 1;
    bool m_receivedSize = false;
};

TEST_CASE("List View Model Property Set/Get", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    ListViewModelPropertyListener tester(commandQueue);
    auto artboardHandle = commandQueue->instantiateDefaultArtboard(fileHandle);
    tester.m_handle =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle,
                                                          artboardHandle,
                                                          &tester);
    auto blankHandle =
        commandQueue->instantiateBlankViewModelInstance(fileHandle,
                                                        "Nested VM");
    auto alternateHandle =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Nested VM",
                                                        "Alternate Nested");
    tester.pushExpectation("Test List", blankHandle);
    tester.pushExpectation("Test List", alternateHandle);
    tester.pushExpectation("Test List", alternateHandle, blankHandle, 0, 1);
    tester.pushRequestExpectation("Test List", 2);
    tester.pushExpectation("Test List", blankHandle, 0);
    tester.pushExpectation("Test List", alternateHandle, 0);
    tester.pushExpectation("Test List", blankHandle, alternateHandle, 0, 1);
    tester.pushRequestExpectation("Test List", 4);

    auto badBlankHandle =
        commandQueue->instantiateBlankViewModelInstance(fileHandle, "blah");
    auto badAlternateHandle =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Nested VM",
                                                        "blah");
    tester.pushBadExpectation("Test List", badBlankHandle);
    tester.pushBadExpectation("Test List", badAlternateHandle);
    tester.pushBadExpectation("Test List", badBlankHandle, 0);
    tester.pushBadExpectation("Test List", badAlternateHandle, 0);

    tester.pushBadExpectation("blah", blankHandle);
    tester.pushBadExpectation("blah", alternateHandle);
    tester.pushBadExpectation("blah", blankHandle, 0);
    tester.pushBadExpectation("blah", alternateHandle, 0);

    tester.pushBadExpectation("Test List", 10, 1);
    tester.pushBadExpectation("Test List", 0, 10);
    tester.pushBadExpectation("Blah", 0, 1);
    tester.pushBadExpectation("Blah", 10, 1);
    tester.pushBadExpectation("Blah", 0, 10);

    tester.pushRequestExpectation("Test List", 4);

    tester.pushBadRequestExpectation("Blah");

    commandQueue->disconnect();
    serverThread.join();
}

class TestFileErrorListener : public CommandQueue::FileListener
{
public:
    virtual void onFileError(const FileHandle handle,
                             uint64_t requestId,
                             std::string error) override
    {
        CHECK(handle == m_handle);
        CHECK(error.size());
        ++m_receivedErrors;
    }

    FileHandle m_handle;
    size_t m_receivedErrors = 0;
};

TEST_CASE("file Error Messages", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    TestFileErrorListener fileListener;

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    fileListener.m_handle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        &fileListener);

    commandQueue->instantiateArtboardNamed(fileListener.m_handle, "Blah");
    commandQueue->instantiateViewModelInstanceNamed(fileListener.m_handle,
                                                    "Test All",
                                                    "blah");
    commandQueue->instantiateViewModelInstanceNamed(fileListener.m_handle,
                                                    "blah",
                                                    "blah");
    commandQueue->instantiateViewModelInstanceNamed(fileListener.m_handle,
                                                    nullptr,
                                                    "blah");

    commandQueue->instantiateDefaultViewModelInstance(fileListener.m_handle,
                                                      "Blah");
    commandQueue->instantiateDefaultViewModelInstance(fileListener.m_handle,
                                                      nullptr);

    commandQueue->instantiateBlankViewModelInstance(fileListener.m_handle,
                                                    "Blah");
    commandQueue->instantiateBlankViewModelInstance(fileListener.m_handle,
                                                    nullptr);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(fileListener.m_receivedErrors == 8);

    TestFileErrorListener badFileListener;
    badFileListener.m_handle =
        commandQueue->loadFile(std::vector<uint8_t>(100 * 1024, 0),
                               &badFileListener);

    commandQueue->instantiateDefaultArtboard(badFileListener.m_handle);
    commandQueue->instantiateDefaultViewModelInstance(badFileListener.m_handle,
                                                      nullptr);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();
    CHECK(badFileListener.m_receivedErrors == 3);

    commandQueue->disconnect();
    serverThread.join();
}

class TestFileListener : public CommandQueue::FileListener
{
public:
    virtual void onArtboardsListed(
        const FileHandle handle,
        uint64_t requestId,
        std::vector<std::string> artboardNames) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        CHECK(artboardNames.size() == m_artboardNames.size());
        for (auto i = 0; i < artboardNames.size(); ++i)
        {
            CHECK(artboardNames[i] == m_artboardNames[i]);
        }

        m_hasCallback = true;
    }

    uint64_t m_requestId;
    FileHandle m_handle;
    std::vector<std::string> m_artboardNames;
    bool m_hasCallback = false;
};

TEST_CASE("listArtboard", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    TestFileListener fileListener;

    std::ifstream stream("assets/entry.riv", std::ios::binary);
    FileHandle goodFile = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        &fileListener);

    fileListener.m_artboardNames = {"New Artboard", "New Artboard"};
    fileListener.m_handle = goodFile;

    fileListener.m_requestId = 0x40;
    commandQueue->requestArtboardNames(goodFile, fileListener.m_requestId);

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(fileListener.m_hasCallback);

    FileHandle badFile =
        commandQueue->loadFile(std::vector<uint8_t>(100 * 1024, 0));
    fileListener.m_handle = goodFile;
    fileListener.m_hasCallback = false;

    commandQueue->requestArtboardNames(badFile);

    wait_for_server(commandQueue.get());

    CHECK(!fileListener.m_hasCallback);

    commandQueue->processMessages();

    commandQueue->disconnect();
    serverThread.join();
}

class TestFileEnumListener : public CommandQueue::FileListener
{
public:
    virtual void onViewModelEnumsListed(const FileHandle handle,
                                        uint64_t requestId,
                                        std::vector<ViewModelEnum> enums)
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        CHECK(enums.size() == m_enums.size());
        for (auto i = 0; i < enums.size(); ++i)
        {
            CHECK(enums[i].name == m_enums[i].name);
            for (auto k = 0; k < enums[i].enumerants.size(); ++k)
            {
                CHECK(enums[i].enumerants[k] == m_enums[i].enumerants[k]);
            }
        }

        m_hasCallback = true;
    }

    uint64_t m_requestId;
    FileHandle m_handle;
    std::array<ViewModelEnum, 1> m_enums = {
        ViewModelEnum{"Test Enum Values", {"Value 1", "Value 2"}}};
    bool m_hasCallback = false;
};

TEST_CASE("listEnums", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    TestFileEnumListener fileListener;

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle goodFile = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        &fileListener);

    fileListener.m_handle = goodFile;

    fileListener.m_requestId = 0x40;
    commandQueue->requestViewModelEnums(goodFile, fileListener.m_requestId);

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(fileListener.m_hasCallback);

    FileHandle badFile =
        commandQueue->loadFile(std::vector<uint8_t>(100 * 1024, 0));
    fileListener.m_handle = goodFile;
    fileListener.m_hasCallback = false;

    commandQueue->requestViewModelEnums(badFile);

    wait_for_server(commandQueue.get());

    CHECK(!fileListener.m_hasCallback);

    commandQueue->processMessages();

    commandQueue->disconnect();
    serverThread.join();
}

class TestRenderImageErrorListener : public CommandQueue::RenderImageListener
{
public:
    virtual void onRenderImageError(const RenderImageHandle handle,
                                    uint64_t requestId,
                                    std::string error) override
    {
        CHECK(handle == m_handle);
        CHECK(error.size());
        CHECK(!m_hasCallback);
        m_hasCallback = true;
    }

    RenderImageHandle m_handle;
    bool m_hasCallback = false;
};

class TestAudioSourceErrorListener : public CommandQueue::AudioSourceListener
{
public:
    virtual void onAudioSourceError(const AudioSourceHandle handle,
                                    uint64_t requestId,
                                    std::string error) override
    {
        CHECK(handle == m_handle);
        CHECK(error.size());
        CHECK(!m_hasCallback);
        m_hasCallback = true;
    }

    AudioSourceHandle m_handle;
    bool m_hasCallback = false;
};

class TestFontErrorListener : public CommandQueue::FontListener
{
public:
    virtual void onFontError(const FontHandle handle,
                             uint64_t requestId,
                             std::string error) override
    {
        CHECK(handle == m_handle);
        CHECK(error.size());
        CHECK(!m_hasCallback);
        m_hasCallback = true;
    }

    FontHandle m_handle;
    bool m_hasCallback = false;
};

TEST_CASE("render image / audio source / font error", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    TestRenderImageErrorListener imageListener;
    TestAudioSourceErrorListener audioListener;
    TestFontErrorListener fontListener;

    imageListener.m_handle =
        commandQueue->decodeImage(std::vector<uint8_t>(1024, 0),
                                  &imageListener);
    audioListener.m_handle =
        commandQueue->decodeAudio(std::vector<uint8_t>(1024, 0),
                                  &audioListener);
    fontListener.m_handle =
        commandQueue->decodeFont(std::vector<uint8_t>(1024, 0), &fontListener);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(imageListener.m_hasCallback);
    CHECK(audioListener.m_hasCallback);
    CHECK(fontListener.m_hasCallback);

    commandQueue->disconnect();
    serverThread.join();
}

class TestStateMachineErrorListener : public CommandQueue::StateMachineListener
{
public:
    virtual void onStateMachineError(const StateMachineHandle handle,
                                     uint64_t requestId,
                                     std::string error) override
    {
        CHECK(handle == m_handle);
        CHECK(error.size());
        ++m_receivedErrors;
    }

    StateMachineHandle m_handle;
    size_t m_receivedErrors = 0;
};

TEST_CASE("state machine error", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    TestStateMachineErrorListener listener;

    std::ifstream stream("assets/entry.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    auto artboardHandle = commandQueue->instantiateDefaultArtboard(fileHandle);

    listener.m_handle =
        commandQueue->instantiateDefaultStateMachine(artboardHandle, &listener);
    commandQueue->bindViewModelInstance(listener.m_handle, nullptr);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(listener.m_receivedErrors == 1);

    TestStateMachineErrorListener badListener;
    badListener.m_handle =
        commandQueue->instantiateDefaultStateMachine(nullptr, &badListener);

    commandQueue->advanceStateMachine(badListener.m_handle, 0);
    commandQueue->pointerDown(badListener.m_handle, {});
    commandQueue->pointerExit(badListener.m_handle, {});
    commandQueue->pointerUp(badListener.m_handle, {});
    commandQueue->pointerMove(badListener.m_handle, {});

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(badListener.m_receivedErrors == 5);

    commandQueue->disconnect();
    serverThread.join();
}

class TestArtboardErrorListener : public CommandQueue::ArtboardListener
{
public:
    virtual void onArtboardError(const ArtboardHandle handle,
                                 uint64_t requestId,
                                 std::string error) override
    {
        CHECK(handle == m_handle);
        CHECK(error.size());
        ++m_receivedErrors;
    }

    size_t m_receivedErrors = 0;
    ArtboardHandle m_handle;
};

TEST_CASE("artboard errors", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/entry.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    TestArtboardErrorListener artboardListener;

    artboardListener.m_handle =
        commandQueue->instantiateArtboardNamed(fileHandle,
                                               "New Artboard",
                                               &artboardListener);

    commandQueue->instantiateStateMachineNamed(artboardListener.m_handle,
                                               "Blah");

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(artboardListener.m_receivedErrors == 1);

    TestArtboardErrorListener badArtboardListener;
    badArtboardListener.m_handle =
        commandQueue->instantiateArtboardNamed(fileHandle,
                                               "Blah",
                                               &badArtboardListener);

    commandQueue->requestStateMachineNames(badArtboardListener.m_handle);
    commandQueue->instantiateDefaultStateMachine(badArtboardListener.m_handle);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(badArtboardListener.m_receivedErrors == 2);

    commandQueue->disconnect();
    serverThread.join();
}

class TestArtboardListener : public CommandQueue::ArtboardListener
{
public:
    virtual void onStateMachinesListed(
        const ArtboardHandle handle,
        uint64_t requestId,
        std::vector<std::string> stateMachineNames) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        CHECK(stateMachineNames.size() == m_stateMachineNames.size());
        for (auto i = 0; i < stateMachineNames.size(); ++i)
        {
            CHECK(stateMachineNames[i] == m_stateMachineNames[i]);
        }

        m_hasCallback = true;
    }

    uint64_t m_requestId;
    ArtboardHandle m_handle;
    std::vector<std::string> m_stateMachineNames;
    bool m_hasCallback = false;
};

TEST_CASE("listStateMachine", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/entry.riv", std::ios::binary);
    FileHandle goodFile = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    TestArtboardListener artboardListener;

    auto artboardHandle =
        commandQueue->instantiateArtboardNamed(goodFile,
                                               "New Artboard",
                                               &artboardListener);
    artboardListener.m_stateMachineNames = {"State Machine 1"};
    artboardListener.m_handle = artboardHandle;

    artboardListener.m_requestId = 0x40;
    commandQueue->requestStateMachineNames(artboardHandle,
                                           artboardListener.m_requestId);

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(artboardListener.m_hasCallback);

    FileHandle badFile =
        commandQueue->loadFile(std::vector<uint8_t>(100 * 1024, 0));

    auto badArtbaord = commandQueue->instantiateDefaultArtboard(badFile);

    artboardListener.m_handle = badArtbaord;
    artboardListener.m_hasCallback = false;

    artboardListener.m_requestId = 0x40;
    commandQueue->requestStateMachineNames(badArtbaord,
                                           artboardListener.m_requestId);

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(!artboardListener.m_hasCallback);

    commandQueue->disconnect();
    serverThread.join();
}

class TestArtboardDefaultViewModelListener
    : public CommandQueue::ArtboardListener
{
public:
    virtual void onArtboardError(const ArtboardHandle handle,
                                 uint64_t requestId,
                                 std::string error) override
    {
        CHECK(m_hasErrorCallback == false);
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasErrorCallback = true;
    }

    virtual void onDefaultViewModelInfoReceived(
        const ArtboardHandle handle,
        uint64_t requestId,
        std::string viewModelName,
        std::string viewModelInstanceName) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        CHECK(viewModelName == m_expectedViewModel);
        CHECK(viewModelInstanceName == m_expectedViewModelInstance);

        m_hasCallback = true;
    }

    uint64_t m_requestId;
    ArtboardHandle m_handle;
    std::string m_expectedViewModel = "Test All";
    std::string m_expectedViewModelInstance = "Test Default";
    bool m_hasCallback = false;
    bool m_hasErrorCallback = false;
};

TEST_CASE("requestDefaultViewModel", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle goodFile = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    TestArtboardDefaultViewModelListener artboardListener;

    auto artboardHandle =
        commandQueue->instantiateArtboardNamed(goodFile,
                                               "Test Artboard",
                                               &artboardListener);
    artboardListener.m_handle = artboardHandle;

    artboardListener.m_requestId = 0x40;
    commandQueue->requestDefaultViewModelInfo(artboardHandle,
                                              goodFile,
                                              artboardListener.m_requestId);

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(artboardListener.m_hasCallback);
    CHECK(!artboardListener.m_hasErrorCallback);

    FileHandle badFile =
        commandQueue->loadFile(std::vector<uint8_t>(100 * 1024, 0));

    auto badArtbaord = commandQueue->instantiateDefaultArtboard(badFile);

    artboardListener.m_hasCallback = false;

    commandQueue->requestDefaultViewModelInfo(badArtbaord,
                                              goodFile,
                                              artboardListener.m_requestId);
    commandQueue->requestDefaultViewModelInfo(artboardHandle,
                                              badFile,
                                              artboardListener.m_requestId);
    commandQueue->requestDefaultViewModelInfo(badArtbaord,
                                              badFile,
                                              artboardListener.m_requestId);

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(!artboardListener.m_hasCallback);
    CHECK(artboardListener.m_hasErrorCallback);

    std::ifstream tstream("assets/entry.riv", std::ios::binary);
    FileHandle noViewModelFile = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(tstream), {}));

    TestArtboardDefaultViewModelListener noViewModelListener;
    noViewModelListener.m_handle =
        commandQueue->instantiateDefaultArtboard(noViewModelFile,
                                                 &noViewModelListener);

    commandQueue->requestDefaultViewModelInfo(noViewModelListener.m_handle,
                                              noViewModelFile,
                                              noViewModelListener.m_requestId);

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(artboardListener.m_hasErrorCallback);

    commandQueue->disconnect();
    serverThread.join();
}

class TestStateMachineListener : public CommandQueue::StateMachineListener
{
public:
    virtual void onStateMachineDeleted(const StateMachineHandle handle,
                                       uint64_t requestId)
    {
        CHECK(m_handle == handle);
    }

    virtual void onStateMachineSettled(const StateMachineHandle handle,
                                       uint64_t requestId)
    {
        CHECK(m_handle == handle);
        CHECK(m_requestId == requestId);
        m_hasCallbck = true;
    }

    StateMachineHandle m_handle = RIVE_NULL_HANDLE;
    uint64_t m_requestId = 0;
    bool m_hasCallbck = false;
};

TEST_CASE("bindViewModelInstance", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();
    CommandServer server(commandQueue, nullContext.get());

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    auto viewModelHandle =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "Test All",
                                                        "Test Alternate");

    auto artboard = commandQueue->instantiateDefaultArtboard(fileHandle);
    auto stateMachineHandle =
        commandQueue->instantiateDefaultStateMachine(artboard);

    commandQueue->bindViewModelInstance(stateMachineHandle, viewModelHandle);

    commandQueue->runOnce([stateMachineHandle,
                           viewModelHandle](CommandServer* server) {
        auto stateMachine = server->getStateMachineInstance(stateMachineHandle);
        CHECK(stateMachine != nullptr);
        auto viewModel = server->getViewModelInstance(viewModelHandle);
        CHECK(viewModel != nullptr);

        CHECK(stateMachine->artboard()->dataContext()->viewModelInstance() ==
              viewModel->instance());
    });

    auto badInstanceHandle =
        commandQueue->instantiateViewModelInstanceNamed(fileHandle,
                                                        "blah",
                                                        "Test Alternate");

    auto badStateMachineHandle =
        commandQueue->instantiateStateMachineNamed(artboard, "blah");

    // every combo of good / bad handles

    commandQueue->bindViewModelInstance(stateMachineHandle, badInstanceHandle);
    commandQueue->bindViewModelInstance(badStateMachineHandle, viewModelHandle);
    commandQueue->bindViewModelInstance(badStateMachineHandle,
                                        badInstanceHandle);

    server.processCommands();
    commandQueue->processMessages();

    commandQueue->disconnect();
}

TEST_CASE("advanceStateMachine", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();
    CommandServer server(commandQueue, nullContext.get());

    std::ifstream stream("assets/settler.riv", std::ios::binary);
    FileHandle goodFile = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));

    auto artboardHandle = commandQueue->instantiateDefaultArtboard(goodFile);

    TestStateMachineListener stateMachineListener;
    stateMachineListener.m_handle =
        commandQueue->instantiateDefaultStateMachine(artboardHandle,
                                                     &stateMachineListener);

    commandQueue->advanceStateMachine(stateMachineListener.m_handle, 10);
    commandQueue->advanceStateMachine(stateMachineListener.m_handle,
                                      10.0,
                                      stateMachineListener.m_requestId);
    // the last advance is what actually settles the statemachine, so we store
    // that id.

    stateMachineListener.m_requestId = 0x50;
    commandQueue->advanceStateMachine(stateMachineListener.m_handle,
                                      10.0,
                                      stateMachineListener.m_requestId);

    server.processCommands();
    commandQueue->processMessages();

    CHECK(stateMachineListener.m_hasCallbck);

    TestStateMachineListener badStateMachineListener;

    badStateMachineListener.m_handle =
        commandQueue->instantiateStateMachineNamed(artboardHandle,
                                                   "blah blah",
                                                   &badStateMachineListener);
    badStateMachineListener.m_requestId = 0x51;
    commandQueue->advanceStateMachine(badStateMachineListener.m_handle,
                                      10,
                                      badStateMachineListener.m_requestId);

    server.processCommands();
    commandQueue->processMessages();

    CHECK(!badStateMachineListener.m_hasCallbck);
}

class DeleteFileListener : public CommandQueue::FileListener
{
public:
    virtual void onFileDeleted(const FileHandle handle,
                               uint64_t requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    uint64_t m_requestId;
    FileHandle m_handle;
    bool m_hasCallback = false;
};

class DeleteArtboardListener : public CommandQueue::ArtboardListener
{
public:
    virtual void onArtboardDeleted(const ArtboardHandle handle,
                                   uint64_t requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    uint64_t m_requestId;
    ArtboardHandle m_handle;
    bool m_hasCallback = false;
};

class DeleteStateMachineListener : public CommandQueue::StateMachineListener
{
public:
    virtual void onStateMachineDeleted(const StateMachineHandle handle,
                                       uint64_t requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    uint64_t m_requestId;
    StateMachineHandle m_handle;
    bool m_hasCallback = false;
};

class DeleteRenderImageListener : public CommandQueue::RenderImageListener
{
public:
    virtual void onRenderImageDeleted(const RenderImageHandle handle,
                                      uint64_t requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    uint64_t m_requestId;
    RenderImageHandle m_handle;
    bool m_hasCallback = false;
};

TEST_CASE("listenerDeleteCallbacks", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    DeleteFileListener fileListener;
    DeleteArtboardListener artboardListener;
    DeleteStateMachineListener stateMachineListener;
    DeleteRenderImageListener renderImageListener;

    std::ifstream stream("assets/entry.riv", std::ios::binary);
    FileHandle goodFile = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        &fileListener);

    auto artboardHandle =
        commandQueue->instantiateArtboardNamed(goodFile,
                                               "New Artboard",
                                               &artboardListener);

    auto stateMachineHandle =
        commandQueue->instantiateStateMachineNamed(artboardHandle,
                                                   "",
                                                   &stateMachineListener);

    std::ifstream imageStream("assets/batdude.png", std::ios::binary);
    auto renderImage = commandQueue->decodeImage(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        &renderImageListener);

    fileListener.m_handle = goodFile;
    artboardListener.m_handle = artboardHandle;
    stateMachineListener.m_handle = stateMachineHandle;
    renderImageListener.m_handle = renderImage;

    CHECK(!fileListener.m_hasCallback);
    CHECK(!artboardListener.m_hasCallback);
    CHECK(!stateMachineListener.m_hasCallback);
    CHECK(!renderImageListener.m_hasCallback);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(!fileListener.m_hasCallback);
    CHECK(!artboardListener.m_hasCallback);
    CHECK(!stateMachineListener.m_hasCallback);
    CHECK(!renderImageListener.m_hasCallback);

    stateMachineListener.m_requestId = 0x50;
    commandQueue->deleteStateMachine(stateMachineHandle,
                                     stateMachineListener.m_requestId);
    artboardListener.m_requestId = 0x51,
    commandQueue->deleteArtboard(artboardHandle, artboardListener.m_requestId);
    fileListener.m_requestId = 0x52;
    commandQueue->deleteFile(goodFile, fileListener.m_requestId);
    renderImageListener.m_requestId = 0x53;
    commandQueue->deleteImage(renderImage, renderImageListener.m_requestId);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(fileListener.m_hasCallback);
    CHECK(artboardListener.m_hasCallback);
    CHECK(stateMachineListener.m_hasCallback);
    CHECK(renderImageListener.m_hasCallback);

    commandQueue->disconnect();
    serverThread.join();
}

class LoadedFileListener : public CommandQueue::FileListener
{
public:
    virtual void onFileLoaded(const FileHandle handle,
                              uint64_t requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    uint64_t m_requestId = 0x10;
    FileHandle m_handle = RIVE_NULL_HANDLE;
    bool m_hasCallback = false;
};

TEST_CASE("fileLoadedCallback", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);
    std::ifstream stream("assets/entry.riv", std::ios::binary);

    LoadedFileListener fileListener;

    fileListener.m_handle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        &fileListener,
        fileListener.m_requestId);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(fileListener.m_hasCallback);

    LoadedFileListener badFileListener;
    badFileListener.m_handle =
        commandQueue->loadFile(std::vector<uint8_t>(1024, {}),
                               &badFileListener,
                               badFileListener.m_requestId);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(!badFileListener.m_hasCallback);

    commandQueue->disconnect();
    serverThread.join();
}

class DecodedImageListener : public CommandQueue::RenderImageListener
{
public:
    virtual void onRenderImageDecoded(const RenderImageHandle handle,
                                      uint64_t requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    uint64_t m_requestId = 0x10;
    RenderImageHandle m_handle = RIVE_NULL_HANDLE;
    bool m_hasCallback = false;
};

class DecodedAudioListener : public CommandQueue::AudioSourceListener
{
public:
    virtual void onAudioSourceDecoded(const AudioSourceHandle handle,
                                      uint64_t requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    uint64_t m_requestId = 0x10;
    AudioSourceHandle m_handle = RIVE_NULL_HANDLE;
    bool m_hasCallback = false;
};

class DecodedFontListener : public CommandQueue::FontListener
{
public:
    virtual void onFontDecoded(const FontHandle handle,
                               uint64_t requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    uint64_t m_requestId = 0x10;
    FontHandle m_handle = RIVE_NULL_HANDLE;
    bool m_hasCallback = false;
};

TEST_CASE("decodedCallbacks", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);
    std::ifstream stream("assets/entry.riv", std::ios::binary);

    DecodedImageListener imageListener;
    DecodedAudioListener audioListener;
    DecodedFontListener fontListener;

    std::ifstream imageStream("assets/batdude.png", std::ios::binary);
    imageListener.m_handle = commandQueue->decodeImage(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(imageStream), {}),
        &imageListener,
        imageListener.m_requestId);

    std::ifstream audioStream("assets/audio/what.wav", std::ios::binary);
    audioListener.m_handle = commandQueue->decodeAudio(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(audioStream), {}),
        &audioListener,
        audioListener.m_requestId);

    std::ifstream fontStream("assets/fonts/OpenSans-Italic.ttf",
                             std::ios::binary);
    fontListener.m_handle = commandQueue->decodeFont(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(fontStream), {}),
        &fontListener,
        fontListener.m_requestId);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(imageListener.m_hasCallback);
    CHECK(audioListener.m_hasCallback);
    CHECK(fontListener.m_hasCallback);

    DecodedImageListener badimageListener;
    DecodedAudioListener badaudioListener;
    DecodedFontListener badfontListener;

    badimageListener.m_handle =
        commandQueue->decodeImage(std::vector<uint8_t>(1024, {}),
                                  &badimageListener,
                                  badimageListener.m_requestId);

    badaudioListener.m_handle =
        commandQueue->decodeAudio(std::vector<uint8_t>(1024, {}),
                                  &badaudioListener,
                                  badaudioListener.m_requestId);

    badfontListener.m_handle =
        commandQueue->decodeFont(std::vector<uint8_t>(1024, {}),
                                 &badfontListener,
                                 badfontListener.m_requestId);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(!badimageListener.m_hasCallback);
    CHECK(!badaudioListener.m_hasCallback);
    CHECK(!badfontListener.m_hasCallback);

    commandQueue->disconnect();
    serverThread.join();
}

TEST_CASE("listenerLifeTimes", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/entry.riv", std::ios::binary);

    CommandQueue::FileListener fileListener;
    CommandQueue::ArtboardListener artboardListener;
    CommandQueue::StateMachineListener stateMachineListener;

    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        &fileListener);

    auto artboardHandle =
        commandQueue->instantiateArtboardNamed(fileHandle,
                                               "New Artboard",
                                               &artboardListener);

    auto stateMachineHandle =
        commandQueue->instantiateDefaultStateMachine(artboardHandle,
                                                     &stateMachineListener);

    commandQueue->requestArtboardNames(fileHandle);
    commandQueue->requestStateMachineNames(artboardHandle);

    CHECK(commandQueue->testing_getFileListener(fileHandle));
    CHECK(commandQueue->testing_getArtboardListener(artboardHandle));
    CHECK(commandQueue->testing_getStateMachineListener(stateMachineHandle));

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CommandQueue::FileListener deleteFileListener;
    CommandQueue::ArtboardListener deleteArtboardListener;
    CommandQueue::StateMachineListener deleteStateMachineListener;

    FileHandle dFileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        &deleteFileListener);

    auto dArtboardHandle =
        commandQueue->instantiateArtboardNamed(fileHandle,
                                               "New Artboard",
                                               &deleteArtboardListener);

    auto dStateMachineHandle = commandQueue->instantiateDefaultStateMachine(
        artboardHandle,
        &deleteStateMachineListener);

    CHECK(commandQueue->testing_getFileListener(dFileHandle));
    CHECK(commandQueue->testing_getArtboardListener(dArtboardHandle));
    CHECK(commandQueue->testing_getStateMachineListener(dStateMachineHandle));

    commandQueue->deleteFile(dFileHandle);
    commandQueue->deleteArtboard(dArtboardHandle);
    commandQueue->deleteStateMachine(dStateMachineHandle);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(commandQueue->testing_getFileListener(dFileHandle));
    CHECK(commandQueue->testing_getArtboardListener(dArtboardHandle));
    CHECK(commandQueue->testing_getStateMachineListener(dStateMachineHandle));

    commandQueue->disconnect();
    serverThread.join();

    FileHandle fh;

    {
        CommandQueue::FileListener listener;
        fh = commandQueue->loadFile(std::vector<uint8_t>(100 * 1024, 0),
                                    &listener);
        CHECK(commandQueue->testing_getFileListener(fh));
    }

    CHECK(!commandQueue->testing_getFileListener(fh));

    ArtboardHandle ah;
    {
        CommandQueue::ArtboardListener listener;
        ah = commandQueue->instantiateDefaultArtboard(fh, &listener);
        CHECK(commandQueue->testing_getArtboardListener(ah));
    }
    CHECK(!commandQueue->testing_getArtboardListener(ah));

    StateMachineHandle sh;
    {
        CommandQueue::StateMachineListener listener;
        sh = commandQueue->instantiateDefaultStateMachine(ah, &listener);
        CHECK(commandQueue->testing_getStateMachineListener(sh));
    }
    CHECK(!commandQueue->testing_getStateMachineListener(sh));

    CHECK(commandQueue->testing_getFileListener(fileHandle));
    CHECK(commandQueue->testing_getArtboardListener(artboardHandle));
    CHECK(commandQueue->testing_getStateMachineListener(stateMachineHandle));

    // If we move we should now point to the new listener
    CommandQueue::FileListener newFileListener = std::move(fileListener);
    auto listenerPtr = commandQueue->testing_getFileListener(fileHandle);
    CHECK(&newFileListener == listenerPtr);

    CommandQueue::ArtboardListener newArtboardListener =
        std::move(artboardListener);
    auto listenerPtr1 =
        commandQueue->testing_getArtboardListener(artboardHandle);
    CHECK(&newArtboardListener == listenerPtr1);

    CommandQueue::StateMachineListener newStateMachineListener =
        std::move(stateMachineListener);
    auto listenerPtr2 =
        commandQueue->testing_getStateMachineListener(stateMachineHandle);
    CHECK(&newStateMachineListener == listenerPtr2);

    // force unref the commandQueue to ensure it stays alive for listeners
    commandQueue.reset();

    // after this we are checking the destructors for fileListener,
    // artboardListener and stateMachineListener as they should gracefully
    // remove themselves from the commandQeueue even though the ref here is gone
}

TEST_CASE("empty test for code cove", "[CommandQueue]")
{
    CommandQueue::FileListener fileL;
    CommandQueue::ArtboardListener artboardL;
    CommandQueue::StateMachineListener statemachineL;

    std::vector<std::string> emptyVector;

    fileL.onFileDeleted(0, 0);
    fileL.onArtboardsListed(0, 0, emptyVector);

    artboardL.onArtboardDeleted(0, 0);
    artboardL.onStateMachinesListed(0, 0, emptyVector);

    statemachineL.onStateMachineDeleted(0, 0);
    statemachineL.onStateMachineSettled(0, 0);

    CHECK(true);
}

// Helps with repetition of checks in the following test
void checkStateMachineBool(rcp<CommandQueue>& commandQueue,
                           StateMachineHandle handle,
                           const char* boolName,
                           bool expectedValue)
{
    commandQueue->runOnce(
        [handle, boolName, expectedValue](CommandServer* server) {
            rive::StateMachineInstance* sm =
                server->getStateMachineInstance(handle);
            CHECK(sm->getBool(boolName)->value() == expectedValue);
        });
}

TEST_CASE("pointer input", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/pointer_events.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    ArtboardHandle artboardHandle =
        commandQueue->instantiateDefaultArtboard(fileHandle);
    StateMachineHandle smHandle =
        commandQueue->instantiateDefaultStateMachine(artboardHandle);

    commandQueue->runOnce(
        [fileHandle, artboardHandle, smHandle](CommandServer* server) {
            CHECK(fileHandle != RIVE_NULL_HANDLE);
            CHECK(server->getFile(fileHandle) != nullptr);
            CHECK(artboardHandle != RIVE_NULL_HANDLE);
            CHECK(server->getArtboardInstance(artboardHandle) != nullptr);
            CHECK(smHandle != RIVE_NULL_HANDLE);
            CHECK(server->getStateMachineInstance(smHandle) != nullptr);
        });

    // Prime for events by advancing once
    commandQueue->advanceStateMachine(smHandle, 0.0f);

    // The listener object in the bottom-left corner,
    // which toggles `isDown` when receiving down events.
    Vec2D toggleOnDownCorner(425.0f, 425.0f);
    commandQueue->pointerDown(smHandle, {.position = toggleOnDownCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerUp(smHandle, {.position = toggleOnDownCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerDown(smHandle, {.position = toggleOnDownCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);

    // The listener object in the top-left corner,
    // which toggles `isDown` when receiving down or up events.
    Vec2D toggleOnDownOrUpCorner(75.0f, 75.0f);
    commandQueue->pointerDown(smHandle, {.position = toggleOnDownOrUpCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerUp(smHandle, {.position = toggleOnDownOrUpCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);

    // The listener object in the top-right corner,
    // which toggles `isDown` when hovered.
    Vec2D toggleOnHoverCorner(425.0f, 75.0f);
    // Center, which has no pointer listener.
    Vec2D center(250.0f, 250.0f);
    commandQueue->pointerMove(smHandle, {.position = center});
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);
    commandQueue->pointerMove(smHandle, {.position = toggleOnHoverCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerMove(smHandle, {.position = center});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerMove(smHandle, {.position = toggleOnHoverCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);

    // A position off of the artboard, which can be used for pointer exits.
    Vec2D offArtboard(-25.0f, -25.0f);
    commandQueue->pointerDown(smHandle, {.position = toggleOnDownOrUpCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    // Slide off while "holding down" the pointer.
    commandQueue->pointerExit(smHandle, {.position = offArtboard});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    // Release the pointer while off the artboard - should not toggle
    commandQueue->pointerUp(smHandle, {.position = offArtboard});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    // Reset
    commandQueue->pointerUp(smHandle, {.position = toggleOnDownOrUpCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);

    // New test, sliding off and back, but this time releasing back on the
    // artboard.
    commandQueue->pointerDown(smHandle, {.position = toggleOnDownOrUpCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerExit(smHandle, {.position = offArtboard});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerMove(smHandle, {.position = toggleOnDownOrUpCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerUp(smHandle, {.position = toggleOnDownOrUpCorner});
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);

    commandQueue->disconnect();
    serverThread.join();
}

static bool aboutEquals(const Vec2D& l, const Vec2D& r, float theta = 0.0001)
{
    auto b = l - r;
    return abs(b.x) < theta && abs(b.y) < theta;
}

TEST_CASE("pointer input translation", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();
    CommandServer server(commandQueue, nullContext.get());

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    ArtboardHandle artboardHandle =
        commandQueue->instantiateDefaultArtboard(fileHandle);
    StateMachineHandle smHandle =
        commandQueue->instantiateDefaultStateMachine(artboardHandle);
    // artboard is 500x500 so 50x50 should translate to 250,250

    // expected result
    Vec2D expected(250, 250);
    // Inputs
    Vec2D center(50, 50);
    Vec2D size(100, 100);

    commandQueue->runOnce([expected, center, size, smHandle](
                              CommandServer* server) {
        auto instance = server->getStateMachineInstance(smHandle);
        CHECK(instance != nullptr);
        auto translated = server->testing_cursorPosForPointerEvent(
            instance,
            {.fit = Fit::contain, .screenBounds = size, .position = center});
        CHECK(aboutEquals(translated, expected));
    });

    // expected result
    Vec2D expectedTL(125, 125);
    // Inputs
    Vec2D topLeft(25, 25);

    commandQueue->runOnce([expectedTL, topLeft, size, smHandle](
                              CommandServer* server) {
        auto instance = server->getStateMachineInstance(smHandle);
        CHECK(instance != nullptr);
        auto translated = server->testing_cursorPosForPointerEvent(
            instance,
            {.fit = Fit::contain, .screenBounds = size, .position = topLeft});
        CHECK(aboutEquals(translated, expectedTL));
    });

    // expected result
    Vec2D expectedTR(375, 375);
    // Inputs
    Vec2D topRight(75, 75);

    commandQueue->runOnce([expectedTR, topRight, size, smHandle](
                              CommandServer* server) {
        auto instance = server->getStateMachineInstance(smHandle);
        CHECK(instance != nullptr);
        auto translated = server->testing_cursorPosForPointerEvent(
            instance,
            {.fit = Fit::contain, .screenBounds = size, .position = topRight});
        CHECK(aboutEquals(translated, expectedTR));
    });

    // expected result
    Vec2D expectedBR(375, 125);
    // Inputs
    Vec2D bottomRight(75, 25);

    commandQueue->runOnce([bottomRight, expectedBR, size, smHandle](
                              CommandServer* server) {
        auto instance = server->getStateMachineInstance(smHandle);
        CHECK(instance != nullptr);
        auto translated =
            server->testing_cursorPosForPointerEvent(instance,
                                                     {.fit = Fit::contain,
                                                      .screenBounds = size,
                                                      .position = bottomRight});
        CHECK(aboutEquals(translated, expectedBR));
    });

    // expected result
    Vec2D expectedBL(125, 375);
    // Inputs
    Vec2D bottomLeft(25, 75);

    commandQueue->runOnce([bottomLeft, expectedBL, size, smHandle](
                              CommandServer* server) {
        auto instance = server->getStateMachineInstance(smHandle);
        CHECK(instance != nullptr);
        auto translated =
            server->testing_cursorPosForPointerEvent(instance,
                                                     {.fit = Fit::contain,
                                                      .screenBounds = size,
                                                      .position = bottomLeft});
        CHECK(aboutEquals(translated, expectedBL));
    });

    server.processCommands();
    commandQueue->processMessages();

    commandQueue->disconnect();
}

// clang format is removing the needed space between the func names and the (
// clang-format off
#define DEFINE_TEST_CALLBACK(fun, handleType, expectedRequestId)               \
    bool m_##fun##WasCalled = false;                                           \
    virtual void fun (const handleType handle, uint64_t requestId) override    \
    {                                                                          \
        CHECK(handle == m_handle);                                            \
        CHECK(requestId == expectedRequestId);                                \
        m_##fun##WasCalled = true;                                             \
    }

#define DEFINE_TEST_CALLBACK_ONE_PARAM(fun,                                    \
                                       handleType,                             \
                                       expectedRequestId,                      \
                                       paramType,                              \
                                       param)                                  \
    bool m_##fun##WasCalled = false;                                           \
    virtual void fun (const handleType handle,                                 \
                     uint64_t requestId,                                       \
                     paramType param) override                                 \
    {                                                                          \
        CHECK(handle == m_handle);                                            \
        CHECK(requestId == expectedRequestId);                                \
        CHECK(param == m_##param);                                            \
        m_##fun##WasCalled = true;                                             \
    }

#define DEFINE_TEST_CALLBACK_TWO_PARAM(fun,                                    \
                                       handleType,                             \
                                       expectedRequestId,                      \
                                       paramType,                              \
                                       param,                                  \
                                       param2Type,                             \
                                       param2)                                 \
    bool m_##fun##WasCalled = false;                                           \
    virtual void fun (const handleType handle,                                 \
                     uint64_t requestId,                                       \
                     paramType param,                                          \
                     param2Type param2) override                               \
    {                                                                          \
        CHECK(handle == m_handle);                                            \
        CHECK(requestId == expectedRequestId);                                \
        CHECK(param == m_##param);                                            \
        CHECK(param2 == m_##param2);                                          \
        m_##fun##WasCalled = true;                                             \
    }
// clang-format on
#define CHECK_CALLBACK(obj, func) CHECK(obj.m_##func##WasCalled)

class GlobalFileListener : public CommandQueue::FileListener
{
public:
    virtual void onFileError(const FileHandle,
                             uint64_t requestId,
                             std::string error) override
    {}

    DEFINE_TEST_CALLBACK(onFileDeleted, FileHandle, 7);
    DEFINE_TEST_CALLBACK(onFileLoaded, FileHandle, 1);
    DEFINE_TEST_CALLBACK_ONE_PARAM(onArtboardsListed,
                                   FileHandle,
                                   2,
                                   std::vector<std::string>,
                                   artboardNames);
    DEFINE_TEST_CALLBACK_ONE_PARAM(onViewModelsListed,
                                   FileHandle,
                                   3,
                                   std::vector<std::string>,
                                   viewModelNames);

    DEFINE_TEST_CALLBACK_TWO_PARAM(onViewModelInstanceNamesListed,
                                   FileHandle,
                                   4,
                                   std::string,
                                   viewModelNameI,
                                   std::vector<std::string>,
                                   instanceNames);
    DEFINE_TEST_CALLBACK_TWO_PARAM(
        onViewModelPropertiesListed,
        FileHandle,
        5,
        std::string,
        viewModelNameP,
        std::vector<CommandQueue::FileListener::ViewModelPropertyData>,
        properties);
    DEFINE_TEST_CALLBACK_ONE_PARAM(onViewModelEnumsListed,
                                   FileHandle,
                                   6,
                                   std::vector<ViewModelEnum>,
                                   enums);

    FileHandle m_handle;
    std::array<std::string, 3> m_artboardNames = {"Test Artboard",
                                                  "Test Transitions",
                                                  "Test Observation"};
    std::array<std::string, 6> m_viewModelNames = {"Empty VM",
                                                   "Test All",
                                                   "Nested VM",
                                                   "State Transition",
                                                   "Alternate VM",
                                                   "Test Slash"};
    std::array<std::string, 2> m_instanceNames = {"Test Default",
                                                  "Test Alternate"};
    std::array<CommandQueue::FileListener::ViewModelPropertyData, 10>
        m_properties = {
            CommandQueue::FileListener::ViewModelPropertyData{
                DataType::artboard,
                "Test Artboard"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::list,
                                                              "Test List"},
            CommandQueue::FileListener::ViewModelPropertyData{
                DataType::assetImage,
                "Test Image"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::number,
                                                              "Test Num"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::string,
                                                              "Test String"},
            CommandQueue::FileListener::ViewModelPropertyData{
                DataType::enumType,
                "Test Enum",
                "Test Enum Values"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::boolean,
                                                              "Test Bool"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::color,
                                                              "Test Color"},
            CommandQueue::FileListener::ViewModelPropertyData{DataType::trigger,
                                                              "Test Trigger"},
            CommandQueue::FileListener::ViewModelPropertyData{
                DataType::viewModel,
                "Test Nested"}};
    std::array<ViewModelEnum, 1> m_enums = {
        ViewModelEnum{"Test Enum Values", {"Value 1", "Value 2"}}};
    std::string m_viewModelNameI = "Test All";
    std::string m_viewModelNameP = "Test All";
};

class GlobalRenderImageListener : public CommandQueue::RenderImageListener
{
public:
    virtual void onRenderImageError(const RenderImageHandle,
                                    uint64_t requestId,
                                    std::string error) override
    {}

    DEFINE_TEST_CALLBACK(onRenderImageDeleted, RenderImageHandle, 8);

    RenderImageHandle m_handle;
};

class GlobalAudioSourceListener : public CommandQueue::AudioSourceListener
{
public:
    virtual void onAudioSourceError(const AudioSourceHandle,
                                    uint64_t requestId,
                                    std::string error) override
    {}

    DEFINE_TEST_CALLBACK(onAudioSourceDeleted, AudioSourceHandle, 9);

    AudioSourceHandle m_handle;
};

class GlobalFontListener : public CommandQueue::FontListener
{
public:
    virtual void onFontError(const FontHandle,
                             uint64_t requestId,
                             std::string error) override
    {}

    DEFINE_TEST_CALLBACK(onFontDeleted, FontHandle, 10);

    FontHandle m_handle;
};

class GlobalArtboardListener : public CommandQueue::ArtboardListener
{
public:
    virtual void onArtboardError(const ArtboardHandle,
                                 uint64_t requestId,
                                 std::string error) override
    {}

    DEFINE_TEST_CALLBACK(onArtboardDeleted, ArtboardHandle, 12);
    DEFINE_TEST_CALLBACK_ONE_PARAM(onStateMachinesListed,
                                   ArtboardHandle,
                                   11,
                                   std::vector<std::string>,
                                   stateMachineNames);
    DEFINE_TEST_CALLBACK_TWO_PARAM(onDefaultViewModelInfoReceived,
                                   ArtboardHandle,
                                   20,
                                   std::string,
                                   viewModelName,
                                   std::string,
                                   instanceName);

    ArtboardHandle m_handle;
    std::array<std::string, 1> m_stateMachineNames = {"SM"};
    std::string m_viewModelName = "Test All";
    std::string m_instanceName = "Test Default";
};

class GlobalViewModelInstanceListener
    : public CommandQueue::ViewModelInstanceListener
{
public:
    virtual void onViewModelInstanceError(const ViewModelInstanceHandle,
                                          uint64_t requestId,
                                          std::string error) override
    {}

    DEFINE_TEST_CALLBACK(onViewModelDeleted, ViewModelInstanceHandle, 15);

    DEFINE_TEST_CALLBACK_ONE_PARAM(onViewModelDataReceived,
                                   ViewModelInstanceHandle,
                                   13,
                                   CommandQueue::ViewModelInstanceData,
                                   instanceData);

    DEFINE_TEST_CALLBACK_TWO_PARAM(onViewModelListSizeReceived,
                                   ViewModelInstanceHandle,
                                   14,
                                   std::string,
                                   path,
                                   size_t,
                                   size);

    size_t m_size = 0;
    std::string m_path = "Test List";
    ViewModelInstanceHandle m_handle;
    CommandQueue::ViewModelInstanceData m_instanceData = {
        .metaData = PropertyData{DataType::boolean, "Test Bool"},
        .boolValue = true};
};

class GlobalStateMachineListener : public CommandQueue::StateMachineListener
{
public:
    virtual void onStateMachineError(const StateMachineHandle,
                                     uint64_t requestId,
                                     std::string error) override
    {}

    DEFINE_TEST_CALLBACK(onStateMachineDeleted, StateMachineHandle, 17);
    DEFINE_TEST_CALLBACK(onStateMachineSettled, StateMachineHandle, 16);

    StateMachineHandle m_handle;
};

TEST_CASE("global Listener", "[CommandQueue]")
{
    GlobalStateMachineListener globalStateMachineListener;
    GlobalViewModelInstanceListener globalViewModelInstanceListener;
    GlobalArtboardListener globalArtboardListener;
    GlobalFontListener globalFontListener;
    GlobalAudioSourceListener globalAudioSourceListener;
    GlobalRenderImageListener globalRenderImageListener;
    GlobalFileListener globalFileListener;

    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    commandQueue->setGlobalFileListener(&globalFileListener);
    commandQueue->setGlobalArtboardListener(&globalArtboardListener);
    commandQueue->setGlobalStateMachineListener(&globalStateMachineListener);
    commandQueue->setGlobalRenderImageListener(&globalRenderImageListener);
    commandQueue->setGlobalAudioSourceListener(&globalAudioSourceListener);
    commandQueue->setGlobalViewModelInstanceListener(
        &globalViewModelInstanceListener);
    commandQueue->setGlobalFontListener(&globalFontListener);

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        nullptr,
        1);
    auto artboardHandle = commandQueue->instantiateDefaultArtboard(fileHandle);
    auto stateMachineHandle =
        commandQueue->instantiateDefaultStateMachine(artboardHandle);
    auto viewModel =
        commandQueue->instantiateDefaultViewModelInstance(fileHandle,
                                                          artboardHandle);

    std::ifstream imageStream("assets/batdude.png", std::ios::binary);
    auto renderImage = commandQueue->decodeImage(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(imageStream), {}));

    std::ifstream audioStream("assets/audio/what.wav", std::ios::binary);
    auto audioSource = commandQueue->decodeAudio(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(audioStream), {}));

    std::ifstream fontStream("assets/fonts/OpenSans-Italic.ttf",
                             std::ios::binary);
    auto font = commandQueue->decodeFont(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(fontStream), {}));

    globalFileListener.m_handle = fileHandle;
    globalArtboardListener.m_handle = artboardHandle;
    globalStateMachineListener.m_handle = stateMachineHandle;
    globalViewModelInstanceListener.m_handle = viewModel;
    globalRenderImageListener.m_handle = renderImage;
    globalAudioSourceListener.m_handle = audioSource;
    globalFontListener.m_handle = font;

    // 1 is create file or fileLoaded callback
    commandQueue->requestArtboardNames(fileHandle, 2);
    commandQueue->requestViewModelNames(fileHandle, 3);
    commandQueue->requestViewModelInstanceNames(fileHandle, "Test All", 4);
    commandQueue->requestViewModelPropertyDefinitions(fileHandle,
                                                      "Test All",
                                                      5);
    commandQueue->requestViewModelEnums(fileHandle, 6);
    commandQueue->requestStateMachineNames(artboardHandle, 11);
    commandQueue->requestDefaultViewModelInfo(artboardHandle, fileHandle, 20);
    commandQueue->requestViewModelInstanceBool(viewModel, "Test Bool", 13);
    commandQueue->requestViewModelInstanceListSize(viewModel, "Test List", 14);
    commandQueue->advanceStateMachine(stateMachineHandle, 1, 16);
    commandQueue->advanceStateMachine(stateMachineHandle, 1, 16);
    commandQueue->advanceStateMachine(stateMachineHandle, 1, 16);

    commandQueue->deleteFont(font, 10);
    commandQueue->deleteStateMachine(stateMachineHandle, 17);
    commandQueue->deleteArtboard(artboardHandle, 12);
    commandQueue->deleteViewModelInstance(viewModel, 15);
    commandQueue->deleteImage(renderImage, 8);
    commandQueue->deleteFile(fileHandle, 7);
    commandQueue->deleteAudio(audioSource, 9);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK_CALLBACK(globalStateMachineListener, onStateMachineDeleted);
    CHECK_CALLBACK(globalStateMachineListener, onStateMachineSettled);

    CHECK_CALLBACK(globalViewModelInstanceListener, onViewModelDeleted);
    CHECK_CALLBACK(globalViewModelInstanceListener, onViewModelDataReceived);
    CHECK_CALLBACK(globalViewModelInstanceListener,
                   onViewModelListSizeReceived);

    CHECK_CALLBACK(globalArtboardListener, onArtboardDeleted);
    CHECK_CALLBACK(globalArtboardListener, onStateMachinesListed);

    CHECK_CALLBACK(globalFileListener, onFileDeleted);
    CHECK_CALLBACK(globalFileListener, onFileLoaded);
    CHECK_CALLBACK(globalFileListener, onArtboardsListed);
    CHECK_CALLBACK(globalFileListener, onViewModelsListed);
    CHECK_CALLBACK(globalFileListener, onViewModelEnumsListed);
    CHECK_CALLBACK(globalFileListener, onViewModelPropertiesListed);
    CHECK_CALLBACK(globalFileListener, onViewModelInstanceNamesListed);

    CHECK_CALLBACK(globalFontListener, onFontDeleted);
    CHECK_CALLBACK(globalAudioSourceListener, onAudioSourceDeleted);
    CHECK_CALLBACK(globalRenderImageListener, onRenderImageDeleted);

    commandQueue->disconnect();
    serverThread.join();
}

// StateMachines depend on Artboards and Artboards depend on Files.
// So if an artboard gets deleted so should the statemachines assosiated with
// it. If a file is deleted so should artboards and therefore statemachines.

TEST_CASE("dependency lifetime management", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    std::ifstream stream("assets/data_bind_test_cmdq.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        nullptr,
        1);

    auto artboardHandle = commandQueue->instantiateDefaultArtboard(fileHandle);
    auto artboardHandle2 = commandQueue->instantiateDefaultArtboard(fileHandle);
    auto artboardHandle3 = commandQueue->instantiateDefaultArtboard(fileHandle);

    auto stateMachine =
        commandQueue->instantiateDefaultStateMachine(artboardHandle);
    auto stateMachine_2 =
        commandQueue->instantiateDefaultStateMachine(artboardHandle);

    auto stateMachine2 =
        commandQueue->instantiateDefaultStateMachine(artboardHandle2);
    auto stateMachine2_2 =
        commandQueue->instantiateDefaultStateMachine(artboardHandle2);

    auto stateMachine3 =
        commandQueue->instantiateDefaultStateMachine(artboardHandle2);
    auto stateMachine3_2 =
        commandQueue->instantiateDefaultStateMachine(artboardHandle2);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(server->getFile(fileHandle) != nullptr);
        CHECK(server->getArtboardInstance(artboardHandle) != nullptr);
        CHECK(server->getArtboardInstance(artboardHandle2) != nullptr);
        CHECK(server->getArtboardInstance(artboardHandle3) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine_2) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine2) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine2_2) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine3) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine3_2) != nullptr);
    });

    commandQueue->deleteArtboard(artboardHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(server->getFile(fileHandle) != nullptr);
        CHECK(server->getArtboardInstance(artboardHandle) == nullptr);
        CHECK(server->getArtboardInstance(artboardHandle2) != nullptr);
        CHECK(server->getArtboardInstance(artboardHandle3) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine_2) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine2) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine2_2) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine3) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine3_2) != nullptr);
    });

    commandQueue->deleteStateMachine(stateMachine2);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(server->getFile(fileHandle) != nullptr);
        CHECK(server->getArtboardInstance(artboardHandle) == nullptr);
        CHECK(server->getArtboardInstance(artboardHandle2) != nullptr);
        CHECK(server->getArtboardInstance(artboardHandle3) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine_2) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine2) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine2_2) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine3) != nullptr);
        CHECK(server->getStateMachineInstance(stateMachine3_2) != nullptr);
    });

    commandQueue->deleteFile(fileHandle);

    commandQueue->runOnce([&](CommandServer* server) {
        CHECK(server->getFile(fileHandle) == nullptr);
        CHECK(server->getArtboardInstance(artboardHandle) == nullptr);
        CHECK(server->getArtboardInstance(artboardHandle2) == nullptr);
        CHECK(server->getArtboardInstance(artboardHandle3) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine_2) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine2) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine2_2) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine3) == nullptr);
        CHECK(server->getStateMachineInstance(stateMachine3_2) == nullptr);
    });

    commandQueue->disconnect();
    serverThread.join();
}
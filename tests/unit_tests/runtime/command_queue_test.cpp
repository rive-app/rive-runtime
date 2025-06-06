/*
 * Copyright 2025 Rive
 */

#include "catch.hpp"

#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/command_queue.hpp"
#include "rive/command_server.hpp"
#include "rive/file.hpp"
#include "common/render_context_null.hpp"
#include <fstream>

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

    // Deleting the file first still works.
    commandQueue->deleteFile(fileHandle);
    commandQueue->runOnce([fileHandle, artboardHandle1](CommandServer* server) {
        CHECK(server->getFile(fileHandle) == nullptr);
        CHECK(server->getArtboardInstance(artboardHandle1) != nullptr);
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
            CHECK(server->getStateMachineInstance(sm2) != nullptr);
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

static constexpr uint32_t MAGIC_NUMBER = 0x50;

class TestFileAssetLoader : public FileAssetLoader
{
public:
    virtual bool loadContents(FileAsset& asset,
                              Span<const uint8_t> inBandBytes,
                              Factory* factory)
    {
        return false;
    }

    uint32_t m_magicNumber = MAGIC_NUMBER;
};

TEST_CASE("load file with asset loader", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();

    CommandServer server(commandQueue, nullContext.get());
    rcp<TestFileAssetLoader> loader = make_rcp<TestFileAssetLoader>();

    std::ifstream stream("assets/entry.riv", std::ios::binary);
    FileHandle fileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}),
        loader);

    CHECK(fileHandle != RIVE_NULL_HANDLE);

    server.processCommands();

    {
        auto aal = server.getFile(fileHandle)->testing_getAssetLoader();
        CHECK(aal == loader.get());
        CHECK(static_cast<TestFileAssetLoader*>(aal)->m_magicNumber ==
              MAGIC_NUMBER);
    }

    std::ifstream hstream("assets/entry.riv", std::ios::binary);
    rcp<TestFileAssetLoader> heapLoader(new TestFileAssetLoader);
    FileHandle heapFileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(hstream), {}),
        heapLoader);

    server.processCommands();

    {
        auto aal = server.getFile(fileHandle)->testing_getAssetLoader();
        CHECK(aal == loader.get());
        CHECK(static_cast<TestFileAssetLoader*>(aal)->m_magicNumber ==
              MAGIC_NUMBER);
    }
    {
        auto aal = server.getFile(heapFileHandle)->testing_getAssetLoader();
        CHECK(aal == heapLoader.get());
        CHECK(static_cast<TestFileAssetLoader*>(aal)->m_magicNumber ==
              MAGIC_NUMBER);
    }

    rcp<TestFileAssetLoader> nullLoader = nullptr;

    std::ifstream nstream("assets/entry.riv", std::ios::binary);
    FileHandle nullFileHandle = commandQueue->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(nstream), {}),
        nullLoader);

    server.processCommands();

    {
        auto aal = server.getFile(fileHandle)->testing_getAssetLoader();
        CHECK(aal == loader.get());
        CHECK(static_cast<TestFileAssetLoader*>(aal)->m_magicNumber ==
              MAGIC_NUMBER);
    }
    {
        auto aal = server.getFile(heapFileHandle)->testing_getAssetLoader();
        CHECK(aal == heapLoader.get());
        CHECK(static_cast<TestFileAssetLoader*>(aal)->m_magicNumber ==
              MAGIC_NUMBER);
    }
    {
        auto aal = server.getFile(nullFileHandle)->testing_getAssetLoader();
        CHECK(aal == nullLoader.get());
    }

    // How to test this better ??
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

    commandQueue->runOnce(
        [bviewModel, dviewModel, nviewModel](CommandServer* server) {
            CHECK(server->getViewModelInstance(bviewModel) != nullptr);
            CHECK(server->getViewModelInstance(dviewModel) != nullptr);
            CHECK(server->getViewModelInstance(nviewModel) != nullptr);
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
    commandQueue->runOnce(
        [bbviewModel, bdviewModel, bnviewModel, bnnviewModel, bnbviewModel](
            CommandServer* server) {
            CHECK(server->getViewModelInstance(bbviewModel) == nullptr);
            CHECK(server->getViewModelInstance(bdviewModel) == nullptr);
            CHECK(server->getViewModelInstance(bnviewModel) == nullptr);
            CHECK(server->getViewModelInstance(bnnviewModel) == nullptr);
            CHECK(server->getViewModelInstance(bnbviewModel) == nullptr);
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
namespace rive
{
bool operator==(const PropertyData& l, const PropertyData& r)
{
    return l.name == r.name && l.type == r.type;
}
} // namespace rive

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
        std::vector<PropertyData> properties) override
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
    std::array<PropertyData, 7> m_expectedProperties = {
        PropertyData{DataType::number, "Test Num"},
        PropertyData{DataType::string, "Test String"},
        PropertyData{DataType::enumType, "Test Enum"},
        PropertyData{DataType::boolean, "Test Bool"},
        PropertyData{DataType::color, "Test Color"},
        PropertyData{DataType::trigger, "Test Trigger"},
        PropertyData{DataType::viewModel, "Test Nested"}};

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

TEST_CASE("listenerDeleteCallbacks", "[CommandQueue]")
{
    auto commandQueue = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandQueue);

    DeleteFileListener fileListener;
    DeleteArtboardListener artboardListener;
    DeleteStateMachineListener stateMachineListener;

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

    fileListener.m_handle = goodFile;
    artboardListener.m_handle = artboardHandle;
    stateMachineListener.m_handle = stateMachineHandle;

    CHECK(!fileListener.m_hasCallback);
    CHECK(!artboardListener.m_hasCallback);
    CHECK(!stateMachineListener.m_hasCallback);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(!fileListener.m_hasCallback);
    CHECK(!artboardListener.m_hasCallback);
    CHECK(!stateMachineListener.m_hasCallback);

    stateMachineListener.m_requestId = 0x50;
    commandQueue->deleteStateMachine(stateMachineHandle,
                                     stateMachineListener.m_requestId);
    artboardListener.m_requestId = 0x51,
    commandQueue->deleteArtboard(artboardHandle, artboardListener.m_requestId);
    fileListener.m_requestId = 0x52;
    commandQueue->deleteFile(goodFile, fileListener.m_requestId);

    wait_for_server(commandQueue.get());
    commandQueue->processMessages();

    CHECK(fileListener.m_hasCallback);
    CHECK(artboardListener.m_hasCallback);
    CHECK(stateMachineListener.m_hasCallback);

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
    commandQueue->pointerDown(smHandle, toggleOnDownCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerUp(smHandle, toggleOnDownCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerDown(smHandle, toggleOnDownCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);

    // The listener object in the top-left corner,
    // which toggles `isDown` when receiving down or up events.
    Vec2D toggleOnDownOrUpCorner(75.0f, 75.0f);
    commandQueue->pointerDown(smHandle, toggleOnDownOrUpCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerUp(smHandle, toggleOnDownOrUpCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);

    // The listener object in the top-right corner,
    // which toggles `isDown` when hovered.
    Vec2D toggleOnHoverCorner(425.0f, 75.0f);
    // Center, which has no pointer listener.
    Vec2D center(250.0f, 250.0f);
    commandQueue->pointerMove(smHandle, center);
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);
    commandQueue->pointerMove(smHandle, toggleOnHoverCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerMove(smHandle, center);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerMove(smHandle, toggleOnHoverCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);

    // A position off of the artboard, which can be used for pointer exits.
    Vec2D offArtboard(-25.0f, -25.0f);
    commandQueue->pointerDown(smHandle, toggleOnDownOrUpCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    // Slide off while "holding down" the pointer.
    commandQueue->pointerExit(smHandle, offArtboard);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    // Release the pointer while off the artboard - should not toggle
    commandQueue->pointerUp(smHandle, offArtboard);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    // Reset
    commandQueue->pointerUp(smHandle, toggleOnDownOrUpCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);

    // New test, sliding off and back, but this time releasing back on the
    // artboard.
    commandQueue->pointerDown(smHandle, toggleOnDownOrUpCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerExit(smHandle, offArtboard);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerMove(smHandle, toggleOnDownOrUpCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", true);
    commandQueue->pointerUp(smHandle, toggleOnDownOrUpCorner);
    checkStateMachineBool(commandQueue, smHandle, "isDown", false);

    commandQueue->disconnect();
    serverThread.join();
}

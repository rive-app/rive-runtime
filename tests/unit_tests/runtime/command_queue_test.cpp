/*
 * Copyright 2025 Rive
 */

#include "catch.hpp"

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

class TestFileListener : public CommandQueue::FileListener
{
public:
    virtual void onArtboardsListed(
        const FileHandle handle,
        RequestId requestId,
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

    RequestId m_requestId;
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

    fileListener.m_requestId = commandQueue->requestArtboardNames(goodFile);

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
        RequestId requestId,
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

    RequestId m_requestId;
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

    artboardListener.m_requestId =
        commandQueue->requestStateMachineNames(artboardHandle);

    wait_for_server(commandQueue.get());

    commandQueue->processMessages();

    CHECK(artboardListener.m_hasCallback);

    FileHandle badFile =
        commandQueue->loadFile(std::vector<uint8_t>(100 * 1024, 0));

    auto badArtbaord = commandQueue->instantiateDefaultArtboard(badFile);

    artboardListener.m_handle = badArtbaord;
    artboardListener.m_hasCallback = false;

    artboardListener.m_requestId =
        commandQueue->requestStateMachineNames(badArtbaord);

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
                                       RequestId requestId)
    {
        CHECK(m_handle == handle);
    }
    /* RequestId in this case is the specific request that caused the
     * statemachine to settle */
    virtual void onStateMachineSettled(const StateMachineHandle handle,
                                       RequestId requestId)
    {
        CHECK(m_handle == handle);
        CHECK(m_requestId == requestId);
        m_hasCallbck = true;
    }

    StateMachineHandle m_handle = RIVE_NULL_HANDLE;
    RequestId m_requestId = 0;
    bool m_hasCallbck = false;
};

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
    stateMachineListener.m_requestId =
        commandQueue->advanceStateMachine(stateMachineListener.m_handle, 10.0);
    // the last advance is what actually settles the statemachine, so we store
    // that id.
    stateMachineListener.m_requestId =
        commandQueue->advanceStateMachine(stateMachineListener.m_handle, 10.0);

    server.processCommands();
    commandQueue->processMessages();

    CHECK(stateMachineListener.m_hasCallbck);

    TestStateMachineListener badStateMachineListener;

    badStateMachineListener.m_handle =
        commandQueue->instantiateStateMachineNamed(artboardHandle,
                                                   "blah blah",
                                                   &badStateMachineListener);
    badStateMachineListener.m_requestId = 0x51;
    badStateMachineListener.m_requestId =
        commandQueue->advanceStateMachine(badStateMachineListener.m_handle, 10);

    server.processCommands();
    commandQueue->processMessages();

    CHECK(!badStateMachineListener.m_hasCallbck);
}

class DeleteFileListener : public CommandQueue::FileListener
{
public:
    virtual void onFileDeleted(const FileHandle handle,
                               RequestId requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    RequestId m_requestId;
    FileHandle m_handle;
    bool m_hasCallback = false;
};

class DeleteArtboardListener : public CommandQueue::ArtboardListener
{
public:
    virtual void onArtboardDeleted(const ArtboardHandle handle,
                                   RequestId requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    RequestId m_requestId;
    ArtboardHandle m_handle;
    bool m_hasCallback = false;
};

class DeleteStateMachineListener : public CommandQueue::StateMachineListener
{
public:
    virtual void onStateMachineDeleted(const StateMachineHandle handle,
                                       RequestId requestId) override
    {
        CHECK(requestId == m_requestId);
        CHECK(handle == m_handle);
        m_hasCallback = true;
    }

    RequestId m_requestId;
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

    stateMachineListener.m_requestId =
        commandQueue->deleteStateMachine(stateMachineHandle);
    artboardListener.m_requestId = commandQueue->deleteArtboard(artboardHandle);
    fileListener.m_requestId = commandQueue->deleteFile(goodFile);

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

    CHECK(!commandQueue->testing_getFileListener(dFileHandle));
    CHECK(!commandQueue->testing_getArtboardListener(dArtboardHandle));
    CHECK(!commandQueue->testing_getStateMachineListener(dStateMachineHandle));

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
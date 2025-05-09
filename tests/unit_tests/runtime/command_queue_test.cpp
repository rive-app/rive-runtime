/*
 * Copyright 2025 Rive
 */

#include "catch.hpp"

#include "rive/animation/state_machine_instance.hpp"
#include "rive/command_queue.hpp"
#include "rive/command_server.hpp"
#include "common/render_context_null.hpp"
#include <fstream>

using namespace rive;

static void server_thread(rcp<CommandQueue> commandBuffer)
{
    std::unique_ptr<gpu::RenderContext> nullContext =
        RenderContextNULL::MakeContext();
    CommandServer server(std::move(commandBuffer), nullContext.get());
    server.serveUntilDisconnect();
}

static void wait_for_server(CommandQueue* commandBuffer)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool complete = false;
    std::unique_lock<std::mutex> lock(mutex);
    commandBuffer->runOnce([&](CommandServer*) {
        std::unique_lock<std::mutex> serverLock(mutex);
        complete = true;
        cv.notify_one();
    });
    while (!complete)
        cv.wait(lock);
}

TEST_CASE("artboard management", "[CommandQueue]")
{
    auto commandBuffer = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandBuffer);

    std::ifstream stream("assets/two_artboards.riv", std::ios::binary);
    FileHandle fileHandle = commandBuffer->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    commandBuffer->runOnce([fileHandle](CommandServer* server) {
        CHECK(fileHandle != RIVE_NULL_HANDLE);
        CHECK(server->getFile(fileHandle) != nullptr);
    });

    ArtboardHandle artboardHandle1 =
        commandBuffer->instantiateArtboardNamed(fileHandle, "One");
    ArtboardHandle artboardHandle2 =
        commandBuffer->instantiateArtboardNamed(fileHandle, "Two");
    ArtboardHandle artboardHandle3 =
        commandBuffer->instantiateArtboardNamed(fileHandle, "Three");
    commandBuffer->runOnce([artboardHandle1, artboardHandle2, artboardHandle3](
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
    commandBuffer->deleteArtboard(artboardHandle3);
    commandBuffer->deleteArtboard(artboardHandle2);
    commandBuffer->runOnce([artboardHandle1, artboardHandle2, artboardHandle3](
                               CommandServer* server) {
        CHECK(server->getArtboardInstance(artboardHandle1) != nullptr);
        CHECK(server->getArtboardInstance(artboardHandle2) == nullptr);
        CHECK(server->getArtboardInstance(artboardHandle3) == nullptr);
    });

    // Deleting the file first still works.
    commandBuffer->deleteFile(fileHandle);
    commandBuffer->runOnce(
        [fileHandle, artboardHandle1](CommandServer* server) {
            CHECK(server->getFile(fileHandle) == nullptr);
            CHECK(server->getArtboardInstance(artboardHandle1) != nullptr);
        });

    commandBuffer->deleteArtboard(artboardHandle1);
    commandBuffer->runOnce([artboardHandle1](CommandServer* server) {
        CHECK(server->getArtboardInstance(artboardHandle1) == nullptr);
    });

    commandBuffer->disconnect();
    serverThread.join();
}

TEST_CASE("state machine management", "[CommandQueue]")
{
    auto commandBuffer = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandBuffer);

    std::ifstream stream("assets/multiple_state_machines.riv",
                         std::ios::binary);
    FileHandle fileHandle = commandBuffer->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    ArtboardHandle artboardHandle =
        commandBuffer->instantiateDefaultArtboard(fileHandle);
    commandBuffer->runOnce([fileHandle, artboardHandle](CommandServer* server) {
        CHECK(fileHandle != RIVE_NULL_HANDLE);
        CHECK(server->getFile(fileHandle) != nullptr);
        CHECK(artboardHandle != RIVE_NULL_HANDLE);
        CHECK(server->getArtboardInstance(artboardHandle) != nullptr);
    });

    StateMachineHandle sm1 =
        commandBuffer->instantiateStateMachineNamed(artboardHandle, "one");
    StateMachineHandle sm2 =
        commandBuffer->instantiateStateMachineNamed(artboardHandle, "two");
    StateMachineHandle sm3 =
        commandBuffer->instantiateStateMachineNamed(artboardHandle, "blahblah");
    commandBuffer->runOnce([sm1, sm2, sm3](CommandServer* server) {
        CHECK(sm1 != RIVE_NULL_HANDLE);
        CHECK(server->getStateMachineInstance(sm1) != nullptr);
        CHECK(sm2 != RIVE_NULL_HANDLE);
        CHECK(server->getStateMachineInstance(sm2) != nullptr);
        // A state machine named "blahblah" doesn't exist.
        CHECK(sm3 != RIVE_NULL_HANDLE);
        CHECK(server->getStateMachineInstance(sm3) == nullptr);
    });

    commandBuffer->deleteFile(fileHandle);
    commandBuffer->deleteArtboard(artboardHandle);
    commandBuffer->deleteStateMachine(sm1);
    commandBuffer->runOnce(
        [fileHandle, artboardHandle, sm1, sm2](CommandServer* server) {
            CHECK(server->getFile(fileHandle) == nullptr);
            CHECK(server->getArtboardInstance(artboardHandle) == nullptr);
            CHECK(server->getStateMachineInstance(sm1) == nullptr);
            CHECK(server->getStateMachineInstance(sm2) != nullptr);
        });

    commandBuffer->deleteStateMachine(sm2);
    commandBuffer->runOnce([sm2](CommandServer* server) {
        CHECK(server->getStateMachineInstance(sm2) == nullptr);
    });

    commandBuffer->disconnect();
    serverThread.join();
}

TEST_CASE("default artboard & state machine", "[CommandQueue]")
{
    auto commandBuffer = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandBuffer);

    std::ifstream stream("assets/entry.riv", std::ios::binary);
    FileHandle fileHandle = commandBuffer->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    ArtboardHandle artboardHandle =
        commandBuffer->instantiateDefaultArtboard(fileHandle);
    StateMachineHandle smHandle =
        commandBuffer->instantiateDefaultStateMachine(artboardHandle);
    commandBuffer->runOnce([artboardHandle, smHandle](CommandServer* server) {
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
        commandBuffer->instantiateArtboardNamed(fileHandle, "");
    StateMachineHandle smHandle2 =
        commandBuffer->instantiateStateMachineNamed(artboardHandle2, "");
    commandBuffer->runOnce([artboardHandle2, smHandle2](CommandServer* server) {
        rive::ArtboardInstance* artboard =
            server->getArtboardInstance(artboardHandle2);
        REQUIRE(artboard != nullptr);
        CHECK(artboard->name() == "New Artboard");
        rive::StateMachineInstance* sm =
            server->getStateMachineInstance(smHandle2);
        REQUIRE(sm != nullptr);
        CHECK(sm->name() == "State Machine 1");
    });

    commandBuffer->disconnect();
    serverThread.join();
}

TEST_CASE("invalid handles", "[CommandQueue]")
{
    auto commandBuffer = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandBuffer);

    std::ifstream stream("assets/entry.riv", std::ios::binary);
    FileHandle goodFile = commandBuffer->loadFile(
        std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {}));
    FileHandle badFile =
        commandBuffer->loadFile(std::vector<uint8_t>(100 * 1024, 0));
    commandBuffer->runOnce([goodFile, badFile](CommandServer* server) {
        CHECK(goodFile != RIVE_NULL_HANDLE);
        CHECK(server->getFile(goodFile) != nullptr);
        CHECK(badFile != RIVE_NULL_HANDLE);
        CHECK(server->getFile(badFile) == nullptr);
    });

    ArtboardHandle goodArtboard =
        commandBuffer->instantiateArtboardNamed(goodFile, "New Artboard");
    ArtboardHandle badArtboard1 =
        commandBuffer->instantiateDefaultArtboard(badFile);
    ArtboardHandle badArtboard2 =
        commandBuffer->instantiateArtboardNamed(badFile, "New Artboard");
    ArtboardHandle badArtboard3 =
        commandBuffer->instantiateArtboardNamed(goodFile, "blahblahblah");
    commandBuffer->runOnce(
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
        commandBuffer->instantiateStateMachineNamed(goodArtboard,
                                                    "State Machine 1");
    StateMachineHandle badSM1 =
        commandBuffer->instantiateStateMachineNamed(badArtboard2,
                                                    "State Machine 1");
    StateMachineHandle badSM2 =
        commandBuffer->instantiateStateMachineNamed(goodArtboard,
                                                    "blahblahblah");
    StateMachineHandle badSM3 =
        commandBuffer->instantiateDefaultStateMachine(badArtboard3);
    commandBuffer->runOnce(
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

    commandBuffer->deleteStateMachine(badSM3);
    commandBuffer->deleteStateMachine(badSM2);
    commandBuffer->deleteStateMachine(badSM1);
    commandBuffer->deleteArtboard(badArtboard3);
    commandBuffer->deleteArtboard(badArtboard2);
    commandBuffer->deleteArtboard(badArtboard1);
    commandBuffer->deleteFile(badFile);
    commandBuffer->runOnce(
        [goodFile, goodArtboard, goodSM](CommandServer* server) {
            CHECK(server->getFile(goodFile) != nullptr);
            CHECK(server->getArtboardInstance(goodArtboard) != nullptr);
            CHECK(server->getStateMachineInstance(goodSM) != nullptr);
        });

    commandBuffer->deleteStateMachine(goodSM);
    commandBuffer->deleteArtboard(goodArtboard);
    commandBuffer->deleteFile(goodFile);
    commandBuffer->runOnce(
        [goodFile, goodArtboard, goodSM](CommandServer* server) {
            CHECK(server->getFile(goodFile) == nullptr);
            CHECK(server->getArtboardInstance(goodArtboard) == nullptr);
            CHECK(server->getStateMachineInstance(goodSM) == nullptr);
        });

    commandBuffer->disconnect();
    serverThread.join();
}

TEST_CASE("draw loops", "[CommandQueue]")
{
    auto commandBuffer = make_rcp<CommandQueue>();
    std::thread serverThread(server_thread, commandBuffer);

    std::atomic_uint64_t frameNumber1 = 0, frameNumber2 = 0;
    std::atomic_uint64_t lastFrameNumber1, lastFrameNumber2;
    auto drawLoop1 = [&frameNumber1](CommandServer*) { ++frameNumber1; };
    auto drawLoop2 = [&frameNumber2](CommandServer*) { ++frameNumber2; };
    commandBuffer->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 == 0);
        CHECK(frameNumber2 == 0);
        lastFrameNumber1 = frameNumber1.load();
        lastFrameNumber2 = frameNumber2.load();
    });

    DrawLoopHandle loopHandle1 = commandBuffer->startDrawLoop(drawLoop1);
    DrawLoopHandle loopHandle2 = commandBuffer->startDrawLoop(drawLoop2);
    do
    {
        wait_for_server(commandBuffer.get());
    } while (frameNumber1 == lastFrameNumber1 ||
             frameNumber2 == lastFrameNumber2);
    commandBuffer->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 > lastFrameNumber1);
        CHECK(frameNumber2 > lastFrameNumber2);
    });

    commandBuffer->stopDrawLoop(loopHandle1);
    commandBuffer->runOnce([&](CommandServer*) {
        lastFrameNumber1 = frameNumber1.load();
        lastFrameNumber2 = frameNumber2.load();
    });

    do
    {
        wait_for_server(commandBuffer.get());
    } while (frameNumber2 == lastFrameNumber2);
    commandBuffer->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 == lastFrameNumber1);
        CHECK(frameNumber2 > lastFrameNumber2);
    });

    loopHandle1 = commandBuffer->startDrawLoop(drawLoop1);
    commandBuffer->stopDrawLoop(loopHandle2);
    commandBuffer->runOnce([&](CommandServer*) {
        lastFrameNumber1 = frameNumber1.load();
        lastFrameNumber2 = frameNumber2.load();
    });

    do
    {
        wait_for_server(commandBuffer.get());
    } while (frameNumber1 == lastFrameNumber1);
    commandBuffer->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 > lastFrameNumber1);
        CHECK(frameNumber2 == lastFrameNumber2);
    });

    commandBuffer->stopDrawLoop(loopHandle1);
    commandBuffer->runOnce([&](CommandServer*) {
        lastFrameNumber1 = frameNumber1.load();
        lastFrameNumber2 = frameNumber2.load();
    });

    for (int i = 0; i < 10; ++i)
    {
        wait_for_server(commandBuffer.get());
    }
    commandBuffer->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 == lastFrameNumber1);
        CHECK(frameNumber2 == lastFrameNumber2);
    });

    loopHandle1 = commandBuffer->startDrawLoop(drawLoop1);
    loopHandle2 = commandBuffer->startDrawLoop(drawLoop2);
    do
    {
        wait_for_server(commandBuffer.get());
    } while (frameNumber1 == lastFrameNumber1 ||
             frameNumber2 == lastFrameNumber2);
    commandBuffer->runOnce([&](CommandServer*) {
        CHECK(frameNumber1 > lastFrameNumber1);
        CHECK(frameNumber2 > lastFrameNumber2);
    });

    // Leave the draw loops running; test tearing down the command buffer with
    // active draw loops.
    commandBuffer->disconnect();
    serverThread.join();
}

/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/object_stream.hpp"
#include "rive/refcnt.hpp"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// Macros for defining and working with command buffer handles.
#if INTPTR_MAX == INT64_MAX
static_assert(sizeof(void*) == 8, "expected a 64-bit environment");
// Define handles as pointers to forward-declared types for type safety.
#define RIVE_DEFINE_HANDLE(NAME) using NAME = struct NAME##_HandlePlaceholder*;
#define RIVE_NULL_HANDLE nullptr
#elif INTPTR_MAX == INT32_MAX
static_assert(sizeof(void*) == 4, "expected a 32-bit environment");
#define RIVE_DEFINE_HANDLE(NAME) using NAME = uint64_t;
#define RIVE_NULL_HANDLE 0
#else
#error "environment not 32 or 64-bit."
#endif

namespace rive
{
class Factory;
class File;
class ArtboardInstance;
class StateMachineInstance;
class CommandServer;

RIVE_DEFINE_HANDLE(FileHandle);
RIVE_DEFINE_HANDLE(ArtboardHandle);
RIVE_DEFINE_HANDLE(StateMachineHandle);
RIVE_DEFINE_HANDLE(DrawLoopHandle);

// Function poimter that gets called back from the server thread.
using CommandServerCallback = std::function<void(CommandServer*)>;

// Client-side recorder for commands that will be executed by a
// CommandServer.
class CommandQueue : public RefCnt<CommandQueue>
{
public:
    CommandQueue();
    ~CommandQueue();

    FileHandle loadFile(std::vector<uint8_t> rivBytes);
    void deleteFile(FileHandle);

    ArtboardHandle instantiateArtboardNamed(FileHandle, std::string name);
    ArtboardHandle instantiateDefaultArtboard(FileHandle fileHandle)
    {
        return instantiateArtboardNamed(fileHandle, "");
    }
    void deleteArtboard(ArtboardHandle);

    StateMachineHandle instantiateStateMachineNamed(ArtboardHandle,
                                                    std::string name);
    StateMachineHandle instantiateDefaultStateMachine(
        ArtboardHandle artboardHandle)
    {
        return instantiateStateMachineNamed(artboardHandle, "");
    }
    void deleteStateMachine(StateMachineHandle);

    DrawLoopHandle startDrawLoop(CommandServerCallback);
    void stopDrawLoop(DrawLoopHandle);

    // Executes a one-time callback on the server. This may eventualy become a
    // testing-only method.
    void runOnce(CommandServerCallback);

    void disconnect();

private:
    enum class Command
    {
        loadFile,
        deleteFile,
        instantiateArtboard,
        deleteArtboard,
        instantiateStateMachine,
        deleteStateMachine,
        startDrawLoop,
        stopDrawLoop,
        runOnce,
        disconnect,
    };

    friend class CommandServer;

    uint64_t m_currentFileHandleIdx = 0;
    uint64_t m_currentArtboardHandleIdx = 0;
    uint64_t m_currentStateMachineHandleIdx = 0;
    uint64_t m_currentDrawLoopHandleIdx = 0;

    std::mutex m_mutex;
    std::condition_variable m_conditionVariable;
    TrivialObjectStream m_commandStream;
    ObjectStream<std::vector<uint8_t>> m_byteVectors;
    ObjectStream<std::string> m_names;
    ObjectStream<CommandServerCallback> m_callbacks;
};
}; // namespace rive

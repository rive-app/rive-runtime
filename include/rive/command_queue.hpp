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
#include <unordered_map>
#include "file_asset_loader.hpp"

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
RIVE_DEFINE_HANDLE(DrawKey);

using RequestId = uint64_t;

// Function poimter that gets called back from the server thread.
using CommandServerCallback = std::function<void(CommandServer*)>;
using CommandServerDrawCallback = std::function<void(DrawKey, CommandServer*)>;

// Client-side recorder for commands that will be executed by a
// CommandServer.
class CommandQueue : public RefCnt<CommandQueue>
{
public:
    template <typename ListenerType, typename HandleType> class ListenerBase
    {
    public:
        friend class CommandQueue;

        ListenerBase(const ListenerBase&) = delete;
        ListenerBase& operator=(const ListenerBase&) = delete;
        ListenerBase(ListenerBase&& other) :
            m_owningQueue(std::move(other.m_owningQueue)),
            m_handle(std::move(other.m_handle))
        {
            if (m_owningQueue)
            {
                m_owningQueue->unregisterListener(
                    m_handle,
                    static_cast<ListenerType*>(&other));
                m_owningQueue->registerListener(
                    m_handle,
                    static_cast<ListenerType*>(this));
            }
        }
        ~ListenerBase()
        {
            if (m_owningQueue)
                m_owningQueue->unregisterListener(
                    m_handle,
                    static_cast<ListenerType*>(this));
        }
        ListenerBase() : m_handle(RIVE_NULL_HANDLE) {}

    private:
        rcp<CommandQueue> m_owningQueue;
        HandleType m_handle;
    };

    class FileListener
        : public CommandQueue::ListenerBase<FileListener, FileHandle>
    {
    public:
        virtual void onFileDeleted(const FileHandle, RequestId) {}

        virtual void onArtboardsListed(const FileHandle,
                                       RequestId,
                                       std::vector<std::string> artboardNames)
        {}
    };

    class ArtboardListener
        : public CommandQueue::ListenerBase<ArtboardListener, ArtboardHandle>
    {
    public:
        virtual void onArtboardDeleted(const ArtboardHandle, RequestId) {}

        virtual void onStateMachinesListed(
            const ArtboardHandle,
            RequestId,
            std::vector<std::string> artboardNames)
        {}
    };

    class StateMachineListener
        : public CommandQueue::ListenerBase<StateMachineListener,
                                            StateMachineHandle>
    {
    public:
        virtual void onStateMachineDeleted(const StateMachineHandle, RequestId)
        {}
        // RequestId in this case is the specific request that caused the
        // statemachine to settle
        virtual void onStateMachineSettled(const StateMachineHandle, RequestId)
        {}
    };

    CommandQueue();
    ~CommandQueue();

    FileHandle loadFile(std::vector<uint8_t> rivBytes,
                        rcp<FileAssetLoader>,
                        FileListener* listener = nullptr,
                        RequestId* outId = nullptr);

    FileHandle loadFile(std::vector<uint8_t> rivBytes,
                        FileListener* listener = nullptr,
                        RequestId* outId = nullptr)
    {
        return loadFile(std::move(rivBytes), nullptr, listener, outId);
    }

    RequestId deleteFile(FileHandle);

    ArtboardHandle instantiateArtboardNamed(
        FileHandle,
        std::string name,
        ArtboardListener* listener = nullptr,
        RequestId* outId = nullptr);
    ArtboardHandle instantiateDefaultArtboard(
        FileHandle fileHandle,
        ArtboardListener* listener = nullptr,
        RequestId* outId = nullptr)
    {
        return instantiateArtboardNamed(fileHandle, "", listener, outId);
    }

    RequestId deleteArtboard(ArtboardHandle);

    StateMachineHandle instantiateStateMachineNamed(
        ArtboardHandle,
        std::string name,
        StateMachineListener* listener = nullptr,
        RequestId* outId = nullptr);
    StateMachineHandle instantiateDefaultStateMachine(
        ArtboardHandle artboardHandle,
        StateMachineListener* listener = nullptr,
        RequestId* outId = nullptr)
    {
        return instantiateStateMachineNamed(artboardHandle,
                                            "",
                                            listener,
                                            outId);
    }

    RequestId advanceStateMachine(StateMachineHandle, float timeToAdvance);

    RequestId deleteStateMachine(StateMachineHandle);

    // Create unique draw key for draw.
    DrawKey createDrawKey();

    // Executes a one-time callback on the server. This may eventualy become a
    // testing-only method.
    void runOnce(CommandServerCallback);

    // Run draw function for given draw key, only the latest function passed
    // will be run per pollCommands.
    void draw(DrawKey, CommandServerDrawCallback);

#ifdef TESTING
    // Sends a commandLoopBreak command to the server. This will cause the
    // processCommands to return even if there are more commands to consume.
    // This does not shutdown the server, it only causes processCommands to
    // return. To continue processing commands, another call to processCommands
    // is required.
    void testing_commandLoopBreak();

    FileListener* testing_getFileListener(FileHandle);
    ArtboardListener* testing_getArtboardListener(ArtboardHandle);
    StateMachineListener* testing_getStateMachineListener(StateMachineHandle);
#endif

    void disconnect();

    RequestId requestArtboardNames(FileHandle);
    RequestId requestStateMachineNames(ArtboardHandle);

    // Consume all messages received from the server.
    void processMessages();

private:
    void registerListener(FileHandle handle, FileListener* listener)
    {
        assert(listener);
        assert(m_fileListeners.find(handle) == m_fileListeners.end());
        m_fileListeners.insert({handle, listener});
    }

    void registerListener(ArtboardHandle handle, ArtboardListener* listener)
    {
        assert(listener);
        assert(m_artboardListeners.find(handle) == m_artboardListeners.end());
        m_artboardListeners.insert({handle, listener});
    }

    void registerListener(StateMachineHandle handle,
                          StateMachineListener* listener)
    {
        assert(listener);
        assert(m_stateMachineListeners.find(handle) ==
               m_stateMachineListeners.end());
        m_stateMachineListeners.insert({handle, listener});
    }

    // On 32-bit architectures, Handles are all uint64_t (not unique types), so
    // it will think we are re-declaring unregisterListener 3 times if we don't
    // add the listener pointer (even though it is not needed).

    void unregisterListener(FileHandle handle, FileListener* listener)
    {
        m_fileListeners.erase(handle);
    }

    void unregisterListener(ArtboardHandle handle, ArtboardListener* listener)
    {
        m_artboardListeners.erase(handle);
    }

    void unregisterListener(StateMachineHandle handle,
                            StateMachineListener* listener)
    {
        m_stateMachineListeners.erase(handle);
    }

    enum class Command
    {
        loadFile,
        deleteFile,
        instantiateArtboard,
        deleteArtboard,
        instantiateStateMachine,
        deleteStateMachine,
        advanceStateMachine,
        runOnce,
        draw,
        disconnect,
        // This will cause processCommands to return once received. We want to
        // ensure that we do not indefinetly block the calling thread. This is
        // how we achieve that.
        commandLoopBreak,
        // messages
        listArtboards,
        listStateMachines
    };

    enum class Message
    {
        // Same as commandLoopBreak for processMessages
        messageLoopBreak,
        artboardsListed,
        stateMachinesListed,
        fileDeleted,
        artboardDeleted,
        stateMachineDeleted,
        stateMachineSettled
    };

    friend class CommandServer;

    uint64_t m_currentFileHandleIdx = 0;
    uint64_t m_currentArtboardHandleIdx = 0;
    uint64_t m_currentStateMachineHandleIdx = 0;
    uint64_t m_currentRequestIdIdx = 0;
    uint64_t m_currentDrawKeyIdx = 0;

    std::mutex m_commandMutex;
    std::condition_variable m_commandConditionVariable;
    PODStream m_commandStream;
    ObjectStream<std::vector<uint8_t>> m_byteVectors;
    ObjectStream<std::string> m_names;
    ObjectStream<CommandServerCallback> m_callbacks;
    ObjectStream<CommandServerDrawCallback> m_drawCallbacks;

    // Messages streams
    std::mutex m_messageMutex;
    PODStream m_messageStream;
    ObjectStream<std::string> m_messageNames;

    // Listeners
    std::unordered_map<FileHandle, FileListener*> m_fileListeners;
    std::unordered_map<ArtboardHandle, ArtboardListener*> m_artboardListeners;
    std::unordered_map<StateMachineHandle, StateMachineListener*>
        m_stateMachineListeners;
};

}; // namespace rive

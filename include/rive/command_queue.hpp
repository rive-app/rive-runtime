/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/object_stream.hpp"
#include "rive/refcnt.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"

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
RIVE_DEFINE_HANDLE(ViewModelInstanceHandle);
RIVE_DEFINE_HANDLE(DrawKey);

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
        virtual void onFileDeleted(const FileHandle, uint64_t requestId) {}

        virtual void onArtboardsListed(const FileHandle,
                                       uint64_t requestId,
                                       std::vector<std::string> artboardNames)
        {}

        virtual void onViewModelsListed(const FileHandle,
                                        uint64_t requestId,
                                        std::vector<std::string> viewModelNames)
        {}

        virtual void onViewModelInstanceNamesListed(
            const FileHandle,
            uint64_t requestId,
            std::string viewModelName,
            std::vector<std::string> instanceNames)
        {}

        virtual void onViewModelPropertiesListed(
            const FileHandle,
            uint64_t requestId,
            std::string viewModelName,
            std::vector<PropertyData> properties)
        {}
    };

    class ArtboardListener
        : public CommandQueue::ListenerBase<ArtboardListener, ArtboardHandle>
    {
    public:
        virtual void onArtboardDeleted(const ArtboardHandle, uint64_t requestId)
        {}

        virtual void onStateMachinesListed(
            const ArtboardHandle,
            uint64_t requestId,
            std::vector<std::string> stateMachineNames)
        {}
    };

    class ViewModelInstanceListener
        : public CommandQueue::ListenerBase<ViewModelInstanceListener,
                                            ViewModelInstanceHandle>
    {
    public:
        virtual void onViewModelDeleted(const ViewModelInstanceHandle,
                                        uint64_t requestId)
        {}
    };

    class StateMachineListener
        : public CommandQueue::ListenerBase<StateMachineListener,
                                            StateMachineHandle>
    {
    public:
        virtual void onStateMachineDeleted(const StateMachineHandle,
                                           uint64_t requestId)
        {}
        // requestId in this case is the specific request that caused the
        // statemachine to settle
        virtual void onStateMachineSettled(const StateMachineHandle,
                                           uint64_t requestId)
        {}
    };

    CommandQueue();
    ~CommandQueue();

    FileHandle loadFile(std::vector<uint8_t> rivBytes,
                        rcp<FileAssetLoader>,
                        FileListener* listener = nullptr,
                        uint64_t requestId = 0);

    FileHandle loadFile(std::vector<uint8_t> rivBytes,
                        FileListener* listener = nullptr,
                        uint64_t requestId = 0)
    {
        return loadFile(std::move(rivBytes), nullptr, listener, requestId);
    }

    void deleteFile(FileHandle, uint64_t requestId = 0);

    ArtboardHandle instantiateArtboardNamed(
        FileHandle,
        std::string name,
        ArtboardListener* listener = nullptr,
        uint64_t requestId = 0);
    ArtboardHandle instantiateDefaultArtboard(
        FileHandle fileHandle,
        ArtboardListener* listener = nullptr,
        uint64_t requestId = 0)
    {
        return instantiateArtboardNamed(fileHandle, "", listener, requestId);
    }

    void deleteArtboard(ArtboardHandle, uint64_t requestId = 0);

    ViewModelInstanceHandle instantiateBlankViewModelInstance(
        FileHandle fileHandle,
        ArtboardHandle artboardHandle,
        ViewModelInstanceListener* listener = nullptr,
        uint64_t requestId = 0);

    ViewModelInstanceHandle instantiateBlankViewModelInstance(
        FileHandle fileHandle,
        std::string viewModelName,
        ViewModelInstanceListener* listener = nullptr,
        uint64_t requestId = 0);

    ViewModelInstanceHandle instantiateDefaultViewModelInstance(
        FileHandle fileHandle,
        ArtboardHandle artboardHandle,
        ViewModelInstanceListener* listener = nullptr,
        uint64_t requestId = 0)
    {
        return instantiateViewModelInstanceNamed(fileHandle,
                                                 artboardHandle,
                                                 "",
                                                 listener,
                                                 requestId);
    }

    ViewModelInstanceHandle instantiateDefaultViewModelInstance(
        FileHandle fileHandle,
        std::string viewModelName,
        ViewModelInstanceListener* listener = nullptr,
        uint64_t requestId = 0)
    {
        return instantiateViewModelInstanceNamed(fileHandle,
                                                 viewModelName,
                                                 "",
                                                 listener,
                                                 requestId);
    }

    ViewModelInstanceHandle instantiateViewModelInstanceNamed(
        FileHandle,
        ArtboardHandle,
        std::string viewModelInstanceName,
        ViewModelInstanceListener* listener = nullptr,
        uint64_t requestId = 0);

    ViewModelInstanceHandle instantiateViewModelInstanceNamed(
        FileHandle,
        std::string viewModelName,
        std::string viewModelInstanceName,
        ViewModelInstanceListener* listener = nullptr,
        uint64_t requestId = 0);

    void deleteViewModelInstance(ViewModelInstanceHandle,
                                 uint64_t requestId = 0);

    StateMachineHandle instantiateStateMachineNamed(
        ArtboardHandle,
        std::string name,
        StateMachineListener* listener = nullptr,
        uint64_t requestId = 0);
    StateMachineHandle instantiateDefaultStateMachine(
        ArtboardHandle artboardHandle,
        StateMachineListener* listener = nullptr,
        uint64_t requestId = 0)
    {
        return instantiateStateMachineNamed(artboardHandle,
                                            "",
                                            listener,
                                            requestId);
    }

    void bindViewModelInstance(StateMachineHandle,
                               ViewModelInstanceHandle,
                               uint64_t requestId = 0);

    void advanceStateMachine(StateMachineHandle,
                             float timeToAdvance,
                             uint64_t requestId = 0);

    // Pointer events
    void pointerMove(StateMachineHandle, Vec2D position);
    void pointerDown(StateMachineHandle, Vec2D position);
    void pointerUp(StateMachineHandle, Vec2D position);
    void pointerExit(StateMachineHandle, Vec2D position);

    void deleteStateMachine(StateMachineHandle, uint64_t requestId = 0);

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

    void requestViewModelNames(FileHandle, uint64_t requestId = 0);
    void requestArtboardNames(FileHandle, uint64_t requestId = 0);
    void requestViewModelPropertyDefinitions(FileHandle,
                                             std::string viewModelName,
                                             uint64_t requestId = 0);
    void requestViewModelInstanceNames(FileHandle,
                                       std::string viewModelName,
                                       uint64_t requestId = 0);
    void requestStateMachineNames(ArtboardHandle, uint64_t requestId = 0);

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

    void registerListener(ViewModelInstanceHandle handle,
                          ViewModelInstanceListener* listener)
    {
        assert(listener);
        assert(m_viewModelListeners.find(handle) == m_viewModelListeners.end());
        m_viewModelListeners.insert({handle, listener});
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

    void unregisterListener(ViewModelInstanceHandle handle,
                            ViewModelInstanceListener* listener)
    {
        m_viewModelListeners.erase(handle);
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
        instantiateViewModel,
        instantiateBlankViewModel,
        instantiateViewModelForArtboard,
        instantiateBlankViewModelForArtboard,
        deleteViewModel,
        instantiateStateMachine,
        deleteStateMachine,
        advanceStateMachine,
        bindViewModelInstance,
        runOnce,
        draw,
        pointerMove,
        pointerDown,
        pointerUp,
        pointerExit,
        disconnect,
        // This will cause processCommands to return once received. We want to
        // ensure that we do not indefinetly block the calling thread. This is
        // how we achieve that.
        commandLoopBreak,
        // messages
        listArtboards,
        listStateMachines,
        listViewModels,
        listViewModelInstanceNames,
        listViewModelProperties
    };

    enum class Message
    {
        // Same as commandLoopBreak for processMessages
        messageLoopBreak,
        artboardsListed,
        stateMachinesListed,
        viewModelsListend,
        viewModelInstanceNamesListed,
        viewModelPropertiesListed,
        fileDeleted,
        artboardDeleted,
        viewModelDeleted,
        stateMachineDeleted,
        stateMachineSettled
    };

    friend class CommandServer;

    uint64_t m_currentFileHandleIdx = 0;
    uint64_t m_currentArtboardHandleIdx = 0;
    uint64_t m_currentViewModelHandleIdx = 0;
    uint64_t m_currentStateMachineHandleIdx = 0;
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
    std::unordered_map<ViewModelInstanceHandle, ViewModelInstanceListener*>
        m_viewModelListeners;
    std::unordered_map<StateMachineHandle, StateMachineListener*>
        m_stateMachineListeners;
};

}; // namespace rive

/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/object_stream.hpp"
#include "rive/refcnt.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
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

RIVE_DEFINE_HANDLE(FontHandle);
RIVE_DEFINE_HANDLE(FileHandle);
RIVE_DEFINE_HANDLE(ArtboardHandle);
RIVE_DEFINE_HANDLE(AudioSourceHandle);
RIVE_DEFINE_HANDLE(RenderImageHandle);
RIVE_DEFINE_HANDLE(StateMachineHandle);
RIVE_DEFINE_HANDLE(ViewModelInstanceHandle);
RIVE_DEFINE_HANDLE(DrawKey);

// Function poimter that gets called back from the server thread.
using CommandServerCallback = std::function<void(CommandServer*)>;
using CommandServerDrawCallback = std::function<void(DrawKey, CommandServer*)>;

struct ViewModelEnum
{
    std::string name;
    std::vector<std::string> enumerants;
};

struct NumberReader
{
    std::atomic<std::atomic<float>*> ptr{nullptr};

    float value() const
    {
        auto p = ptr.load(std::memory_order_acquire);
        return p ? p->load(std::memory_order_relaxed) : 0.0f;
    }
    bool isReady() const
    {
        return ptr.load(std::memory_order_acquire) != nullptr;
    }
};

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
        virtual void onFileError(const FileHandle,
                                 uint64_t requestId,
                                 std::string error)
        {}

        virtual void onFileDeleted(const FileHandle, uint64_t requestId) {}

        virtual void onFileLoaded(const FileHandle, uint64_t requestId) {}

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

        struct ViewModelPropertyData
        {
            DataType type = DataType::none;
            std::string name = "";
            std::string metaData = "";
        };

        virtual void onViewModelPropertiesListed(
            const FileHandle,
            uint64_t requestId,
            std::string viewModelName,
            std::vector<ViewModelPropertyData> properties)
        {}

        virtual void onViewModelEnumsListed(const FileHandle,
                                            uint64_t requestId,
                                            std::vector<ViewModelEnum> enums)
        {}
    };

    class RenderImageListener
        : public CommandQueue::ListenerBase<RenderImageListener,
                                            RenderImageHandle>
    {
    public:
        virtual void onRenderImageDecoded(const RenderImageHandle,
                                          uint64_t requestId)
        {}

        virtual void onRenderImageError(const RenderImageHandle,
                                        uint64_t requestId,
                                        std::string error)
        {}

        virtual void onRenderImageDeleted(const RenderImageHandle,
                                          uint64_t requestId)
        {}
    };

    class AudioSourceListener
        : public CommandQueue::ListenerBase<AudioSourceListener,
                                            AudioSourceHandle>
    {
    public:
        virtual void onAudioSourceDecoded(const AudioSourceHandle,
                                          uint64_t requestId)
        {}

        virtual void onAudioSourceError(const AudioSourceHandle,
                                        uint64_t requestId,
                                        std::string error)
        {}

        virtual void onAudioSourceDeleted(const AudioSourceHandle,
                                          uint64_t requestId)
        {}
    };

    class FontListener
        : public CommandQueue::ListenerBase<FontListener, FontHandle>
    {
    public:
        virtual void onFontDecoded(const FontHandle, uint64_t requestId) {}

        virtual void onFontError(const FontHandle,
                                 uint64_t requestId,
                                 std::string error)
        {}

        virtual void onFontDeleted(const FontHandle, uint64_t requestId) {}
    };

    class ArtboardListener
        : public CommandQueue::ListenerBase<ArtboardListener, ArtboardHandle>
    {
    public:
        virtual void onArtboardError(const ArtboardHandle,
                                     uint64_t requestId,
                                     std::string error)
        {}

        virtual void onDefaultViewModelInfoReceived(const ArtboardHandle,
                                                    uint64_t requestId,
                                                    std::string viewModelName,
                                                    std::string instanceName)
        {}

        virtual void onArtboardDeleted(const ArtboardHandle, uint64_t requestId)
        {}

        virtual void onStateMachinesListed(
            const ArtboardHandle,
            uint64_t requestId,
            std::vector<std::string> stateMachineNames)
        {}
    };

    struct ViewModelInstanceData
    {
        // The path and type of this property
        PropertyData metaData;

        // The specific value of this property, you must use the correct union
        // member base on type in metaData, both enumType and string use
        // stringValue

        // std::string is non trival, so it stays outside the union
        std::string stringValue;
        union
        {
            bool boolValue;
            float numberValue;
            ColorInt colorValue;
        };
    };

    class ViewModelInstanceListener
        : public CommandQueue::ListenerBase<ViewModelInstanceListener,
                                            ViewModelInstanceHandle>
    {
    public:
        virtual void onViewModelInstanceError(const ViewModelInstanceHandle,
                                              uint64_t requestId,
                                              std::string error)
        {}

        virtual void onViewModelDeleted(const ViewModelInstanceHandle,
                                        uint64_t requestId)
        {}

        virtual void onViewModelDataReceived(const ViewModelInstanceHandle,
                                             uint64_t requestId,
                                             ViewModelInstanceData)
        {}

        virtual void onViewModelListSizeReceived(const ViewModelInstanceHandle,
                                                 uint64_t requestId,
                                                 std::string path,
                                                 size_t size)
        {}

        virtual void onViewModelInstanceNameReceived(
            const ViewModelInstanceHandle,
            uint64_t requestId,
            std::string name)
        {}
    };

    class StateMachineListener
        : public CommandQueue::ListenerBase<StateMachineListener,
                                            StateMachineHandle>
    {
    public:
        virtual void onStateMachineError(const StateMachineHandle,
                                         uint64_t requestId,
                                         std::string error)
        {}

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
                        FileListener* listener = nullptr,
                        uint64_t requestId = 0);

    void deleteFile(FileHandle, uint64_t requestId = 0);

    void addGlobalImageAsset(std::string name,
                             RenderImageHandle,
                             uint64_t requestId = 0);
    void addGlobalFontAsset(std::string name,
                            FontHandle,
                            uint64_t requestId = 0);
    void addGlobalAudioAsset(std::string name,
                             AudioSourceHandle,
                             uint64_t requestId = 0);

    void removeGlobalImageAsset(std::string name, uint64_t requestId = 0);
    void removeGlobalFontAsset(std::string name, uint64_t requestId = 0);
    void removeGlobalAudioAsset(std::string name, uint64_t requestId = 0);

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

    void setArtboardSize(ArtboardHandle,
                         float width,
                         float height,
                         float scale = 1,
                         uint64_t requestId = 0);

    void resetArtboardSize(ArtboardHandle, uint64_t requestId = 0);

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

    ViewModelInstanceHandle referenceNestedViewModelInstance(
        ViewModelInstanceHandle,
        std::string path,
        ViewModelInstanceListener* listener = nullptr,
        uint64_t requestId = 0);

    ViewModelInstanceHandle referenceListViewModelInstance(
        ViewModelInstanceHandle,
        std::string path,
        int index,
        ViewModelInstanceListener* listener = nullptr,
        uint64_t requestId = 0);

    void fireViewModelTrigger(ViewModelInstanceHandle,
                              std::string path,
                              uint64_t requestId = 0);

    void setViewModelInstanceBool(ViewModelInstanceHandle,
                                  std::string path,
                                  bool value,
                                  uint64_t requestId = 0);
    void setViewModelInstanceNumber(ViewModelInstanceHandle,
                                    std::string path,
                                    float value,
                                    uint64_t requestId = 0);
    void setViewModelInstanceColor(ViewModelInstanceHandle,
                                   std::string path,
                                   ColorInt value,
                                   uint64_t requestId = 0);
    void setViewModelInstanceEnum(ViewModelInstanceHandle,
                                  std::string path,
                                  std::string value,
                                  uint64_t requestId = 0);
    void setViewModelInstanceString(ViewModelInstanceHandle,
                                    std::string path,
                                    std::string value,
                                    uint64_t requestId = 0);
    void setViewModelInstanceImage(ViewModelInstanceHandle,
                                   std::string path,
                                   RenderImageHandle value,
                                   uint64_t requestId = 0);
    void setViewModelInstanceArtboard(ViewModelInstanceHandle,
                                      std::string path,
                                      ArtboardHandle value,
                                      uint64_t requestId = 0);
    void setViewModelInstanceNestedViewModel(ViewModelInstanceHandle,
                                             std::string path,
                                             ViewModelInstanceHandle value,
                                             uint64_t requestId = 0);
    void insertViewModelInstanceListViewModel(ViewModelInstanceHandle,
                                              std::string path,
                                              ViewModelInstanceHandle value,
                                              int index,
                                              uint64_t requestId = 0);

    void appendViewModelInstanceListViewModel(ViewModelInstanceHandle handle,
                                              std::string path,
                                              ViewModelInstanceHandle value,
                                              uint64_t requestId = 0)
    {
        insertViewModelInstanceListViewModel(handle,
                                             std::move(path),
                                             value,
                                             -1,
                                             requestId);
    }

    void removeViewModelInstanceListViewModel(
        ViewModelInstanceHandle,
        std::string path,
        int index,
        ViewModelInstanceHandle value = RIVE_NULL_HANDLE,
        uint64_t requestId = 0);

    void removeViewModelInstanceListViewModel(ViewModelInstanceHandle handle,
                                              std::string path,
                                              ViewModelInstanceHandle value,
                                              uint64_t requestId = 0)
    {
        removeViewModelInstanceListViewModel(handle,
                                             std::move(path),
                                             -1,
                                             value,
                                             requestId);
    }

    void swapViewModelInstanceListValues(ViewModelInstanceHandle handle,
                                         std::string path,
                                         int indexa,
                                         int indexb,
                                         uint64_t requestId = 0);

    void subscribeToViewModelProperty(ViewModelInstanceHandle handle,
                                      std::string path,
                                      DataType type,
                                      uint64_t requestId = 0);

    void unsubscribeToViewModelProperty(ViewModelInstanceHandle handle,
                                        std::string path,
                                        DataType type,
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
    struct PointerEvent
    {
        Fit fit = Fit::none; // the fit the artboard is drawn with
        Alignment alignment; // the alignment the artboard is drawn with
        Vec2D screenBounds;  // the bounds of coordinate system of the cursor
        Vec2D position;      // the cursor position
        float scaleFactor = 1.0f; // scale factor for things like retina display
        int pointerId = 0;        // stable pointer identifier for multitouch
    };

    // All pointer events will automatically convert between artboard and screen
    // space for you based on the values passed in the PointerEvent struct.

    void pointerMove(StateMachineHandle, PointerEvent, uint64_t requestId = 0);
    void pointerDown(StateMachineHandle, PointerEvent, uint64_t requestId = 0);
    void pointerUp(StateMachineHandle, PointerEvent, uint64_t requestId = 0);
    void pointerExit(StateMachineHandle, PointerEvent, uint64_t requestId = 0);

    void deleteStateMachine(StateMachineHandle, uint64_t requestId = 0);

    RenderImageHandle decodeImage(std::vector<uint8_t> imageEncodedBytes,
                                  RenderImageListener* listener = nullptr,
                                  uint64_t requestId = 0);
    // This is needed for unreal rhi because all images come pre-decoded and
    // uploaded to gpu.
    RenderImageHandle addExternalImage(rcp<RenderImage> externalImage,
                                       RenderImageListener* listener = nullptr,
                                       uint64_t requestId = 0);

    void deleteImage(RenderImageHandle, uint64_t requestId = 0);

    AudioSourceHandle decodeAudio(std::vector<uint8_t> imageEncodedBytes,
                                  AudioSourceListener* listener = nullptr,
                                  uint64_t requestId = 0);

    AudioSourceHandle addExternalAudio(rcp<AudioSource> externalAudio,
                                       AudioSourceListener* listener = nullptr,
                                       uint64_t requestId = 0);

    void deleteAudio(AudioSourceHandle, uint64_t requestId = 0);

    FontHandle decodeFont(std::vector<uint8_t> imageEncodedBytes,
                          FontListener* listener = nullptr,
                          uint64_t requestId = 0);

    FontHandle addExternalFont(rcp<Font> externalFont,
                               FontListener* listener = nullptr,
                               uint64_t requestId = 0);

    void deleteFont(FontHandle, uint64_t requestId = 0);

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
    void requestViewModelEnums(FileHandle, uint64_t requestId = 0);
    void requestViewModelPropertyDefinitions(FileHandle,
                                             std::string viewModelName,
                                             uint64_t requestId = 0);

    void requestViewModelInstanceNames(FileHandle,
                                       std::string viewModelName,
                                       uint64_t requestId = 0);

    void requestViewModelInstanceBool(ViewModelInstanceHandle,
                                      std::string path,
                                      uint64_t requestId = 0);

    void requestViewModelInstanceNumber(ViewModelInstanceHandle,
                                        std::string path,
                                        uint64_t requestId = 0);

    void requestViewModelInstanceColor(ViewModelInstanceHandle,
                                       std::string path,
                                       uint64_t requestId = 0);

    void requestViewModelInstanceEnum(ViewModelInstanceHandle,
                                      std::string path,
                                      uint64_t requestId = 0);

    void requestViewModelInstanceString(ViewModelInstanceHandle,
                                        std::string path,
                                        uint64_t requestId = 0);

    void requestViewModelInstanceListSize(ViewModelInstanceHandle,
                                          std::string path,
                                          uint64_t requestId = 0);

    void requestViewModelInstanceName(ViewModelInstanceHandle,
                                      uint64_t requestId = 0);

    void requestStateMachineNames(ArtboardHandle, uint64_t requestId = 0);
    void requestDefaultViewModelInfo(ArtboardHandle,
                                     FileHandle,
                                     uint64_t requestId = 0);

    std::shared_ptr<NumberReader> createNumberReader(
        ViewModelInstanceHandle,
        std::string path,
        uint64_t requestId = 0);

    void releaseNumberReader(ViewModelInstanceHandle,
                             std::string path,
                             uint64_t requestId = 0);

    // Consume all messages received from the server.
    void processMessages();

    void setGlobalFileListener(FileListener* listener)
    {
        m_globalFileListener = listener;
    }
    void setGlobalArtboardListener(ArtboardListener* listener)
    {
        m_globalArtboardListener = listener;
    }
    void setGlobalStateMachineListener(StateMachineListener* listener)
    {
        m_globalStateMachineListener = listener;
    }
    void setGlobalViewModelInstanceListener(ViewModelInstanceListener* listener)
    {
        m_globalViewModelListener = listener;
    }
    void setGlobalRenderImageListener(RenderImageListener* listener)
    {
        m_globalImageListener = listener;
    }
    void setGlobalAudioSourceListener(AudioSourceListener* listener)
    {
        m_globalAudioListener = listener;
    }
    void setGlobalFontListener(FontListener* listener)
    {
        m_globalFontListener = listener;
    }

    void setMessageAvailableCallback(std::function<void()> callback)
    {
        m_messageAvailableCallback = std::move(callback);
    }

private:
    void registerListener(FileHandle handle, FileListener* listener)
    {
        assert(listener);
        assert(m_fileListeners.find(handle) == m_fileListeners.end());
        m_fileListeners.insert({handle, listener});
    }

    void registerListener(RenderImageHandle handle,
                          RenderImageListener* listener)
    {
        assert(listener);
        assert(m_imageListeners.find(handle) == m_imageListeners.end());
        m_imageListeners.insert({handle, listener});
    }

    void registerListener(AudioSourceHandle handle,
                          AudioSourceListener* listener)
    {
        assert(listener);
        assert(m_audioListeners.find(handle) == m_audioListeners.end());
        m_audioListeners.insert({handle, listener});
    }

    void registerListener(FontHandle handle, FontListener* listener)
    {
        assert(listener);
        assert(m_fontListeners.find(handle) == m_fontListeners.end());
        m_fontListeners.insert({handle, listener});
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

    void unregisterListener(RenderImageHandle handle,
                            RenderImageListener* listener)
    {
        m_imageListeners.erase(handle);
    }

    void unregisterListener(AudioSourceHandle handle,
                            AudioSourceListener* listener)
    {
        m_audioListeners.erase(handle);
    }

    void unregisterListener(FontHandle handle, FontListener* listener)
    {
        m_fontListeners.erase(handle);
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
        decodeImage,
        externalImage,
        decodeAudio,
        externalAudio,
        decodeFont,
        externalFont,
        deleteImage,
        deleteAudio,
        deleteFont,
        addImageFileAsset,
        addAudioFileAsset,
        addFontFileAsset,
        removeImageFileAsset,
        removeAudioFileAsset,
        removeFontFileAsset,
        instantiateArtboard,
        deleteArtboard,
        setArtboardSize,
        resetArtboardSize,
        instantiateViewModel,
        refNestedViewModel,
        refListViewModel,
        instantiateBlankViewModel,
        instantiateViewModelForArtboard,
        instantiateBlankViewModelForArtboard,
        setViewModelInstanceValue,
        addViewModelListValue,
        removeViewModelListValue,
        swapViewModelListValue,
        subscribeViewModelProperty,
        unsubscribeViewModelProperty,
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
        listViewModelEnums,
        listArtboards,
        listStateMachines,
        getDefaultViewModel,
        listViewModels,
        listViewModelInstanceNames,
        listViewModelProperties,
        listViewModelPropertyValue,
        getViewModelListSize,
        getViewModelInstanceName,
        createNumberReader,
        releaseNumberReader
    };

    enum class Message
    {
        // Same as commandLoopBreak for processMessages
        messageLoopBreak,
        viewModelEnumsListed,
        artboardsListed,
        stateMachinesListed,
        defaultViewModelReceived,
        viewModelsListend,
        viewModelInstanceNamesListed,
        viewModelPropertiesListed,
        viewModelPropertyValueReceived,
        viewModelListSizeReceived,
        viewModelInstanceNameReceived,
        fileLoaded,
        fileDeleted,
        imageDecoded,
        imageDeleted,
        audioDecoded,
        audioDeleted,
        fontDecoded,
        fontDeleted,
        artboardDeleted,
        viewModelDeleted,
        stateMachineDeleted,
        stateMachineSettled,
        fileError,
        artboardError,
        viewModelError,
        imageError,
        audioError,
        fontError,
        stateMachineError
    };

    friend class CommandServer;

    uint64_t m_currentFileHandleIdx = 0;
    uint64_t m_currentListHandleIdx = 0;
    uint64_t m_currentFontHandleIdx = 0;
    uint64_t m_currentArtboardHandleIdx = 0;
    uint64_t m_currentViewModelHandleIdx = 0;
    uint64_t m_currentRenderImageHandleIdx = 0;
    uint64_t m_currentAudioSourceHandleIdx = 0;
    uint64_t m_currentStateMachineHandleIdx = 0;
    uint64_t m_currentDrawKeyIdx = 0;

    std::mutex m_commandMutex;
    std::condition_variable m_commandConditionVariable;
    PODStream m_commandStream;
    ObjectStream<rcp<RenderImage>> m_externalImages;
    ObjectStream<rcp<AudioSource>> m_externalAudioSources;
    ObjectStream<rcp<Font>> m_externalFonts;
    ObjectStream<std::vector<uint8_t>> m_byteVectors;
    ObjectStream<PointerEvent> m_pointerEvents;
    ObjectStream<std::string> m_names;
    ObjectStream<CommandServerCallback> m_callbacks;
    ObjectStream<CommandServerDrawCallback> m_drawCallbacks;
    ObjectStream<std::shared_ptr<NumberReader>> m_numberReaderPtrs;

    // Messages streams
    std::mutex m_messageMutex;
    PODStream m_messageStream;
    ObjectStream<std::string> m_messageNames;

    // Listeners
    FileListener* m_globalFileListener = nullptr;
    RenderImageListener* m_globalImageListener = nullptr;
    AudioSourceListener* m_globalAudioListener = nullptr;
    FontListener* m_globalFontListener = nullptr;
    ArtboardListener* m_globalArtboardListener = nullptr;
    ViewModelInstanceListener* m_globalViewModelListener = nullptr;
    StateMachineListener* m_globalStateMachineListener = nullptr;

    std::unordered_map<FileHandle, FileListener*> m_fileListeners;
    std::unordered_map<RenderImageHandle, RenderImageListener*>
        m_imageListeners;
    std::unordered_map<AudioSourceHandle, AudioSourceListener*>
        m_audioListeners;
    std::unordered_map<FontHandle, FontListener*> m_fontListeners;
    std::unordered_map<ArtboardHandle, ArtboardListener*> m_artboardListeners;
    std::unordered_map<ViewModelInstanceHandle, ViewModelInstanceListener*>
        m_viewModelListeners;
    std::unordered_map<StateMachineHandle, StateMachineListener*>
        m_stateMachineListeners;

    std::function<void()> m_messageAvailableCallback;
};

}; // namespace rive

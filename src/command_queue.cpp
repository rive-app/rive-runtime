/*
 * Copyright 2025 Rive
 */

#include "rive/command_queue.hpp"

namespace rive
{
namespace
{
// RAII utility to lock a mutex, and call notify_one() on a condition variable
// immediately before unlocking.
class AutoLockAndNotify
{
public:
    AutoLockAndNotify(std::mutex& mutex, std::condition_variable& cv) :
        m_mutex(mutex), m_cv(cv)
    {
        m_mutex.lock();
    }

    ~AutoLockAndNotify()
    {
        m_cv.notify_one();
        m_mutex.unlock();
    }

private:
    std::mutex& m_mutex;
    std::condition_variable& m_cv;
};
}; // namespace

CommandQueue::CommandQueue() {}

CommandQueue::~CommandQueue() {}

FileHandle CommandQueue::loadFile(std::vector<uint8_t> rivBytes,
                                  FileListener* listener,
                                  uint64_t requestId)
{
    auto handle = reinterpret_cast<FileHandle>(++m_currentFileHandleIdx);

    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = handle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(handle, listener);
    }

    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::loadFile;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_byteVectors << std::move(rivBytes);

    return handle;
}

void CommandQueue::deleteFile(FileHandle fileHandle, uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::deleteFile;
    m_commandStream << fileHandle;
    m_commandStream << requestId;
}

void CommandQueue::addGlobalImageAsset(std::string name,
                                       RenderImageHandle handle,
                                       uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::addImageFileAsset;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << name;
}

void CommandQueue::addGlobalFontAsset(std::string name,
                                      FontHandle handle,
                                      uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::addFontFileAsset;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << name;
}

void CommandQueue::addGlobalAudioAsset(std::string name,
                                       AudioSourceHandle handle,
                                       uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::addAudioFileAsset;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << name;
}

void CommandQueue::removeGlobalImageAsset(std::string name, uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::removeImageFileAsset;
    m_commandStream << requestId;
    m_names << name;
}

void CommandQueue::removeGlobalFontAsset(std::string name, uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::removeFontFileAsset;
    m_commandStream << requestId;
    m_names << name;
}

void CommandQueue::removeGlobalAudioAsset(std::string name, uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::removeAudioFileAsset;
    m_commandStream << requestId;
    m_names << name;
}

ArtboardHandle CommandQueue::instantiateArtboardNamed(
    FileHandle fileHandle,
    std::string name,
    ArtboardListener* listener,
    uint64_t requestId)
{
    auto handle =
        reinterpret_cast<ArtboardHandle>(++m_currentArtboardHandleIdx);

    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = handle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(handle, listener);
    }

    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::instantiateArtboard;
    m_commandStream << handle;
    m_commandStream << fileHandle;
    m_commandStream << requestId;
    m_names << std::move(name);

    return handle;
}

void CommandQueue::setArtboardSize(ArtboardHandle artboardHandle,
                                   float width,
                                   float height,
                                   float scale,
                                   uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::setArtboardSize;
    m_commandStream << artboardHandle;
    m_commandStream << width / scale;
    m_commandStream << height / scale;
    m_commandStream << requestId;
}

void CommandQueue::resetArtboardSize(ArtboardHandle artboardHandle,
                                     uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::resetArtboardSize;
    m_commandStream << artboardHandle;
    m_commandStream << requestId;
}

void CommandQueue::deleteArtboard(ArtboardHandle artboardHandle,
                                  uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::deleteArtboard;
    m_commandStream << artboardHandle;
    m_commandStream << requestId;
}

ViewModelInstanceHandle CommandQueue::instantiateBlankViewModelInstance(
    FileHandle fileHandle,
    ArtboardHandle artboardHandle,
    ViewModelInstanceListener* listener,
    uint64_t requestId)
{
    auto viewHandle = reinterpret_cast<ViewModelInstanceHandle>(
        ++m_currentViewModelHandleIdx);
    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = viewHandle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(viewHandle, listener);
    }
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::instantiateBlankViewModelForArtboard;
    m_commandStream << fileHandle;
    m_commandStream << artboardHandle;
    m_commandStream << viewHandle;
    m_commandStream << requestId;

    return viewHandle;
}

ViewModelInstanceHandle CommandQueue::instantiateBlankViewModelInstance(
    FileHandle fileHandle,
    std::string viewModelName,
    ViewModelInstanceListener* listener,
    uint64_t requestId)
{
    auto viewHandle = reinterpret_cast<ViewModelInstanceHandle>(
        ++m_currentViewModelHandleIdx);
    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = viewHandle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(viewHandle, listener);
    }
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::instantiateBlankViewModel;
    m_commandStream << fileHandle;
    m_commandStream << viewHandle;
    m_commandStream << requestId;
    m_names << viewModelName;

    return viewHandle;
}

ViewModelInstanceHandle CommandQueue::instantiateViewModelInstanceNamed(
    FileHandle fileHandle,
    ArtboardHandle artboardHandle,
    std::string viewModelInstanceName,
    ViewModelInstanceListener* listener,
    uint64_t requestId)
{
    auto viewHandle = reinterpret_cast<ViewModelInstanceHandle>(
        ++m_currentViewModelHandleIdx);
    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = viewHandle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(viewHandle, listener);
    }
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::instantiateViewModelForArtboard;
    m_commandStream << fileHandle;
    m_commandStream << artboardHandle;
    m_commandStream << viewHandle;
    m_commandStream << requestId;
    m_names << viewModelInstanceName;

    return viewHandle;
}

ViewModelInstanceHandle CommandQueue::instantiateViewModelInstanceNamed(
    FileHandle fileHandle,
    std::string viewModelName,
    std::string viewModelInstanceName,
    ViewModelInstanceListener* listener,
    uint64_t requestId)
{
    auto viewHandle = reinterpret_cast<ViewModelInstanceHandle>(
        ++m_currentViewModelHandleIdx);
    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = viewHandle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(viewHandle, listener);
    }
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::instantiateViewModel;
    m_commandStream << fileHandle;
    m_commandStream << viewHandle;
    m_commandStream << requestId;
    m_names << viewModelName;
    m_names << viewModelInstanceName;

    return viewHandle;
}

ViewModelInstanceHandle CommandQueue::referenceNestedViewModelInstance(
    ViewModelInstanceHandle handle,
    std::string path,
    ViewModelInstanceListener* listener,
    uint64_t requestId)
{
    auto viewHandle = reinterpret_cast<ViewModelInstanceHandle>(
        ++m_currentViewModelHandleIdx);
    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = viewHandle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(viewHandle, listener);
    }
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::refNestedViewModel;
    m_commandStream << handle;
    m_commandStream << viewHandle;
    m_commandStream << requestId;
    m_names << path;

    return viewHandle;
}

ViewModelInstanceHandle CommandQueue::referenceListViewModelInstance(
    ViewModelInstanceHandle handle,
    std::string path,
    int index,
    ViewModelInstanceListener* listener,
    uint64_t requestId)
{
    auto viewHandle = reinterpret_cast<ViewModelInstanceHandle>(
        ++m_currentViewModelHandleIdx);
    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = handle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(viewHandle, listener);
    }
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::refListViewModel;
    m_commandStream << handle;
    m_commandStream << index;
    m_commandStream << viewHandle;
    m_commandStream << requestId;
    m_names << path;

    return viewHandle;
}

void CommandQueue::fireViewModelTrigger(ViewModelInstanceHandle handle,
                                        std::string path,
                                        uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::setViewModelInstanceValue;
    m_commandStream << handle;
    m_commandStream << DataType::trigger;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::setViewModelInstanceBool(ViewModelInstanceHandle handle,
                                            std::string path,
                                            bool value,
                                            uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::setViewModelInstanceValue;
    m_commandStream << handle;
    m_commandStream << DataType::boolean;
    m_commandStream << requestId;
    m_commandStream << value;
    m_names << path;
}

void CommandQueue::setViewModelInstanceNumber(ViewModelInstanceHandle handle,
                                              std::string path,
                                              float value,
                                              uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::setViewModelInstanceValue;
    m_commandStream << handle;
    m_commandStream << DataType::number;
    m_commandStream << requestId;
    m_commandStream << value;
    m_names << path;
}

void CommandQueue::setViewModelInstanceColor(ViewModelInstanceHandle handle,
                                             std::string path,
                                             ColorInt value,
                                             uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::setViewModelInstanceValue;
    m_commandStream << handle;
    m_commandStream << DataType::color;
    m_commandStream << requestId;
    m_commandStream << value;
    m_names << path;
}

void CommandQueue::setViewModelInstanceEnum(ViewModelInstanceHandle handle,
                                            std::string path,
                                            std::string value,
                                            uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::setViewModelInstanceValue;
    m_commandStream << handle;
    m_commandStream << DataType::enumType;
    m_commandStream << requestId;
    m_names << path;
    m_names << value;
}

void CommandQueue::setViewModelInstanceString(ViewModelInstanceHandle handle,
                                              std::string path,
                                              std::string value,
                                              uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::setViewModelInstanceValue;
    m_commandStream << handle;
    m_commandStream << DataType::string;
    m_commandStream << requestId;
    m_names << path;
    m_names << value;
}

void CommandQueue::setViewModelInstanceImage(ViewModelInstanceHandle handle,
                                             std::string path,
                                             RenderImageHandle value,
                                             uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::setViewModelInstanceValue;
    m_commandStream << handle;
    m_commandStream << DataType::assetImage;
    m_commandStream << requestId;
    m_commandStream << value;
    m_names << path;
}

void CommandQueue::setViewModelInstanceArtboard(ViewModelInstanceHandle handle,
                                                std::string path,
                                                ArtboardHandle value,
                                                uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::setViewModelInstanceValue;
    m_commandStream << handle;
    m_commandStream << DataType::artboard;
    m_commandStream << requestId;
    m_commandStream << value;
    m_names << path;
}

void CommandQueue::setViewModelInstanceNestedViewModel(
    ViewModelInstanceHandle handle,
    std::string path,
    ViewModelInstanceHandle value,
    uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::setViewModelInstanceValue;
    m_commandStream << handle;
    m_commandStream << DataType::viewModel;
    m_commandStream << requestId;
    m_commandStream << value;
    m_names << path;
}

void CommandQueue::insertViewModelInstanceListViewModel(
    ViewModelInstanceHandle handle,
    std::string path,
    ViewModelInstanceHandle value,
    int index,
    uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::addViewModelListValue;
    m_commandStream << handle;
    m_commandStream << value;
    m_commandStream << index;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::removeViewModelInstanceListViewModel(
    ViewModelInstanceHandle handle,
    std::string path,
    int index,
    ViewModelInstanceHandle value,
    uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::removeViewModelListValue;
    m_commandStream << handle;
    m_commandStream << value;
    m_commandStream << index;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::swapViewModelInstanceListValues(
    ViewModelInstanceHandle handle,
    std::string path,
    int indexa,
    int indexb,
    uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::swapViewModelListValue;
    m_commandStream << handle;
    m_commandStream << indexa;
    m_commandStream << indexb;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::subscribeToViewModelProperty(ViewModelInstanceHandle handle,
                                                std::string path,
                                                DataType type,
                                                uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::subscribeViewModelProperty;
    m_commandStream << handle;
    m_commandStream << type;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::unsubscribeToViewModelProperty(
    ViewModelInstanceHandle handle,
    std::string path,
    DataType type,
    uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::unsubscribeViewModelProperty;
    m_commandStream << handle;
    m_commandStream << type;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::deleteViewModelInstance(ViewModelInstanceHandle handle,
                                           uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::deleteViewModel;
    m_commandStream << handle;
    m_commandStream << requestId;
}

StateMachineHandle CommandQueue::instantiateStateMachineNamed(
    ArtboardHandle artboardHandle,
    std::string name,
    StateMachineListener* listener,
    uint64_t requestId)
{
    auto handle =
        reinterpret_cast<StateMachineHandle>(++m_currentStateMachineHandleIdx);

    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = handle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(handle, listener);
    }

    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::instantiateStateMachine;
    m_commandStream << handle;
    m_commandStream << artboardHandle;
    m_commandStream << requestId;
    m_names << std::move(name);

    return handle;
}

void CommandQueue::pointerMove(StateMachineHandle stateMachineHandle,
                               PointerEvent pointerEvent,
                               uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::pointerMove;
    m_commandStream << stateMachineHandle;
    m_commandStream << requestId;
    m_pointerEvents << std::move(pointerEvent);
}

void CommandQueue::pointerDown(StateMachineHandle stateMachineHandle,
                               PointerEvent pointerEvent,
                               uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::pointerDown;
    m_commandStream << stateMachineHandle;
    m_commandStream << requestId;
    m_pointerEvents << std::move(pointerEvent);
}

void CommandQueue::pointerUp(StateMachineHandle stateMachineHandle,
                             PointerEvent pointerEvent,
                             uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::pointerUp;
    m_commandStream << stateMachineHandle;
    m_commandStream << requestId;
    m_pointerEvents << std::move(pointerEvent);
}

void CommandQueue::pointerExit(StateMachineHandle stateMachineHandle,
                               PointerEvent pointerEvent,
                               uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::pointerExit;
    m_commandStream << stateMachineHandle;
    m_commandStream << requestId;
    m_pointerEvents << std::move(pointerEvent);
}

void CommandQueue::bindViewModelInstance(StateMachineHandle handle,
                                         ViewModelInstanceHandle viewModel,
                                         uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::bindViewModelInstance;
    m_commandStream << handle;
    m_commandStream << viewModel;
    m_commandStream << requestId;
}

void CommandQueue::advanceStateMachine(StateMachineHandle stateMachineHandle,
                                       float timeToAdvance,
                                       uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::advanceStateMachine;
    m_commandStream << stateMachineHandle;
    m_commandStream << requestId;
    m_commandStream << timeToAdvance;
}

void CommandQueue::deleteStateMachine(StateMachineHandle stateMachineHandle,
                                      uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::deleteStateMachine;
    m_commandStream << stateMachineHandle;
    m_commandStream << requestId;
}

RenderImageHandle CommandQueue::decodeImage(
    std::vector<uint8_t> imageEncodedBytes,
    RenderImageListener* listener,
    uint64_t requestId)
{
    auto handle =
        reinterpret_cast<RenderImageHandle>(++m_currentRenderImageHandleIdx);

    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = handle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(handle, listener);
    }

    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::decodeImage;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_byteVectors << std::move(imageEncodedBytes);
    return handle;
}

RenderImageHandle CommandQueue::addExternalImage(rcp<RenderImage> externalImage,
                                                 RenderImageListener* listener,
                                                 uint64_t requestId)
{
    auto handle =
        reinterpret_cast<RenderImageHandle>(++m_currentRenderImageHandleIdx);

    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = handle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(handle, listener);
    }

    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::externalImage;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_externalImages << std::move(externalImage);
    return handle;
}

void CommandQueue::deleteImage(RenderImageHandle handle, uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::deleteImage;
    m_commandStream << handle;
    m_commandStream << requestId;
}

AudioSourceHandle CommandQueue::decodeAudio(
    std::vector<uint8_t> imageEncodedBytes,
    AudioSourceListener* listener,
    uint64_t requestId)
{
    auto handle =
        reinterpret_cast<AudioSourceHandle>(++m_currentAudioSourceHandleIdx);

    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = handle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(handle, listener);
    }

    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::decodeAudio;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_byteVectors << std::move(imageEncodedBytes);
    return handle;
}

AudioSourceHandle CommandQueue::addExternalAudio(rcp<AudioSource> externalAudio,
                                                 AudioSourceListener* listener,
                                                 uint64_t requestId)
{
    auto handle =
        reinterpret_cast<AudioSourceHandle>(++m_currentAudioSourceHandleIdx);

    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = handle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(handle, listener);
    }

    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::externalAudio;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_externalAudioSources << std::move(externalAudio);
    return handle;
}

void CommandQueue::deleteAudio(AudioSourceHandle handle, uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::deleteAudio;
    m_commandStream << handle;
    m_commandStream << requestId;
}

FontHandle CommandQueue::decodeFont(std::vector<uint8_t> imageEncodedBytes,
                                    FontListener* listener,
                                    uint64_t requestId)
{
    auto handle = reinterpret_cast<FontHandle>(++m_currentFontHandleIdx);

    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = handle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(handle, listener);
    }

    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::decodeFont;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_byteVectors << std::move(imageEncodedBytes);
    return handle;
}

FontHandle CommandQueue::addExternalFont(rcp<Font> externalFont,
                                         FontListener* listener,
                                         uint64_t requestId)
{
    auto handle = reinterpret_cast<FontHandle>(++m_currentFontHandleIdx);

    if (listener)
    {
        assert(listener->m_handle == RIVE_NULL_HANDLE);
        listener->m_handle = handle;
        listener->m_owningQueue = ref_rcp(this);
        registerListener(handle, listener);
    }

    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::externalFont;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_externalFonts << std::move(externalFont);
    return handle;
}

void CommandQueue::deleteFont(FontHandle handle, uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::deleteFont;
    m_commandStream << handle;
    m_commandStream << requestId;
}

DrawKey CommandQueue::createDrawKey()
{
    // lock here so we can do this from several threads safely
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    auto key = reinterpret_cast<DrawKey>(++m_currentDrawKeyIdx);
    return key;
}

void CommandQueue::draw(DrawKey drawKey, CommandServerDrawCallback callback)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::draw;
    m_commandStream << drawKey;
    m_drawCallbacks << std::move(callback);
}
#ifdef TESTING
void CommandQueue::testing_commandLoopBreak()
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::commandLoopBreak;
}

CommandQueue::FileListener* CommandQueue::testing_getFileListener(
    FileHandle handle)
{
    auto iter = m_fileListeners.find(handle);
    if (iter != m_fileListeners.end())
        return iter->second;
    return nullptr;
}

CommandQueue::ArtboardListener* CommandQueue::testing_getArtboardListener(
    ArtboardHandle handle)
{
    auto iter = m_artboardListeners.find(handle);
    if (iter != m_artboardListeners.end())
        return iter->second;
    return nullptr;
}

CommandQueue::StateMachineListener* CommandQueue::
    testing_getStateMachineListener(StateMachineHandle handle)
{
    auto iter = m_stateMachineListeners.find(handle);
    if (iter != m_stateMachineListeners.end())
        return iter->second;
    return nullptr;
}

#endif
void CommandQueue::runOnce(CommandServerCallback callback)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::runOnce;
    m_callbacks << std::move(callback);
}

void CommandQueue::disconnect()
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::disconnect;
}

void CommandQueue::requestViewModelNames(FileHandle fileHandle,
                                         uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listViewModels;
    m_commandStream << fileHandle;
    m_commandStream << requestId;
}

void CommandQueue::requestArtboardNames(FileHandle fileHandle,
                                        uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listArtboards;
    m_commandStream << fileHandle;
    m_commandStream << requestId;
}

void CommandQueue::requestViewModelEnums(FileHandle fileHandle,
                                         uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listViewModelEnums;
    m_commandStream << fileHandle;
    m_commandStream << requestId;
}

void CommandQueue::requestViewModelPropertyDefinitions(
    FileHandle handle,
    std::string viewModelName,
    uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listViewModelProperties;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << viewModelName;
}

void CommandQueue::requestViewModelInstanceNames(FileHandle handle,
                                                 std::string viewModelName,
                                                 uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listViewModelInstanceNames;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << viewModelName;
}

void CommandQueue::requestViewModelInstanceBool(ViewModelInstanceHandle handle,
                                                std::string path,
                                                uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listViewModelPropertyValue;
    m_commandStream << DataType::boolean;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::requestViewModelInstanceNumber(
    ViewModelInstanceHandle handle,
    std::string path,
    uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listViewModelPropertyValue;
    m_commandStream << DataType::number;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::requestViewModelInstanceColor(ViewModelInstanceHandle handle,
                                                 std::string path,
                                                 uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listViewModelPropertyValue;
    m_commandStream << DataType::color;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::requestViewModelInstanceEnum(ViewModelInstanceHandle handle,
                                                std::string path,
                                                uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listViewModelPropertyValue;
    m_commandStream << DataType::enumType;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::requestViewModelInstanceString(
    ViewModelInstanceHandle handle,
    std::string path,
    uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listViewModelPropertyValue;
    m_commandStream << DataType::string;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::requestViewModelInstanceListSize(
    ViewModelInstanceHandle handle,
    std::string path,
    uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::getViewModelListSize;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::requestViewModelInstanceName(
    ViewModelInstanceHandle handle,
    uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::getViewModelInstanceName;
    m_commandStream << handle;
    m_commandStream << requestId;
}

void CommandQueue::requestStateMachineNames(ArtboardHandle artboardHandle,
                                            uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listStateMachines;
    m_commandStream << artboardHandle;
    m_commandStream << requestId;
}

void CommandQueue::requestDefaultViewModelInfo(ArtboardHandle artboardHandle,
                                               FileHandle fileHandle,
                                               uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::getDefaultViewModel;
    m_commandStream << fileHandle;
    m_commandStream << artboardHandle;
    m_commandStream << requestId;
}

std::shared_ptr<NumberReader> CommandQueue::createNumberReader(
    ViewModelInstanceHandle handle,
    std::string path,
    uint64_t requestId)
{
    auto reader = std::make_shared<NumberReader>();
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::createNumberReader;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << std::move(path);
    m_numberReaderPtrs << reader;
    return reader;
}

void CommandQueue::releaseNumberReader(ViewModelInstanceHandle handle,
                                       std::string path,
                                       uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::releaseNumberReader;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << std::move(path);
}

void CommandQueue::processMessages()
{
    std::unique_lock<std::mutex> lock(m_messageMutex);

    if (m_messageStream.empty())
        return;

    // Mark where we will end processing messages. This way if new messages come
    // in while we're processing the existing ones, we won't loop forever.
    m_messageStream << Message::messageLoopBreak;

    do
    {
        Message message;
        m_messageStream >> message;

        switch (message)
        {
            case Message::messageLoopBreak:
                lock.unlock();
                return;
            case Message::viewModelEnumsListed:
            {
                size_t numEnums;
                FileHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageStream >> numEnums;
                std::vector<ViewModelEnum> enums(numEnums);
                for (auto& enumData : enums)
                {
                    size_t numEnumDataValue;
                    m_messageStream >> numEnumDataValue;
                    m_messageNames >> enumData.name;
                    enumData.enumerants.resize(numEnumDataValue);
                    for (auto& enumDataValue : enumData.enumerants)
                    {
                        m_messageNames >> enumDataValue;
                    }
                }
                lock.unlock();

                if (m_globalFileListener)
                {
                    m_globalFileListener->onViewModelEnumsListed(
                        handle,
                        requestId,
                        std::move(enums));
                }

                auto itr = m_fileListeners.find(handle);
                if (itr != m_fileListeners.end())
                {
                    itr->second->onViewModelEnumsListed(itr->first,
                                                        requestId,
                                                        std::move(enums));
                }
                break;
            }
            case Message::artboardsListed:
            {
                size_t numArtboards;
                FileHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageStream >> numArtboards;
                std::vector<std::string> artboardNames(numArtboards);
                for (auto& name : artboardNames)
                {
                    m_messageNames >> name;
                }
                lock.unlock();

                if (m_globalFileListener)
                {
                    m_globalFileListener->onArtboardsListed(handle,
                                                            requestId,
                                                            artboardNames);
                }

                auto itr = m_fileListeners.find(handle);
                if (itr != m_fileListeners.end())
                {
                    itr->second->onArtboardsListed(itr->first,
                                                   requestId,
                                                   std::move(artboardNames));
                }
                break;
            }
            case Message::stateMachinesListed:
            {
                size_t numStateMachines;
                ArtboardHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageStream >> numStateMachines;
                std::vector<std::string> stateMachineNames(numStateMachines);
                for (auto& name : stateMachineNames)
                {
                    m_messageNames >> name;
                }
                lock.unlock();

                if (m_globalArtboardListener)
                {
                    m_globalArtboardListener->onStateMachinesListed(
                        handle,
                        requestId,
                        stateMachineNames);
                }

                auto itr = m_artboardListeners.find(handle);
                if (itr != m_artboardListeners.end())
                {
                    itr->second->onStateMachinesListed(
                        itr->first,
                        requestId,
                        std::move(stateMachineNames));
                }

                break;
            }
            case Message::defaultViewModelReceived:
            {
                ArtboardHandle handle;
                uint64_t requestId;
                std::string defaultViewModel;
                std::string defaultViewModelInstance;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageNames >> defaultViewModel;
                m_messageNames >> defaultViewModelInstance;
                lock.unlock();

                if (m_globalArtboardListener)
                {
                    m_globalArtboardListener->onDefaultViewModelInfoReceived(
                        handle,
                        requestId,
                        defaultViewModel,
                        defaultViewModelInstance);
                }

                auto itr = m_artboardListeners.find(handle);
                if (itr != m_artboardListeners.end())
                {
                    itr->second->onDefaultViewModelInfoReceived(
                        itr->first,
                        requestId,
                        std::move(defaultViewModel),
                        std::move(defaultViewModelInstance));
                }

                break;
            }
            case Message::viewModelsListend:
            {
                size_t numViewModels;
                FileHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageStream >> numViewModels;
                std::vector<std::string> viewModelNames(numViewModels);
                for (auto& name : viewModelNames)
                {
                    m_messageNames >> name;
                }
                lock.unlock();

                if (m_globalFileListener)
                {
                    m_globalFileListener->onViewModelsListed(handle,
                                                             requestId,
                                                             viewModelNames);
                }

                auto itr = m_fileListeners.find(handle);
                if (itr != m_fileListeners.end())
                {
                    itr->second->onViewModelsListed(itr->first,
                                                    requestId,
                                                    std::move(viewModelNames));
                }

                break;
            }

            case Message::viewModelInstanceNamesListed:
            {
                size_t numViewModels;
                FileHandle handle;
                uint64_t requestId;
                std::string viewModelName;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageStream >> numViewModels;
                m_messageNames >> viewModelName;
                std::vector<std::string> viewModelInstanceNames(numViewModels);
                for (auto& name : viewModelInstanceNames)
                {
                    m_messageNames >> name;
                }
                lock.unlock();

                if (m_globalFileListener)
                {
                    m_globalFileListener->onViewModelInstanceNamesListed(
                        handle,
                        requestId,
                        viewModelName,
                        viewModelInstanceNames);
                }

                auto itr = m_fileListeners.find(handle);
                if (itr != m_fileListeners.end())
                {
                    itr->second->onViewModelInstanceNamesListed(
                        itr->first,
                        requestId,
                        std::move(viewModelName),
                        std::move(viewModelInstanceNames));
                }
                break;
            }

            case Message::viewModelPropertiesListed:
            {
                size_t numViewProperties;
                FileHandle handle;
                uint64_t requestId;
                std::string viewModelName;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageStream >> numViewProperties;
                m_messageNames >> viewModelName;
                std::vector<CommandQueue::FileListener::ViewModelPropertyData>
                    viewModelProperties(numViewProperties);

                for (auto& property : viewModelProperties)
                {
                    m_messageStream >> property.type;
                    m_messageNames >> property.name;
                    if (property.type == DataType::enumType ||
                        property.type == DataType::viewModel)
                    {
                        m_messageNames >> property.metaData;
                    }
                }
                lock.unlock();

                if (m_globalFileListener)
                {
                    m_globalFileListener->onViewModelPropertiesListed(
                        handle,
                        requestId,
                        viewModelName,
                        viewModelProperties);
                }

                auto itr = m_fileListeners.find(handle);
                if (itr != m_fileListeners.end())
                {
                    itr->second->onViewModelPropertiesListed(
                        itr->first,
                        requestId,
                        std::move(viewModelName),
                        std::move(viewModelProperties));
                }

                break;
            }

            case Message::viewModelPropertyValueReceived:
            {
                ViewModelInstanceHandle handle;
                uint64_t requestId;
                ViewModelInstanceData value;

                m_messageStream >> handle;
                m_messageStream >> value.metaData.type;
                m_messageStream >> requestId;
                m_messageNames >> value.metaData.name;

                switch (value.metaData.type)
                {
                    case DataType::assetImage:
                    case DataType::list:
                    case DataType::trigger:
                        break;
                    case DataType::boolean:
                        m_messageStream >> value.boolValue;
                        break;
                    case DataType::number:
                        m_messageStream >> value.numberValue;
                        break;
                    case DataType::color:
                        m_messageStream >> value.colorValue;
                        break;
                    case DataType::enumType:
                    case DataType::string:
                        m_messageNames >> value.stringValue;
                        break;
                    default:
                        RIVE_UNREACHABLE();
                }

                lock.unlock();

                if (m_globalViewModelListener)
                {
                    m_globalViewModelListener->onViewModelDataReceived(
                        handle,
                        requestId,
                        value);
                }

                auto itr = m_viewModelListeners.find(handle);
                if (itr != m_viewModelListeners.end())
                {
                    itr->second->onViewModelDataReceived(handle,
                                                         requestId,
                                                         value);
                }

                break;
            }

            case Message::viewModelListSizeReceived:
            {
                ViewModelInstanceHandle handle;
                std::string path;
                size_t size;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> size;
                m_messageStream >> requestId;
                m_messageNames >> path;
                lock.unlock();
                if (m_globalViewModelListener)
                {
                    m_globalViewModelListener->onViewModelListSizeReceived(
                        handle,
                        requestId,
                        path,
                        size);
                }
                auto itr = m_viewModelListeners.find(handle);
                if (itr != m_viewModelListeners.end())
                {
                    itr->second->onViewModelListSizeReceived(handle,
                                                             requestId,
                                                             std::move(path),
                                                             size);
                }
                break;
            }

            case Message::viewModelInstanceNameReceived:
            {
                ViewModelInstanceHandle handle;
                std::string name;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageNames >> name;
                lock.unlock();
                if (m_globalViewModelListener)
                {
                    m_globalViewModelListener
                        ->onViewModelInstanceNameReceived(handle,
                                                         requestId,
                                                         name);
                }
                auto itr = m_viewModelListeners.find(handle);
                if (itr != m_viewModelListeners.end())
                {
                    itr->second->onViewModelInstanceNameReceived(
                        handle,
                        requestId,
                        std::move(name));
                }
                break;
            }

            case Message::fileLoaded:
            {
                FileHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalFileListener)
                {
                    m_globalFileListener->onFileLoaded(handle, requestId);
                }
                auto itr = m_fileListeners.find(handle);
                if (itr != m_fileListeners.end())
                {
                    itr->second->onFileLoaded(handle, requestId);
                }
                break;
            }

            case Message::fileDeleted:
            {
                FileHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalFileListener)
                {
                    m_globalFileListener->onFileDeleted(handle, requestId);
                }
                auto itr = m_fileListeners.find(handle);
                if (itr != m_fileListeners.end())
                {
                    itr->second->onFileDeleted(handle, requestId);
                }
                break;
            }
            case Message::imageDecoded:
            {
                RenderImageHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalImageListener)
                {
                    m_globalImageListener->onRenderImageDecoded(handle,
                                                                requestId);
                }
                auto itr = m_imageListeners.find(handle);
                if (itr != m_imageListeners.end())
                {
                    itr->second->onRenderImageDecoded(handle, requestId);
                }
                break;
            }
            case Message::imageDeleted:
            {
                RenderImageHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalImageListener)
                {
                    m_globalImageListener->onRenderImageDeleted(handle,
                                                                requestId);
                }
                auto itr = m_imageListeners.find(handle);
                if (itr != m_imageListeners.end())
                {
                    itr->second->onRenderImageDeleted(handle, requestId);
                }
                break;
            }
            case Message::audioDecoded:
            {
                AudioSourceHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalAudioListener)
                {
                    m_globalAudioListener->onAudioSourceDecoded(handle,
                                                                requestId);
                }
                auto itr = m_audioListeners.find(handle);
                if (itr != m_audioListeners.end())
                {
                    itr->second->onAudioSourceDecoded(handle, requestId);
                }
                break;
            }
            case Message::audioDeleted:
            {
                AudioSourceHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalAudioListener)
                {
                    m_globalAudioListener->onAudioSourceDeleted(handle,
                                                                requestId);
                }
                auto itr = m_audioListeners.find(handle);
                if (itr != m_audioListeners.end())
                {
                    itr->second->onAudioSourceDeleted(handle, requestId);
                }
                break;
            }
            case Message::fontDecoded:
            {
                FontHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalFontListener)
                {
                    m_globalFontListener->onFontDecoded(handle, requestId);
                }
                auto itr = m_fontListeners.find(handle);
                if (itr != m_fontListeners.end())
                {
                    itr->second->onFontDecoded(handle, requestId);
                }
                break;
            }
            case Message::fontDeleted:
            {
                FontHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalFontListener)
                {
                    m_globalFontListener->onFontDeleted(handle, requestId);
                }
                auto itr = m_fontListeners.find(handle);
                if (itr != m_fontListeners.end())
                {
                    itr->second->onFontDeleted(handle, requestId);
                }
                break;
            }
            case Message::artboardDeleted:
            {
                ArtboardHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalArtboardListener)
                {
                    m_globalArtboardListener->onArtboardDeleted(handle,
                                                                requestId);
                }
                auto itr = m_artboardListeners.find(handle);
                if (itr != m_artboardListeners.end())
                {
                    itr->second->onArtboardDeleted(handle, requestId);
                }
                break;
            }
            case Message::viewModelDeleted:
            {
                ViewModelInstanceHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalViewModelListener)
                {
                    m_globalViewModelListener->onViewModelDeleted(handle,
                                                                  requestId);
                }
                auto itr = m_viewModelListeners.find(handle);
                if (itr != m_viewModelListeners.end())
                {
                    itr->second->onViewModelDeleted(handle, requestId);
                }
                break;
            }
            case Message::stateMachineSettled:
            {
                StateMachineHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalStateMachineListener)
                {
                    m_globalStateMachineListener->onStateMachineSettled(
                        handle,
                        requestId);
                }
                auto itr = m_stateMachineListeners.find(handle);
                if (itr != m_stateMachineListeners.end())
                {
                    itr->second->onStateMachineSettled(handle, requestId);
                }
                break;
            }
            case Message::stateMachineDeleted:
            {
                StateMachineHandle handle;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                lock.unlock();
                if (m_globalStateMachineListener)
                {
                    m_globalStateMachineListener->onStateMachineDeleted(
                        handle,
                        requestId);
                }
                auto itr = m_stateMachineListeners.find(handle);
                if (itr != m_stateMachineListeners.end())
                {
                    itr->second->onStateMachineDeleted(handle, requestId);
                }
                break;
            }

            case Message::fileError:
            {
                FileHandle handle;
                std::string error;
                uint64_t requestId;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageNames >> error;
                lock.unlock();
                if (m_globalFileListener)
                {
                    m_globalFileListener->onFileError(handle, requestId, error);
                }
                auto itr = m_fileListeners.find(handle);
                if (itr != m_fileListeners.end())
                {
                    itr->second->onFileError(handle,
                                             requestId,
                                             std::move(error));
                }

                break;
            }
            case Message::viewModelError:
            {
                ViewModelInstanceHandle handle;
                uint64_t requestId;

                std::string error;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageNames >> error;
                lock.unlock();
                if (m_globalViewModelListener)
                {
                    m_globalViewModelListener->onViewModelInstanceError(
                        handle,
                        requestId,
                        error);
                }
                auto itr = m_viewModelListeners.find(handle);
                if (itr != m_viewModelListeners.end())
                {
                    itr->second->onViewModelInstanceError(handle,
                                                          requestId,
                                                          std::move(error));
                }
                break;
            }
            case Message::imageError:
            {
                RenderImageHandle handle;
                uint64_t requestId;
                std::string error;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageNames >> error;
                lock.unlock();
                if (m_globalImageListener)
                {
                    m_globalImageListener->onRenderImageError(handle,
                                                              requestId,
                                                              error);
                }
                auto itr = m_imageListeners.find(handle);
                if (itr != m_imageListeners.end())
                {
                    itr->second->onRenderImageError(handle,
                                                    requestId,
                                                    std::move(error));
                }
                break;
            }

            case Message::audioError:
            {
                AudioSourceHandle handle;
                uint64_t requestId;
                std::string error;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageNames >> error;
                lock.unlock();
                if (m_globalAudioListener)
                {
                    m_globalAudioListener->onAudioSourceError(handle,
                                                              requestId,
                                                              error);
                }
                auto itr = m_audioListeners.find(handle);
                if (itr != m_audioListeners.end())
                {
                    itr->second->onAudioSourceError(handle,
                                                    requestId,
                                                    std::move(error));
                }
                break;
            }
            case Message::fontError:
            {
                FontHandle handle;
                uint64_t requestId;
                std::string error;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageNames >> error;
                lock.unlock();
                if (m_globalFontListener)
                {
                    m_globalFontListener->onFontError(handle, requestId, error);
                }
                auto itr = m_fontListeners.find(handle);
                if (itr != m_fontListeners.end())
                {
                    itr->second->onFontError(handle,
                                             requestId,
                                             std::move(error));
                }
                break;
            }

            case Message::stateMachineError:
            {
                StateMachineHandle handle;
                uint64_t requestId;
                std::string error;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageNames >> error;
                lock.unlock();
                if (m_globalStateMachineListener)
                {
                    m_globalStateMachineListener->onStateMachineError(handle,
                                                                      requestId,
                                                                      error);
                }
                auto itr = m_stateMachineListeners.find(handle);
                if (itr != m_stateMachineListeners.end())
                {
                    itr->second->onStateMachineError(handle,
                                                     requestId,
                                                     std::move(error));
                }
                break;
            }
            case Message::artboardError:
            {
                ArtboardHandle handle;
                uint64_t requestId;
                std::string error;
                m_messageStream >> handle;
                m_messageStream >> requestId;
                m_messageNames >> error;
                lock.unlock();
                if (m_globalArtboardListener)
                {
                    m_globalArtboardListener->onArtboardError(handle,
                                                              requestId,
                                                              error);
                }
                auto itr = m_artboardListeners.find(handle);
                if (itr != m_artboardListeners.end())
                {
                    itr->second->onArtboardError(handle,
                                                 requestId,
                                                 std::move(error));
                }
                break;
            }
        }

        assert(!lock.owns_lock());
        lock.lock();
    } while (!m_messageStream.empty());
}

}; // namespace rive

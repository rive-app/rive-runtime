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
                                  rcp<FileAssetLoader> loader,
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
    m_commandStream << loader;
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
                               PointerEvent pointerEvent)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::pointerMove;
    m_commandStream << stateMachineHandle;
    m_pointerEvents << std::move(pointerEvent);
}

void CommandQueue::pointerDown(StateMachineHandle stateMachineHandle,
                               PointerEvent pointerEvent)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::pointerDown;
    m_commandStream << stateMachineHandle;
    m_pointerEvents << std::move(pointerEvent);
}

void CommandQueue::pointerUp(StateMachineHandle stateMachineHandle,
                             PointerEvent pointerEvent)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::pointerUp;
    m_commandStream << stateMachineHandle;
    m_pointerEvents << std::move(pointerEvent);
}

void CommandQueue::pointerExit(StateMachineHandle stateMachineHandle,
                               PointerEvent pointerEvent)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::pointerExit;
    m_commandStream << stateMachineHandle;
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
    m_commandStream << Command::listViewModelPropertieValue;
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
    m_commandStream << Command::listViewModelPropertieValue;
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
    m_commandStream << Command::listViewModelPropertieValue;
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
    m_commandStream << Command::listViewModelPropertieValue;
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
    m_commandStream << Command::listViewModelPropertieValue;
    m_commandStream << DataType::string;
    m_commandStream << handle;
    m_commandStream << requestId;
    m_names << path;
}

void CommandQueue::requestStateMachineNames(ArtboardHandle artboardHandle,
                                            uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listStateMachines;
    m_commandStream << artboardHandle;
    m_commandStream << requestId;
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
                std::vector<PropertyData> viewModelProperties(
                    numViewProperties);

                for (auto& property : viewModelProperties)
                {
                    m_messageStream >> property.type;
                    m_messageNames >> property.name;
                }
                lock.unlock();

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

                auto itr = m_viewModelListeners.find(handle);
                if (itr != m_viewModelListeners.end())
                {
                    itr->second->onViewModelDataReceived(handle,
                                                         value,
                                                         requestId);
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
                auto itr = m_fileListeners.find(handle);
                if (itr != m_fileListeners.end())
                {
                    itr->second->onFileDeleted(handle, requestId);
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
                auto itr = m_stateMachineListeners.find(handle);
                if (itr != m_stateMachineListeners.end())
                {
                    itr->second->onStateMachineDeleted(handle, requestId);
                }
                break;
            }
            default:
            {
                RIVE_UNREACHABLE();
                break;
            }
        }

        assert(!lock.owns_lock());
        lock.lock();
    } while (!m_messageStream.empty());
}

}; // namespace rive

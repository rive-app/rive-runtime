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

void CommandQueue::requestArtboardNames(FileHandle fileHandle,
                                        uint64_t requestId)
{
    AutoLockAndNotify lock(m_commandMutex, m_commandConditionVariable);
    m_commandStream << Command::listArtboards;
    m_commandStream << fileHandle;
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
                    m_fileListeners.erase(itr);
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
                    m_artboardListeners.erase(itr);
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
                    m_stateMachineListeners.erase(itr);
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

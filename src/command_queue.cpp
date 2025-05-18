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

FileHandle CommandQueue::loadFile(std::vector<uint8_t> rivBytes)
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    auto handle = reinterpret_cast<FileHandle>(++m_currentFileHandleIdx);
    m_commandStream << Command::loadFile;
    m_commandStream << handle;
    m_byteVectors << std::move(rivBytes);
    return reinterpret_cast<FileHandle>(handle);
}

void CommandQueue::deleteFile(FileHandle fileHandle)
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    m_commandStream << Command::deleteFile;
    m_commandStream << fileHandle;
}

ArtboardHandle CommandQueue::instantiateArtboardNamed(FileHandle fileHandle,
                                                      std::string name)
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    auto handle = reinterpret_cast<FileHandle>(++m_currentArtboardHandleIdx);
    m_commandStream << Command::instantiateArtboard;
    m_commandStream << handle;
    m_commandStream << fileHandle;
    m_names << std::move(name);
    return reinterpret_cast<ArtboardHandle>(handle);
}

void CommandQueue::deleteArtboard(ArtboardHandle artboardHandle)
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    m_commandStream << Command::deleteArtboard;
    m_commandStream << artboardHandle;
}

StateMachineHandle CommandQueue::instantiateStateMachineNamed(
    ArtboardHandle artboardHandle,
    std::string name)
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    auto handle =
        reinterpret_cast<FileHandle>(++m_currentStateMachineHandleIdx);
    m_commandStream << Command::instantiateStateMachine;
    m_commandStream << handle;
    m_commandStream << artboardHandle;
    m_names << std::move(name);
    return reinterpret_cast<StateMachineHandle>(handle);
}

void CommandQueue::deleteStateMachine(StateMachineHandle stateMachineHandle)
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    m_commandStream << Command::deleteStateMachine;
    m_commandStream << stateMachineHandle;
}

DrawKey CommandQueue::createDrawKey()
{
    // lock here so we can do this from several threads safely
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    auto key = reinterpret_cast<DrawKey>(++m_currentDrawKeyIdx);
    return key;
}

void CommandQueue::draw(DrawKey drawKey, CommandServerDrawCallback callback)
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    m_commandStream << Command::draw;
    m_commandStream << drawKey;
    m_drawCallbacks << std::move(callback);
}
#ifdef TESTING
void CommandQueue::testing_messagePollBreak()
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    m_commandStream << Command::messagePollBreak;
}
#endif
void CommandQueue::runOnce(CommandServerCallback callback)
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    m_commandStream << Command::runOnce;
    m_callbacks << std::move(callback);
}

void CommandQueue::disconnect()
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    m_commandStream << Command::disconnect;
}
}; // namespace rive

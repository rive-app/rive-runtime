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
    auto handle = reinterpret_cast<FileHandle>(++m_currentFileHandleIdx);
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
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
    auto handle = reinterpret_cast<FileHandle>(++m_currentArtboardHandleIdx);
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
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
    auto handle =
        reinterpret_cast<FileHandle>(++m_currentStateMachineHandleIdx);
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
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

DrawLoopHandle CommandQueue::startDrawLoop(CommandServerCallback drawLoop)
{
    auto handle =
        reinterpret_cast<DrawLoopHandle>(++m_currentDrawLoopHandleIdx);
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    m_commandStream << Command::startDrawLoop;
    m_commandStream << handle;
    m_callbacks << std::move(drawLoop);
    return handle;
}

void CommandQueue::stopDrawLoop(DrawLoopHandle handle)
{
    AutoLockAndNotify lock(m_mutex, m_conditionVariable);
    m_commandStream << Command::stopDrawLoop;
    m_commandStream << handle;
}

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

/*
 * Copyright 2025 Rive
 */

#include "rive/command_server.hpp"

#include "rive/file.hpp"
#include "rive/animation/state_machine_instance.hpp"

namespace rive
{
CommandServer::CommandServer(rcp<CommandQueue> commandBuffer,
                             Factory* factory) :
    m_commandBuffer(std::move(commandBuffer)),
    m_factory(factory)
#ifndef NDEBUG
    ,
    m_threadID(std::this_thread::get_id())
#endif
{}

CommandServer::~CommandServer() {}

File* CommandServer::getFile(FileHandle handle) const
{
    assert(std::this_thread::get_id() == m_threadID);
    auto it = m_files.find(handle);
    return it != m_files.end() ? it->second.get() : nullptr;
}

ArtboardInstance* CommandServer::getArtboardInstance(
    ArtboardHandle handle) const
{
    assert(std::this_thread::get_id() == m_threadID);
    auto it = m_artboards.find(handle);
    return it != m_artboards.end() ? it->second.get() : nullptr;
}

StateMachineInstance* CommandServer::getStateMachineInstance(
    StateMachineHandle handle) const
{
    assert(std::this_thread::get_id() == m_threadID);
    auto it = m_stateMachines.find(handle);
    return it != m_stateMachines.end() ? it->second.get() : nullptr;
}

void CommandServer::serveUntilDisconnect()
{
    assert(std::this_thread::get_id() == m_threadID);

    TrivialObjectStream& commandStream = m_commandBuffer->m_commandStream;
    for (;;)
    {
        for (const auto& drawLoop : m_drawLoops)
        {
            drawLoop.second(this);
        }

        std::unique_lock<std::mutex> lock(m_commandBuffer->m_mutex);
        while (commandStream.empty() && m_drawLoops.empty())
        {
            // If we have no draw loops, block until we get a message.
            // Otherwise, keep running draw loops while polling for messages.
            m_commandBuffer->m_conditionVariable.wait(lock);
        }
        while (!commandStream.empty())
        {
            CommandQueue::Command command;
            commandStream >> command;
            switch (command)
            {
                case CommandQueue::Command::loadFile:
                {
                    FileHandle handle;
                    std::vector<uint8_t> rivBytes;
                    commandStream >> handle;
                    m_commandBuffer->m_byteVectors >> rivBytes;
                    lock.unlock();

                    std::unique_ptr<rive::File> file =
                        rive::File::import(rivBytes, m_factory);
                    if (file != nullptr)
                    {
                        m_files[handle] = std::move(file);
                    }
                    else
                    {
                        fprintf(stderr, "ERROR: failed to load Rive file.\n");
                    }
                    break;
                }

                case CommandQueue::Command::deleteFile:
                {
                    FileHandle handle;
                    commandStream >> handle;
                    lock.unlock();

                    m_files.erase(handle);
                    break;
                }

                case CommandQueue::Command::instantiateArtboard:
                {
                    ArtboardHandle handle;
                    FileHandle fileHandle;
                    std::string name;
                    commandStream >> handle;
                    commandStream >> fileHandle;
                    m_commandBuffer->m_names >> name;
                    lock.unlock();

                    if (rive::File* file = getFile(fileHandle))
                    {
                        if (auto artboard = name.empty()
                                                ? file->artboardDefault()
                                                : file->artboardNamed(name))
                        {
                            m_artboards[handle] = std::move(artboard);
                        }
                        else
                        {
                            fprintf(stderr,
                                    "ERROR: artboard \"%s\" not found.\n",
                                    name.c_str());
                        }
                    }
                    break;
                }

                case CommandQueue::Command::deleteArtboard:
                {
                    ArtboardHandle handle;
                    commandStream >> handle;
                    lock.unlock();

                    m_artboards.erase(handle);
                    break;
                }

                case CommandQueue::Command::instantiateStateMachine:
                {
                    StateMachineHandle handle;
                    ArtboardHandle artboardHandle;
                    std::string name;
                    commandStream >> handle;
                    commandStream >> artboardHandle;
                    m_commandBuffer->m_names >> name;
                    lock.unlock();

                    if (rive::ArtboardInstance* artboard =
                            getArtboardInstance(artboardHandle))
                    {
                        if (auto stateMachine =
                                name.empty()
                                    ? artboard->defaultStateMachine()
                                    : artboard->stateMachineNamed(name))
                        {
                            m_stateMachines[handle] = std::move(stateMachine);
                        }
                        else
                        {
                            fprintf(stderr,
                                    "ERROR: state machine \"%s\" not found.\n",
                                    name.c_str());
                        }
                    }
                    break;
                }

                case CommandQueue::Command::deleteStateMachine:
                {
                    StateMachineHandle handle;
                    commandStream >> handle;
                    lock.unlock();

                    m_stateMachines.erase(handle);
                    break;
                }

                case CommandQueue::Command::startDrawLoop:
                {
                    DrawLoopHandle handle;
                    CommandServerCallback drawLoop;
                    commandStream >> handle;
                    m_commandBuffer->m_callbacks >> drawLoop;
                    lock.unlock();

                    m_drawLoops[handle] = std::move(drawLoop);
                    break;
                }

                case CommandQueue::Command::stopDrawLoop:
                {
                    DrawLoopHandle handle;
                    commandStream >> handle;
                    lock.unlock();

                    m_drawLoops.erase(handle);
                    break;
                }

                case CommandQueue::Command::runOnce:
                {
                    CommandServerCallback callback;
                    m_commandBuffer->m_callbacks >> callback;
                    lock.unlock();

                    callback(this);
                    break;
                }

                case CommandQueue::Command::disconnect:
                {
                    lock.unlock();
                    return;
                }
            }

            // Should have unlocked by now.
            assert(!lock.owns_lock());
            lock.lock();
        }
    }
}
}; // namespace rive

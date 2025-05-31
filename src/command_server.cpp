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
    m_commandQueue(std::move(commandBuffer)),
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
    while (waitCommands())
    {
    }
}

bool CommandServer::waitCommands()
{
    PODStream& commandStream = m_commandQueue->m_commandStream;
    if (commandStream.empty())
    {
        std::unique_lock<std::mutex> lock(m_commandQueue->m_commandMutex);
        while (commandStream.empty())
            m_commandQueue->m_commandConditionVariable.wait(lock);
    }

    return processCommands();
}

bool CommandServer::processCommands()
{
    assert(m_wasDisconnectReceived == false);
    assert(std::this_thread::get_id() == m_threadID);

    PODStream& commandStream = m_commandQueue->m_commandStream;
    std::unique_lock<std::mutex> lock(m_commandQueue->m_commandMutex);

    // Early out if we don't have anything to process.
    if (commandStream.empty())
        return !m_wasDisconnectReceived;

    // Ensure we stop processing messages and get to the draw loop.
    // This avoids a race condition where we never stop processing messages and
    // therefore never draw anything.
    commandStream << CommandQueue::Command::commandLoopBreak;

    // The map should be empty at this point.
    assert(m_uniqueDraws.empty());

    bool shouldProcessCommands = true;
    do
    {
        CommandQueue::Command command;
        commandStream >> command;
        switch (command)
        {
            case CommandQueue::Command::loadFile:
            {
                FileHandle handle;
                RequestId requestId;
                std::vector<uint8_t> rivBytes;
                rcp<FileAssetLoader> loader;
                commandStream >> handle;
                commandStream >> requestId;
                commandStream >> loader;
                m_commandQueue->m_byteVectors >> rivBytes;
                lock.unlock();
                std::unique_ptr<rive::File> file =
                    rive::File::import(rivBytes,
                                       m_factory,
                                       nullptr,
                                       std::move(loader));
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
                RequestId requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                m_files.erase(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                m_commandQueue->m_messageStream
                    << CommandQueue::Message::fileDeleted;
                m_commandQueue->m_messageStream << handle;
                m_commandQueue->m_messageStream << requestId;
                break;
            }

            case CommandQueue::Command::instantiateArtboard:
            {
                ArtboardHandle handle;
                FileHandle fileHandle;
                RequestId requestId;
                std::string name;
                commandStream >> handle;
                commandStream >> fileHandle;
                commandStream >> requestId;
                m_commandQueue->m_names >> name;
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
                RequestId requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                m_artboards.erase(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                m_commandQueue->m_messageStream
                    << CommandQueue::Message::artboardDeleted;
                m_commandQueue->m_messageStream << handle;
                m_commandQueue->m_messageStream << requestId;
                break;
            }

            case CommandQueue::Command::instantiateStateMachine:
            {
                StateMachineHandle handle;
                ArtboardHandle artboardHandle;
                RequestId requestId;
                std::string name;
                commandStream >> handle;
                commandStream >> artboardHandle;
                commandStream >> requestId;
                m_commandQueue->m_names >> name;
                lock.unlock();
                if (rive::ArtboardInstance* artboard =
                        getArtboardInstance(artboardHandle))
                {
                    if (auto stateMachine =
                            name.empty() ? artboard->defaultStateMachine()
                                         : artboard->stateMachineNamed(name))
                    {
                        m_stateMachines[handle] = std::move(stateMachine);
                    }
                    else
                    {
                        fprintf(stderr,
                                "ERROR: Could not create state machine with "
                                "name \"%s\" because it was not found.\n",
                                name.c_str());
                    }
                }
                break;
            }

            case CommandQueue::Command::advanceStateMachine:
            {
                StateMachineHandle handle;
                RequestId requestId;
                float timeToAdvance;
                commandStream >> handle;
                commandStream >> requestId;
                commandStream >> timeToAdvance;
                lock.unlock();

                if (auto stateMachine = getStateMachineInstance(handle))
                {
                    if (!stateMachine->advanceAndApply(timeToAdvance))
                    {
                        std::unique_lock<std::mutex> messageLock(
                            m_commandQueue->m_messageMutex);
                        m_commandQueue->m_messageStream
                            << CommandQueue::Message::stateMachineSettled;
                        m_commandQueue->m_messageStream << handle;
                        m_commandQueue->m_messageStream << requestId;
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: State machine \"%llu\" not found for "
                            "advance.\n",
                            reinterpret_cast<unsigned long long>(handle));
                }

                break;
            }

            case CommandQueue::Command::deleteStateMachine:
            {
                StateMachineHandle handle;
                RequestId requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                m_stateMachines.erase(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                m_commandQueue->m_messageStream
                    << CommandQueue::Message::stateMachineDeleted;
                m_commandQueue->m_messageStream << handle;
                m_commandQueue->m_messageStream << requestId;
                break;
            }

            case CommandQueue::Command::runOnce:
            {
                CommandServerCallback callback;
                m_commandQueue->m_callbacks >> callback;
                lock.unlock();
                callback(this);
                break;
            }

            case CommandQueue::Command::draw:
            {
                DrawKey drawKey;
                CommandServerDrawCallback drawCallback;
                commandStream >> drawKey;
                m_commandQueue->m_drawCallbacks >> drawCallback;
                lock.unlock();
                m_uniqueDraws[drawKey] = std::move(drawCallback);
                break;
            }

            case CommandQueue::Command::commandLoopBreak:
            {
                lock.unlock();
                shouldProcessCommands = false;
                break;
            }

            case CommandQueue::Command::listArtboards:
            {
                FileHandle handle;
                RequestId requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                auto file = getFile(handle);
                if (file)
                {
                    auto artboards = file->artboards();

                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    m_commandQueue->m_messageStream
                        << CommandQueue::Message::artboardsListed;
                    m_commandQueue->m_messageStream << handle;
                    m_commandQueue->m_messageStream << requestId;
                    m_commandQueue->m_messageStream << artboards.size();
                    for (auto artboard : artboards)
                    {
                        m_commandQueue->m_messageNames << artboard->name();
                    }
                }

                break;
            }

            case CommandQueue::Command::listStateMachines:
            {
                ArtboardHandle handle;
                RequestId requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                auto artboard = getArtboardInstance(handle);
                if (artboard)
                {
                    auto numStateMachines = artboard->stateMachineCount();
                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    m_commandQueue->m_messageStream
                        << CommandQueue::Message::stateMachinesListed;
                    m_commandQueue->m_messageStream << handle;
                    m_commandQueue->m_messageStream << requestId;
                    m_commandQueue->m_messageStream << numStateMachines;
                    for (int i = 0; i < numStateMachines; ++i)
                    {
                        m_commandQueue->m_messageNames
                            << artboard->stateMachineNameAt(i);
                    }
                }
                break;
            }

            case CommandQueue::Command::disconnect:
            {
                lock.unlock();
                m_wasDisconnectReceived = true;
                return false;
            }
        }

        // Should have unlocked by now.
        assert(!lock.owns_lock());
        lock.lock();
    } while (!commandStream.empty() && shouldProcessCommands);

    for (const auto& drawPair : m_uniqueDraws)
    {
        drawPair.second(drawPair.first, this);
    }

    m_uniqueDraws.clear();

    return !m_wasDisconnectReceived;
}
}; // namespace rive

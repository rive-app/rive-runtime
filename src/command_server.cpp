/*
 * Copyright 2025 Rive
 */

#include "rive/command_server.hpp"

#include "rive/file.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
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

ViewModelInstanceRuntime* CommandServer::getViewModelInstance(
    ViewModelInstanceHandle handle) const
{
    assert(std::this_thread::get_id() == m_threadID);
    auto it = m_viewModels.find(handle);
    return it != m_viewModels.end() ? it->second.get() : nullptr;
}

Vec2D CommandServer::cursorPosForPointerEvent(
    StateMachineInstance* instance,
    const CommandQueue::PointerEvent& pointerEvent)
{
    AABB artboardBounds = instance->artboard()->bounds();
    AABB surfaceBounds = AABB(Vec2D{0.0f, 0.0f}, pointerEvent.screenBounds);

    if (surfaceBounds == artboardBounds || surfaceBounds.width() == 0.0f ||
        surfaceBounds.height() == 0.0f)
    {
        return pointerEvent.position;
    }

    Mat2D forward = rive::computeAlignment(pointerEvent.fit,
                                           pointerEvent.alignment,
                                           surfaceBounds,
                                           artboardBounds,
                                           pointerEvent.scaleFactor);

    Mat2D inverse = forward.invertOrIdentity();

    return inverse * pointerEvent.position;
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
    PODStream& messageStream = m_commandQueue->m_messageStream;
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
                uint64_t requestId;
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
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                m_files.erase(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                messageStream << CommandQueue::Message::fileDeleted;
                messageStream << handle;
                messageStream << requestId;
                break;
            }

            case CommandQueue::Command::instantiateArtboard:
            {
                ArtboardHandle handle;
                FileHandle fileHandle;
                uint64_t requestId;
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
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                m_artboards.erase(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                messageStream << CommandQueue::Message::artboardDeleted;
                messageStream << handle;
                messageStream << requestId;
                break;
            }

            case CommandQueue::Command::instantiateViewModel:
            case CommandQueue::Command::instantiateBlankViewModel:
            case CommandQueue::Command::instantiateViewModelForArtboard:
            case CommandQueue::Command::instantiateBlankViewModelForArtboard:
            {
                bool usesInstanceName =
                    command == CommandQueue::Command::instantiateViewModel ||
                    command ==
                        CommandQueue::Command::instantiateViewModelForArtboard;
                bool usesArtboard =
                    command == CommandQueue::Command::
                                   instantiateBlankViewModelForArtboard ||
                    command ==
                        CommandQueue::Command::instantiateViewModelForArtboard;

                FileHandle fileHandle;
                ArtboardHandle artboardHandle;
                ViewModelInstanceHandle viewHandle;
                uint64_t requestId;
                std::string viewModelName;
                std::string viewModelInstanceName;
                commandStream >> fileHandle;
                if (usesArtboard)
                {
                    commandStream >> artboardHandle;
                }
                commandStream >> viewHandle;
                commandStream >> requestId;
                if (!usesArtboard)
                {
                    m_commandQueue->m_names >> viewModelName;
                }
                if (usesInstanceName)
                {
                    m_commandQueue->m_names >> viewModelInstanceName;
                }
                lock.unlock();
                if (auto file = getFile(fileHandle))
                {
                    ViewModelRuntime* viewModel = nullptr;

                    if (usesArtboard)
                    {
                        if (auto artboardInstance =
                                getArtboardInstance(artboardHandle))
                        {
                            viewModel = file->defaultArtboardViewModel(
                                artboardInstance);
                        }
                        else
                        {
                            fprintf(stderr,
                                    "ERROR: artboardInstance not found when "
                                    "trying to create default view model\n");
                        }
                    }
                    else
                    {
                        viewModel = file->viewModelByName(viewModelName);
                    }

                    if (viewModel)
                    {
                        rcp<ViewModelInstanceRuntime> instance;

                        if (usesInstanceName)
                        {
                            if (viewModelInstanceName.empty())
                            {
                                instance =
                                    ref_rcp(viewModel->createDefaultInstance());
                            }
                            else
                            {
                                instance =
                                    ref_rcp(viewModel->createInstanceFromName(
                                        viewModelInstanceName));
                            }
                        }
                        else
                        {
                            instance = ref_rcp(viewModel->createInstance());
                        }

                        if (instance)
                        {
                            m_viewModels[viewHandle] = std::move(instance);
                        }
                        else
                        {
                            fprintf(stderr,
                                    "ERROR: Could not create view instance\n");
                        }
                    }
                    else
                    {
                        fprintf(stderr, "ERROR: view model not found\n");
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: file not found for view model runtime\n");
                }
                break;
            }

            case CommandQueue::Command::deleteViewModel:
            {
                ViewModelInstanceHandle handle;
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                m_viewModels.erase(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                messageStream << CommandQueue::Message::viewModelDeleted;
                messageStream << handle;
                messageStream << requestId;
                break;
            }
            case CommandQueue::Command::instantiateStateMachine:
            {
                StateMachineHandle handle;
                ArtboardHandle artboardHandle;
                uint64_t requestId;
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

            case CommandQueue::Command::bindViewModelInstance:
            {
                StateMachineHandle handle;
                ViewModelInstanceHandle viewModel;
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> viewModel;
                commandStream >> requestId;
                lock.unlock();

                if (auto stateMachine = getStateMachineInstance(handle))
                {
                    if (auto viewModelInstance =
                            getViewModelInstance(viewModel))
                    {
                        stateMachine->bindViewModelInstance(
                            viewModelInstance->instance());
                    }
                    else
                    {
                        fprintf(stderr,
                                "ERROR: View Model not found for bind\n");
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: State machine \"%llu\" not found for "
                            "binding view model.\n",
                            reinterpret_cast<unsigned long long>(handle));
                }
                break;
            }

            case CommandQueue::Command::advanceStateMachine:
            {
                StateMachineHandle handle;
                uint64_t requestId;
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
                        messageStream
                            << CommandQueue::Message::stateMachineSettled;
                        messageStream << handle;
                        messageStream << requestId;
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
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                m_stateMachines.erase(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                messageStream << CommandQueue::Message::stateMachineDeleted;
                messageStream << handle;
                messageStream << requestId;
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
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                auto file = getFile(handle);
                if (file)
                {
                    auto artboards = file->artboards();

                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::artboardsListed;
                    messageStream << handle;
                    messageStream << requestId;
                    messageStream << artboards.size();
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
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                auto artboard = getArtboardInstance(handle);
                if (artboard)
                {
                    auto numStateMachines = artboard->stateMachineCount();
                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::stateMachinesListed;
                    messageStream << handle;
                    messageStream << requestId;
                    messageStream << numStateMachines;
                    for (int i = 0; i < numStateMachines; ++i)
                    {
                        m_commandQueue->m_messageNames
                            << artboard->stateMachineNameAt(i);
                    }
                }
                break;
            }

            case CommandQueue::Command::listViewModels:
            {
                FileHandle handle;
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                auto file = getFile(handle);
                if (file)
                {
                    auto numViewModels = file->viewModelCount();

                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::viewModelsListend;
                    messageStream << handle;
                    messageStream << requestId;
                    messageStream << numViewModels;
                    for (int i = 0; i < numViewModels; ++i)
                    {
                        m_commandQueue->m_messageNames
                            << file->viewModelByIndex(i)->name();
                    }
                }
                break;
            }

            case CommandQueue::Command::listViewModelInstanceNames:
            {
                FileHandle handle;
                uint64_t requestId;
                std::string viewModelName;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_names >> viewModelName;
                lock.unlock();
                auto file = getFile(handle);
                if (file)
                {
                    auto model = file->viewModelByName(viewModelName);
                    if (model)
                    {
                        auto names = model->instanceNames();

                        std::unique_lock<std::mutex> messageLock(
                            m_commandQueue->m_messageMutex);
                        messageStream << CommandQueue::Message::
                                viewModelInstanceNamesListed;
                        messageStream << handle;
                        messageStream << requestId;
                        messageStream << names.size();
                        m_commandQueue->m_messageNames << viewModelName;
                        for (auto& name : names)
                        {
                            m_commandQueue->m_messageNames << name;
                        }
                    }
                }
                break;
            }

            case CommandQueue::Command::listViewModelProperties:
            {
                FileHandle handle;
                uint64_t requestId;
                std::string viewModelName;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_names >> viewModelName;
                lock.unlock();
                auto file = getFile(handle);
                if (file)
                {
                    auto model = file->viewModelByName(viewModelName);
                    if (model)
                    {
                        auto properties = model->properties();

                        std::unique_lock<std::mutex> messageLock(
                            m_commandQueue->m_messageMutex);
                        messageStream
                            << CommandQueue::Message::viewModelPropertiesListed;
                        messageStream << handle;
                        messageStream << requestId;
                        messageStream << properties.size();
                        m_commandQueue->m_messageNames << viewModelName;
                        for (auto& property : properties)
                        {
                            messageStream << property.type;
                            m_commandQueue->m_messageNames << property.name;
                        }
                    }
                }
                break;
            }

            case CommandQueue::Command::pointerMove:
            {
                StateMachineHandle handle;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                m_commandQueue->m_pointerEvents >> pointerEvent;
                lock.unlock();
                if (auto stateMachine = getStateMachineInstance(handle))
                {
                    Vec2D position =
                        cursorPosForPointerEvent(stateMachine, pointerEvent);
                    stateMachine->pointerMove(position);
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: State machine \"%llu\" not found for "
                            "pointerMove.\n",
                            reinterpret_cast<unsigned long long>(handle));
                }
                break;
            }

            case CommandQueue::Command::pointerDown:
            {
                StateMachineHandle handle;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                m_commandQueue->m_pointerEvents >> pointerEvent;
                lock.unlock();
                if (auto stateMachine = getStateMachineInstance(handle))
                {
                    Vec2D position =
                        cursorPosForPointerEvent(stateMachine, pointerEvent);
                    stateMachine->pointerDown(position);
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: State machine \"%llu\" not found for "
                            "pointerDown.\n",
                            reinterpret_cast<unsigned long long>(handle));
                }
                break;
            }

            case CommandQueue::Command::pointerUp:
            {
                StateMachineHandle handle;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                m_commandQueue->m_pointerEvents >> pointerEvent;
                lock.unlock();
                if (auto stateMachine = getStateMachineInstance(handle))
                {
                    Vec2D position =
                        cursorPosForPointerEvent(stateMachine, pointerEvent);
                    stateMachine->pointerUp(position);
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: State machine \"%llu\" not found for "
                            "pointerUp.\n",
                            reinterpret_cast<unsigned long long>(handle));
                }
                break;
            }

            case CommandQueue::Command::pointerExit:
            {
                StateMachineHandle handle;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                m_commandQueue->m_pointerEvents >> pointerEvent;
                lock.unlock();
                if (auto stateMachine = getStateMachineInstance(handle))
                {
                    Vec2D position =
                        cursorPosForPointerEvent(stateMachine, pointerEvent);
                    stateMachine->pointerExit(position);
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: State machine \"%llu\" not found for "
                            "pointerExit.\n",
                            reinterpret_cast<unsigned long long>(handle));
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

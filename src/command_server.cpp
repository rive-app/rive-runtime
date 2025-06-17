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

RenderImage* CommandServer::getImage(RenderImageHandle handle) const
{
    assert(std::this_thread::get_id() == m_threadID);
    auto it = m_images.find(handle);
    return it != m_images.end() ? it->second.get() : nullptr;
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

void CommandServer::checkPropertySubscriptions()
{
    for (auto& subscription : m_propertySubscriptions)
    {
        uint64_t requestId = subscription.requestId;
        ViewModelInstanceHandle handle = subscription.rootViewModel;
        CommandQueue::ViewModelInstanceData data;
        data.metaData = subscription.data;

        if (auto viewModel = getViewModelInstance(handle))
        {
            if (auto property = viewModel->property(data.metaData.name))
            {
                if (property->hasChanged())
                {
                    property->clearChanges();
                    switch (data.metaData.type)
                    {
                        // These don't have values but are still valid
                        // subscriptions.
                        case DataType::assetImage:
                        case DataType::trigger:
                        case DataType::list:
                            break;
                        case DataType::boolean:
                            if (auto value = static_cast<
                                    ViewModelInstanceBooleanRuntime*>(property))
                            {
                                data.boolValue = value->value();
                            }
                            break;

                        case DataType::color:
                            if (auto value =
                                    static_cast<ViewModelInstanceColorRuntime*>(
                                        property))
                            {
                                data.colorValue = value->value();
                            }
                            break;

                        case DataType::number:
                            if (auto value = static_cast<
                                    ViewModelInstanceNumberRuntime*>(property))
                            {
                                data.numberValue = value->value();
                            }
                            break;

                        case DataType::enumType:
                            if (auto value =
                                    static_cast<ViewModelInstanceEnumRuntime*>(
                                        property))
                            {
                                data.stringValue = value->value();
                            }
                            break;

                        case DataType::string:
                            if (auto value = static_cast<
                                    ViewModelInstanceStringRuntime*>(property))
                            {
                                data.stringValue = value->value();
                            }
                            break;
                        default:
                            fprintf(
                                stderr,
                                "ERROR: Invalid data type {%i} when checking "
                                "subscriptions",
                                static_cast<unsigned int>(data.metaData.type));
                            continue;
                    }

                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    m_commandQueue->m_messageStream << CommandQueue::Message::
                            viewModelPropertyValueReceived;
                    m_commandQueue->m_messageStream << handle;
                    m_commandQueue->m_messageStream << data.metaData.type;
                    m_commandQueue->m_messageNames << data.metaData.name;
                    m_commandQueue->m_messageStream << requestId;
                    switch (data.metaData.type)
                    {
                        case DataType::assetImage:
                        case DataType::trigger:
                        case DataType::list:
                            break;
                        case DataType::boolean:
                            m_commandQueue->m_messageStream << data.boolValue;
                            break;
                        case DataType::number:
                            m_commandQueue->m_messageStream << data.numberValue;
                            break;
                        case DataType::color:
                            m_commandQueue->m_messageStream << data.colorValue;
                            break;
                        case DataType::enumType:
                        case DataType::string:
                            m_commandQueue->m_messageNames << data.stringValue;
                            break;
                        default:
                            RIVE_UNREACHABLE();
                    }
                }
            }
        }
    }
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
        {
            assert(m_commandQueue->m_callbacks.empty());
            assert(m_commandQueue->m_byteVectors.empty());
            assert(m_commandQueue->m_names.empty());
            m_commandQueue->m_commandConditionVariable.wait(lock);
        }
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
                                       loader ? std::move(loader) : nullptr);
                if (file != nullptr)
                {
                    m_files[handle] = std::move(file);

                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::fileLoaded;
                    messageStream << handle;
                    messageStream << requestId;
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

            case CommandQueue::Command::decodeImage:
            {
                RenderImageHandle handle;
                uint64_t requestId;
                std::vector<uint8_t> bytes;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_byteVectors >> bytes;
                lock.unlock();

                auto image = factory()->decodeImage(bytes);
                if (image)
                {
                    m_images[handle] = std::move(image);
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: Command Server failed to decode image\n");
                }

                break;
            }

            case CommandQueue::Command::deleteImage:
            {
                RenderImageHandle handle;
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                m_images.erase(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                messageStream << CommandQueue::Message::imageDeleted;
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

            case CommandQueue::Command::addViewModelListValue:
            {
                ViewModelInstanceHandle rootHandle;
                ViewModelInstanceHandle viewHandle;
                uint64_t requestId;
                std::string path;
                int index;
                commandStream >> rootHandle;
                commandStream >> viewHandle;
                commandStream >> index;
                commandStream >> requestId;
                m_commandQueue->m_names >> path;
                lock.unlock();

                if (auto root = getViewModelInstance(rootHandle))
                {
                    if (auto viewModel = getViewModelInstance(viewHandle))
                    {
                        if (auto property = root->propertyList(path))
                        {
                            if (index >= 0)
                                property->addInstanceAt(viewModel, index);
                            else
                                property->addInstance(viewModel);
                        }
                        else
                        {
                            fprintf(stderr,
                                    "ERROR: failed to find list at path %s for "
                                    "add list\n",
                                    path.c_str());
                        }
                    }
                    else
                    {
                        fprintf(stderr,
                                "ERROR: failed to find value view model "
                                "instance for add list\n");
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: failed to find root view model instance "
                            "for add list\n");
                }

                break;
            }

            case CommandQueue::Command::removeViewModelListValue:
            {
                ViewModelInstanceHandle rootHandle;
                ViewModelInstanceHandle viewHandle;
                uint64_t requestId;
                std::string path;
                int index;
                commandStream >> rootHandle;
                commandStream >> viewHandle;
                commandStream >> index;
                commandStream >> requestId;
                m_commandQueue->m_names >> path;
                lock.unlock();

                if (auto root = getViewModelInstance(rootHandle))
                {
                    if (auto property = root->propertyList(path))
                    {
                        if (index >= 0)
                        {
                            property->removeInstanceAt(index);
                        }
                        else if (auto viewModel =
                                     getViewModelInstance(viewHandle))
                        {
                            property->removeInstance(viewModel);
                        }
                    }
                    else
                    {
                        fprintf(stderr,
                                "ERROR: failed to find list on view model "
                                "instance for "
                                "remove at path %s\n",
                                path.c_str());
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: failed to find view model instance for "
                            "remove list\n");
                }

                break;
            }

            case CommandQueue::Command::swapViewModelListValue:
            {
                ViewModelInstanceHandle rootHandle;
                uint64_t requestId;
                std::string path;
                int indexa, indexb;
                commandStream >> rootHandle;
                commandStream >> indexa;
                commandStream >> indexb;
                commandStream >> requestId;
                m_commandQueue->m_names >> path;
                lock.unlock();
                if (auto viewModel = getViewModelInstance(rootHandle))
                {
                    if (auto list = viewModel->propertyList(path))
                    {
                        list->swap(indexa, indexb);
                    }
                    else
                    {
                        fprintf(stderr,
                                "ERROR: failed to find list on view model "
                                "instance for "
                                "swap at path %s\n",
                                path.c_str());
                    }
                }
                else
                {
                    fprintf(
                        stderr,
                        "ERROR: failed to find view model instance for swap\n");
                }
                break;
            }

            case CommandQueue::Command::subscribeViewModelProperty:
            case CommandQueue::Command::unsubscribeViewModelProperty:
            {
                ViewModelInstanceHandle rootHandle;
                PropertyData data;
                uint64_t requestId;
                commandStream >> rootHandle;
                commandStream >> data.type;
                commandStream >> requestId;
                m_commandQueue->m_names >> data.name;
                lock.unlock();

                if (command ==
                    CommandQueue::Command::subscribeViewModelProperty)
                {
                    // Validate that this is a valid thing to subscribe to.

                    if (auto view = getViewModelInstance(rootHandle))
                    {
                        if (data.type != DataType::viewModel &&
                            data.type != DataType::integer &&
                            data.type != DataType::none &&
                            data.type != DataType::symbolListIndex)
                        {
                            if (view->property(data.name) != nullptr)
                            {
                                m_propertySubscriptions.push_back(
                                    {requestId, data, rootHandle});
                            }
                            else
                            {
                                fprintf(stderr,
                                        "ERROR: property %s not found "
                                        "when subscribing\n",
                                        data.name.c_str());
                            }
                        }
                        else
                        {
                            fprintf(stderr,
                                    "ERROR: property type %i is not valid when "
                                    "subscribing\n",
                                    static_cast<unsigned int>(data.type));
                        }
                    }
                    else
                    {
                        fprintf(stderr,
                                "ERROR: root view not found when subscribing "
                                "to property %s\n",
                                data.name.c_str());
                    }
                }
                else
                {
                    auto itr = std::remove_if(
                        m_propertySubscriptions.begin(),
                        m_propertySubscriptions.end(),
                        [data, rootHandle](const Subscription& val) {
                            return val.data.name == data.name &&
                                   val.data.type == data.type &&
                                   val.rootViewModel == rootHandle;
                        });

                    m_propertySubscriptions.erase(
                        itr,
                        m_propertySubscriptions.end());
                }

                break;
            }

            case CommandQueue::Command::refNestedViewModel:
            {
                ViewModelInstanceHandle rootViewHandle;
                ViewModelInstanceHandle nestedViewHandle;
                uint64_t requestId;
                std::string path;
                commandStream >> rootViewHandle;
                commandStream >> nestedViewHandle;
                commandStream >> requestId;
                m_commandQueue->m_names >> path;
                lock.unlock();

                if (auto rootViewInstance =
                        getViewModelInstance(rootViewHandle))
                {
                    if (auto nestedViewModel =
                            rootViewInstance->propertyViewModel(path))
                    {
                        m_viewModels[nestedViewHandle] =
                            ref_rcp(nestedViewModel);
                    }
                    else
                    {
                        fprintf(
                            stderr,
                            "ERROR: nested view not found when refing nested "
                            "view model at path %s\n",
                            path.c_str());
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: root view not found when refing nested "
                            "view model at path %s\n",
                            path.c_str());
                }
                break;
            }

            case CommandQueue::Command::refListViewModel:
            {
                ViewModelInstanceHandle rootViewHandle;
                ViewModelInstanceHandle listViewHandle;
                int index;
                uint64_t requestId;
                std::string path;
                commandStream >> rootViewHandle;
                commandStream >> index;
                commandStream >> listViewHandle;
                commandStream >> requestId;
                m_commandQueue->m_names >> path;
                lock.unlock();

                if (auto rootViewInstance =
                        getViewModelInstance(rootViewHandle))
                {
                    if (auto list = rootViewInstance->propertyList(path))
                    {
                        auto viewModelInstance = list->instanceAt(index);
                        if (viewModelInstance)
                        {
                            m_viewModels[listViewHandle] =
                                ref_rcp(viewModelInstance);
                        }
                        else
                        {
                            fprintf(stderr,
                                    "ERROR: View model not found on list %s at "
                                    "index %i\n",
                                    path.c_str(),
                                    index);
                        }
                    }
                    else
                    {
                        fprintf(stderr,
                                "ERROR: list not found at path %s when refing "
                                "view model at index %i\n",
                                path.c_str(),
                                index);
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: root view not found when refing nested "
                            "view model at path %s\n",
                            path.c_str());
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

            case CommandQueue::Command::listViewModelEnums:
            {
                FileHandle handle;
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                auto file = getFile(handle);
                if (file)
                {
                    auto enums = file->enums();
                    std::vector<ViewModelEnum> enumDatas;

                    for (auto tenum : enums)
                    {
                        ViewModelEnum data;
                        data.name = tenum->enumName();
                        auto values = tenum->values();
                        for (auto value : values)
                        {
                            data.enumerants.push_back(value->key());
                        }

                        enumDatas.push_back(data);
                    }

                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream
                        << CommandQueue::Message::viewModelEnumsListed;
                    messageStream << handle;
                    messageStream << requestId;
                    messageStream << enumDatas.size();
                    for (auto& enumData : enumDatas)
                    {
                        m_commandQueue->m_messageNames << enumData.name;
                        messageStream << enumData.enumerants.size();
                        for (auto& enumValueName : enumData.enumerants)
                            m_commandQueue->m_messageNames << enumValueName;
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

            case CommandQueue::Command::setViewModelInstanceValue:
            {
                ViewModelInstanceHandle handle = RIVE_NULL_HANDLE;
                ViewModelInstanceHandle nestedHandle = RIVE_NULL_HANDLE;
                RenderImageHandle imageHandle = RIVE_NULL_HANDLE;
                uint64_t requestId;
                CommandQueue::ViewModelInstanceData value;

                commandStream >> handle;
                commandStream >> value.metaData.type;
                commandStream >> requestId;
                m_commandQueue->m_names >> value.metaData.name;

                switch (value.metaData.type)
                {
                    // Triggers are valid but have no data.
                    case DataType::trigger:
                        break;
                    case DataType::boolean:
                        commandStream >> value.boolValue;
                        break;
                    case DataType::number:
                        commandStream >> value.numberValue;
                        break;
                    case DataType::color:
                        commandStream >> value.colorValue;
                        break;
                    case DataType::string:
                    case DataType::enumType:
                        m_commandQueue->m_names >> value.stringValue;
                        break;
                    case DataType::viewModel:
                        commandStream >> nestedHandle;
                        break;
                    case DataType::assetImage:
                        commandStream >> imageHandle;
                        break;
                    default:
                        RIVE_UNREACHABLE();
                }
                lock.unlock();

                if (auto viewModelInstance = getViewModelInstance(handle))
                {
                    switch (value.metaData.type)
                    {
                        case DataType::trigger:
                        {
                            if (auto property =
                                    viewModelInstance->propertyTrigger(
                                        value.metaData.name))
                            {
                                property->trigger();
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "setting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::boolean:
                        {
                            if (auto property =
                                    viewModelInstance->propertyBoolean(
                                        value.metaData.name))
                            {
                                property->value(value.boolValue);
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "setting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::number:
                        {
                            if (auto property =
                                    viewModelInstance->propertyNumber(
                                        value.metaData.name))
                            {
                                property->value(value.numberValue);
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "setting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::color:
                        {
                            if (auto property =
                                    viewModelInstance->propertyColor(
                                        value.metaData.name))
                            {
                                property->value(value.colorValue);
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "setting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::string:
                        {
                            if (auto property =
                                    viewModelInstance->propertyString(
                                        value.metaData.name))
                            {
                                property->value(value.stringValue);
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "setting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::enumType:
                        {
                            if (auto property = viewModelInstance->propertyEnum(
                                    value.metaData.name))
                            {
                                property->value(value.stringValue);
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "setting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::viewModel:
                        {
                            if (auto nestedViewModel =
                                    getViewModelInstance(nestedHandle))
                            {
                                if (!viewModelInstance->replaceViewModel(
                                        value.metaData.name,
                                        nestedViewModel))
                                {
                                    fprintf(stderr,
                                            "ERROR: Could not replace view "
                                            "model at "
                                            "path %s",
                                            value.metaData.name.c_str());
                                }
                            }
                            else
                            {
                                fprintf(stderr,
                                        "ERROR: Could not find view model to "
                                        "set for "
                                        "view model "
                                        "instance when "
                                        "setting property with path %s\n",
                                        value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::assetImage:
                        {
                            if (auto image = getImage(imageHandle))
                            {
                                if (auto imageProperty =
                                        viewModelInstance->propertyImage(
                                            value.metaData.name))
                                {
                                    imageProperty->value(image);
                                }
                                else
                                {
                                    fprintf(stderr,
                                            "ERROR: Could not find image "
                                            "property at "
                                            "path %s",
                                            value.metaData.name.c_str());
                                }
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find image to set for "
                                    "view model "
                                    "instance when "
                                    "setting property with path %s\n",
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        default:
                            RIVE_UNREACHABLE();
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: Could not find view model instance when "
                            "setting property type %u with path %s\n",
                            static_cast<unsigned int>(value.metaData.type),
                            value.metaData.name.c_str());
                }
                break;
            }

            case CommandQueue::Command::listViewModelPropertyValue:
            {
                ViewModelInstanceHandle handle;
                uint64_t requestId;
                CommandQueue::ViewModelInstanceData value;
                commandStream >> value.metaData.type;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_names >> value.metaData.name;
                lock.unlock();

                if (auto viewModelInstance = getViewModelInstance(handle))
                {
                    switch (value.metaData.type)
                    {
                        case DataType::boolean:
                        {
                            if (auto property =
                                    viewModelInstance->propertyBoolean(
                                        value.metaData.name))
                            {
                                value.boolValue = property->value();
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "getting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::number:
                        {
                            if (auto property =
                                    viewModelInstance->propertyNumber(
                                        value.metaData.name))
                            {
                                value.numberValue = property->value();
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "getting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::color:
                        {
                            if (auto property =
                                    viewModelInstance->propertyColor(
                                        value.metaData.name))
                            {
                                value.colorValue = property->value();
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "getting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::string:
                        {
                            if (auto property =
                                    viewModelInstance->propertyString(
                                        value.metaData.name))
                            {
                                value.stringValue = property->value();
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "getting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        case DataType::enumType:
                        {
                            if (auto property = viewModelInstance->propertyEnum(
                                    value.metaData.name))
                            {
                                value.stringValue = property->value();
                            }
                            else
                            {
                                fprintf(
                                    stderr,
                                    "ERROR: Could not find view model property "
                                    "instance when "
                                    "getting property type %u with path %s\n",
                                    static_cast<unsigned int>(
                                        value.metaData.type),
                                    value.metaData.name.c_str());
                            }
                            break;
                        }
                        default:
                            RIVE_UNREACHABLE();
                    }

                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::
                            viewModelPropertyValueReceived;
                    messageStream << handle;
                    messageStream << value.metaData.type;
                    m_commandQueue->m_messageNames << value.metaData.name;
                    messageStream << requestId;
                    switch (value.metaData.type)
                    {
                        case DataType::boolean:
                            messageStream << value.boolValue;
                            break;
                        case DataType::number:
                            messageStream << value.numberValue;
                            break;
                        case DataType::color:
                            messageStream << value.colorValue;
                            break;
                        case DataType::enumType:
                        case DataType::string:
                            m_commandQueue->m_messageNames << value.stringValue;
                            break;
                        default:
                            RIVE_UNREACHABLE();
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: Could not find view model instance when "
                            "getting property type %u with path %s\n",
                            static_cast<unsigned int>(value.metaData.type),
                            value.metaData.name.c_str());
                }
                break;
            }

            case CommandQueue::Command::getViewModelListSize:
            {
                ViewModelInstanceHandle handle;
                uint64_t requestId;
                std::string path;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_names >> path;
                lock.unlock();

                if (auto viewModel = getViewModelInstance(handle))
                {
                    if (auto property = viewModel->propertyList(path))
                    {
                        auto size = property->size();
                        std::unique_lock<std::mutex> messageLock(
                            m_commandQueue->m_messageMutex);
                        messageStream
                            << CommandQueue::Message::viewModelListSizeReceived;
                        messageStream << handle;
                        messageStream << size;
                        messageStream << requestId;
                        m_commandQueue->m_messageNames << path;
                    }
                    else
                    {
                        fprintf(
                            stderr,
                            "ERROR: failed to get list at path %s when getting "
                            "list size\n",
                            path.c_str());
                    }
                }
                else
                {
                    fprintf(stderr,
                            "ERROR: failed to get view model when getting list "
                            "size\n");
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

    // Since we always retake the lock at the end of the do while we need to
    // unlock here.
    lock.unlock();

    for (const auto& drawPair : m_uniqueDraws)
    {
        drawPair.second(drawPair.first, this);
    }

    m_uniqueDraws.clear();

    checkPropertySubscriptions();

    return !m_wasDisconnectReceived;
}
}; // namespace rive

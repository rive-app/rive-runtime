/*
 * Copyright 2025 Rive
 */

#include "rive/command_server.hpp"

#include "rive/file.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/assets/audio_asset.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/viewmodel/runtime/viewmodel_runtime.hpp"
#include "rive/animation/state_machine_instance.hpp"

namespace rive
{

class CommandServer::CommandFileAssetLoader : public FileAssetLoader
{
public:
    CommandFileAssetLoader(CommandServer* server) : m_server(server) {}

    virtual bool loadContents(FileAsset& asset,
                              Span<const uint8_t> inBandBytes,
                              Factory* factory) override
    {
        if (asset.is<ImageAsset>())
        {
            // No need for another if because as just asserts the above
            // condition anyway
            auto imageAsset = asset.as<ImageAsset>();
            auto itr = m_imageAssets.find(asset.uniqueName());
            if (itr != m_imageAssets.end())
            {
                auto image = m_server->getImage(itr->second);
                if (image)
                {
                    imageAsset->renderImage(ref_rcp(image));
                    return true;
                }
                return false;
            }
        }

        else if (asset.is<AudioAsset>())
        {
            auto audioAsset = asset.as<AudioAsset>();
            auto itr = m_audioAssets.find(asset.uniqueName());
            if (itr != m_audioAssets.end())
            {
                auto audioSource = m_server->getAudioSource(itr->second);
                if (audioSource)
                {
                    audioAsset->audioSource(ref_rcp(audioSource));
                    return true;
                }
                return false;
            }
        }

        else if (asset.is<FontAsset>())
        {
            auto fontAsset = asset.as<FontAsset>();
            auto itr = m_fontAssets.find(asset.uniqueName());
            if (itr != m_fontAssets.end())
            {
                auto font = m_server->getFont(itr->second);
                if (font)
                {
                    fontAsset->font(ref_rcp(font));
                    return true;
                }
                return false;
            }
        }

        else
        {
            RIVE_UNREACHABLE();
        }

        return false;
    }

    void addRenderImage(std::string name, RenderImageHandle handle)
    {
        m_imageAssets[name] = handle;
    }

    void addAudioSource(std::string name, AudioSourceHandle handle)
    {
        m_audioAssets[name] = handle;
    }

    void addFont(std::string name, FontHandle handle)
    {
        m_fontAssets[name] = handle;
    }

    void removeRenderImage(RenderImageHandle handle)
    {
        using ItrType = std::pair<std::string, RenderImageHandle>;
        auto itr = std::find_if(
            m_imageAssets.begin(),
            m_imageAssets.end(),
            [handle](const ItrType& p) { return p.second == handle; });
        if (itr != m_imageAssets.end())
            m_imageAssets.erase(itr);
    }

    void removeAudioSource(AudioSourceHandle handle)
    {
        using ItrType = std::pair<std::string, AudioSourceHandle>;
        auto itr = std::find_if(
            m_audioAssets.begin(),
            m_audioAssets.end(),
            [handle](const ItrType& p) { return p.second == handle; });
        if (itr != m_audioAssets.end())
            m_audioAssets.erase(itr);
    }

    void removeFont(FontHandle handle)
    {
        using ItrType = std::pair<std::string, FontHandle>;
        auto itr = std::find_if(
            m_fontAssets.begin(),
            m_fontAssets.end(),
            [handle](const ItrType& p) { return p.second == handle; });
        if (itr != m_fontAssets.end())
            m_fontAssets.erase(itr);
    }

    void removeRenderImage(std::string name) { m_imageAssets.erase(name); }
    void removeAudioSource(std::string name) { m_audioAssets.erase(name); }
    void removeFont(std::string name) { m_fontAssets.erase(name); }

#ifdef TESTING
    RenderImageHandle testing_imageNamed(std::string name)
    {
        auto itr = m_imageAssets.find(name);
        if (itr != m_imageAssets.end())
            return itr->second;
        return nullptr;
    }

    AudioSourceHandle testing_audioNamed(std::string name)
    {
        auto itr = m_audioAssets.find(name);
        if (itr != m_audioAssets.end())
            return itr->second;
        return nullptr;
    }

    FontHandle testing_fontNamed(std::string name)
    {
        auto itr = m_fontAssets.find(name);
        if (itr != m_fontAssets.end())
            return itr->second;
        return nullptr;
    }
#endif

private:
    const CommandServer* m_server;

    std::unordered_map<std::string, RenderImageHandle> m_imageAssets;
    std::unordered_map<std::string, AudioSourceHandle> m_audioAssets;
    std::unordered_map<std::string, FontHandle> m_fontAssets;
};

std::ostream& operator<<(std::ostream& os, DataType t)
{
    switch (t)
    {
        case DataType::none:
            os << "None";
            break;
        case DataType::string:
            os << "String";
            break;
        case DataType::number:
            os << "Number";
            break;
        case DataType::boolean:
            os << "Boolean";
            break;
        case DataType::color:
            os << "Color";
            break;
        case DataType::list:
            os << "List";
            break;
        case DataType::enumType:
            os << "Enum";
            break;
        case DataType::trigger:
            os << "Trigger";
            break;
        case DataType::viewModel:
            os << "View Model";
            break;
        case DataType::integer:
            os << "Integer";
            break;
        case DataType::symbolListIndex:
            os << "Symbol List Index";
            break;
        case DataType::assetImage:
            os << "Asset Image";
            break;
        default:
            os << "Unknown DataType";
            break;
    }
    return os;
}

#ifdef TESTING

RenderImageHandle CommandServer::testing_globalImageNamed(std::string name)
{
    return m_fileAssetLoader->testing_imageNamed(name);
}

AudioSourceHandle CommandServer::testing_globalAudioNamed(std::string name)
{
    return m_fileAssetLoader->testing_audioNamed(name);
}

FontHandle CommandServer::testing_globalFontNamed(std::string name)
{
    return m_fileAssetLoader->testing_fontNamed(name);
}

bool CommandServer::testing_globalImageContains(std::string name)
{
    return m_fileAssetLoader->testing_imageNamed(name) != nullptr;
}

bool CommandServer::testing_globalAudioContains(std::string name)
{
    return m_fileAssetLoader->testing_audioNamed(name) != nullptr;
}

bool CommandServer::testing_globalFontContains(std::string name)
{
    return m_fileAssetLoader->testing_fontNamed(name) != nullptr;
}

#endif

CommandServer::CommandServer(rcp<CommandQueue> commandBuffer,
                             Factory* factory) :
    m_commandQueue(std::move(commandBuffer)),
    m_factory(factory),
#ifndef NDEBUG
    m_threadID(std::this_thread::get_id()),
#endif
    m_fileAssetLoader(make_rcp<CommandFileAssetLoader>(this))
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

AudioSource* CommandServer::getAudioSource(AudioSourceHandle handle) const
{
    assert(std::this_thread::get_id() == m_threadID);
    auto it = m_audioSources.find(handle);
    return it != m_audioSources.end() ? it->second.get() : nullptr;
}

Font* CommandServer::getFont(FontHandle handle) const
{
    assert(std::this_thread::get_id() == m_threadID);
    auto it = m_fonts.find(handle);
    return it != m_fonts.end() ? it->second.get() : nullptr;
}

ArtboardInstance* CommandServer::getArtboardInstance(
    ArtboardHandle handle) const
{
    assert(std::this_thread::get_id() == m_threadID);
    auto it = m_artboards.find(handle);
    return it != m_artboards.end() ? it->second.get()->artboard() : nullptr;
}

rcp<BindableArtboard> CommandServer::getBindableArtboard(
    ArtboardHandle handle) const
{
    assert(std::this_thread::get_id() == m_threadID);
    auto it = m_artboards.find(handle);
    return it != m_artboards.end() ? it->second : nullptr;
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
                            ErrorReporter<ViewModelInstanceHandle>(
                                this,
                                handle,
                                requestId,
                                CommandQueue::Message::viewModelError)
                                << "ERROR : Invalid data type {"
                                << data.metaData.type << "} when checking"
                                << "subscriptions";
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
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_byteVectors >> rivBytes;
                lock.unlock();
                rcp<rive::File> file = rive::File::import(rivBytes,
                                                          m_factory,
                                                          nullptr,
                                                          m_fileAssetLoader);
                if (file != nullptr)
                {
                    m_fileDependencies[handle] = {};
                    m_files[handle] = file;

                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::fileLoaded;
                    messageStream << handle;
                    messageStream << requestId;
                }
                else
                {
                    ErrorReporter<FileHandle>(this,
                                              handle,
                                              requestId,
                                              CommandQueue::Message::fileError)
                        << "failed to load Rive file.";
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
                auto itr = m_fileDependencies.find(handle);
                if (itr != m_fileDependencies.end())
                {
                    auto& artboardVector = itr->second;
                    for (auto artboardHandle : artboardVector)
                    {
                        cleanupArtboard(artboardHandle, requestId);
                    }

                    m_fileDependencies.erase(itr);
                }
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
                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::imageDecoded;
                    messageStream << handle;
                    messageStream << requestId;
                }
                else
                {
                    ErrorReporter<RenderImageHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::imageError)
                        << "Command Server failed to decode image";
                }

                break;
            }

            case CommandQueue::Command::externalImage:
            {
                RenderImageHandle handle;
                uint64_t requestId;
                rcp<RenderImage> image;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_externalImages >> image;
                lock.unlock();

                if (image)
                {
                    m_images[handle] = std::move(image);
                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::imageDecoded;
                    messageStream << handle;
                    messageStream << requestId;
                }
                else
                {
                    ErrorReporter<RenderImageHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::imageError)
                        << "External image was empty";
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
                m_fileAssetLoader->removeRenderImage(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                messageStream << CommandQueue::Message::imageDeleted;
                messageStream << handle;
                messageStream << requestId;
                break;
            }

            case CommandQueue::Command::decodeAudio:
            {
                AudioSourceHandle handle;
                uint64_t requestId;
                std::vector<uint8_t> bytes;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_byteVectors >> bytes;
                lock.unlock();

                auto audio = factory()->decodeAudio(bytes);
                if (audio)
                {
                    m_audioSources[handle] = std::move(audio);
                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::audioDecoded;
                    messageStream << handle;
                    messageStream << requestId;
                }
                else
                {
                    ErrorReporter<AudioSourceHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::audioError)
                        << "Command Server failed to decode audio";
                }

                break;
            }

            case CommandQueue::Command::externalAudio:
            {
                AudioSourceHandle handle;
                uint64_t requestId;
                rcp<AudioSource> audio;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_externalAudioSources >> audio;
                lock.unlock();

                if (audio)
                {
                    m_audioSources[handle] = std::move(audio);
                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::audioDecoded;
                    messageStream << handle;
                    messageStream << requestId;
                }
                else
                {
                    ErrorReporter<AudioSourceHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::audioError)
                        << "External audio source was invalid";
                }

                break;
            }

            case CommandQueue::Command::deleteAudio:
            {
                AudioSourceHandle handle;
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                m_audioSources.erase(handle);
                m_fileAssetLoader->removeAudioSource(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                messageStream << CommandQueue::Message::audioDeleted;
                messageStream << handle;
                messageStream << requestId;
                break;
            }

            case CommandQueue::Command::decodeFont:
            {
                FontHandle handle;
                uint64_t requestId;
                std::vector<uint8_t> bytes;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_byteVectors >> bytes;
                lock.unlock();

                auto font = factory()->decodeFont(bytes);
                if (font)
                {
                    m_fonts[handle] = std::move(font);
                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::fontDecoded;
                    messageStream << handle;
                    messageStream << requestId;
                }
                else
                {
                    ErrorReporter<FontHandle>(this,
                                              handle,
                                              requestId,
                                              CommandQueue::Message::fontError)
                        << "Command Server failed to decode font";
                }

                break;
            }

            case CommandQueue::Command::externalFont:
            {
                FontHandle handle;
                uint64_t requestId;
                rcp<Font> font;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_externalFonts >> font;
                lock.unlock();

                if (font)
                {
                    m_fonts[handle] = std::move(font);
                    std::unique_lock<std::mutex> messageLock(
                        m_commandQueue->m_messageMutex);
                    messageStream << CommandQueue::Message::fontDecoded;
                    messageStream << handle;
                    messageStream << requestId;
                }
                else
                {
                    ErrorReporter<FontHandle>(this,
                                              handle,
                                              requestId,
                                              CommandQueue::Message::fontError)
                        << "Command Server failed to decode font";
                }

                break;
            }

            case CommandQueue::Command::deleteFont:
            {
                FontHandle handle;
                uint64_t requestId;
                commandStream >> handle;
                commandStream >> requestId;
                lock.unlock();
                m_fonts.erase(handle);
                m_fileAssetLoader->removeFont(handle);
                std::unique_lock<std::mutex> messageLock(
                    m_commandQueue->m_messageMutex);
                messageStream << CommandQueue::Message::fontDeleted;
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
                                            ? file->bindableArtboardDefault()
                                            : file->bindableArtboardNamed(name))
                    {
                        m_artboardDependencies[handle] = {};
                        m_artboards[handle] = std::move(artboard);
                    }
                    else
                    {
                        ErrorReporter<FileHandle>(
                            this,
                            fileHandle,
                            requestId,
                            CommandQueue::Message::fileError)
                            << "artboard \"" << name << "\" not found.";
                    }
                }
                else
                {
                    ErrorReporter<FileHandle>(this,
                                              fileHandle,
                                              requestId,
                                              CommandQueue::Message::fileError)
                        << "file " << fileHandle
                        << " not found when trying to create artboard";
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
                cleanupArtboard(handle, requestId);
                // We don't remove from the file dependencies here because
                // calling erase on a non existent key is fine.
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
                            if (usesInstanceName)
                            {
                                if (viewModelInstanceName.empty())
                                {
                                    ErrorReporter<FileHandle>(
                                        this,
                                        fileHandle,
                                        requestId,
                                        CommandQueue::Message::fileError)
                                        << "ArtboardInstance " << artboardHandle
                                        << " Not found when trying to create"
                                        << " default view model with default "
                                           "view "
                                        << "model instance";
                                }
                                else
                                {
                                    ErrorReporter<FileHandle>(
                                        this,
                                        fileHandle,
                                        requestId,
                                        CommandQueue::Message::fileError)
                                        << "ArtboardInstance " << artboardHandle
                                        << " Not found when trying to create "
                                        << "default view model with "
                                        << "view model instance name "
                                        << viewModelInstanceName;
                                }
                            }
                            else
                            {
                                ErrorReporter<FileHandle>(
                                    this,
                                    fileHandle,
                                    requestId,
                                    CommandQueue::Message::fileError)
                                    << "ArtboardInstance " << artboardHandle
                                    << " Not found when trying to create "
                                    << "default view model with blank view "
                                    << "model instance";
                            }
                        }
                    }
                    else
                    {
                        viewModel = file->viewModelByName(viewModelName);
                        if (viewModel == nullptr)
                        {
                            ErrorReporter<FileHandle>(
                                this,
                                fileHandle,
                                requestId,
                                CommandQueue::Message::fileError)
                                << "View model " << viewModelName
                                << " not found";
                        }
                    }

                    if (viewModel)
                    {
                        rcp<ViewModelInstanceRuntime> instance;

                        if (usesInstanceName)
                        {
                            if (viewModelInstanceName.empty())
                            {
                                instance = viewModel->createDefaultInstance();
                                if (instance == nullptr)
                                {
                                    ErrorReporter<FileHandle>(
                                        this,
                                        fileHandle,
                                        requestId,
                                        CommandQueue::Message::fileError)
                                        << "Could not create "
                                        << "default view model instance "
                                        << "from view model " << viewModelName;
                                }
                            }
                            else
                            {
                                instance = viewModel->createInstanceFromName(
                                    viewModelInstanceName);
                                if (instance == nullptr)
                                {
                                    ErrorReporter<FileHandle>(
                                        this,
                                        fileHandle,
                                        requestId,
                                        CommandQueue::Message::fileError)
                                        << "Could not create "
                                           "view model instance named "
                                        << viewModelInstanceName
                                        << "from view model " << viewModelName;
                                }
                            }
                        }
                        else
                        {
                            instance = viewModel->createInstance();
                            if (instance == nullptr)
                            {
                                ErrorReporter<FileHandle>(
                                    this,
                                    fileHandle,
                                    requestId,
                                    CommandQueue::Message::fileError)
                                    << "Could not create "
                                    << "blank view model instance "
                                    << "from view model " << viewModelName;
                            }
                        }

                        if (instance)
                        {
                            m_viewModels[viewHandle] = std::move(instance);
                        }
                    }
                }
                else
                {
                    ErrorReporter<FileHandle>(this,
                                              fileHandle,
                                              requestId,
                                              CommandQueue::Message::fileError)
                        << "File " << fileHandle
                        << " not found when creating view model instance ";
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
                            ErrorReporter<ViewModelInstanceHandle>(
                                this,
                                rootHandle,
                                requestId,
                                CommandQueue::Message::viewModelError)
                                << "failed to find list at path " << path
                                << " when trying to add to a list";
                        }
                    }
                    else
                    {
                        ErrorReporter<ViewModelInstanceHandle>(
                            this,
                            rootHandle,
                            requestId,
                            CommandQueue::Message::viewModelError)
                            << "failed to find value view model " << viewHandle
                            << "isntance for add list";
                    }
                }
                else
                {
                    ErrorReporter<ViewModelInstanceHandle>(
                        this,
                        rootHandle,
                        requestId,
                        CommandQueue::Message::viewModelError)
                        << "failed to find root view model isntance "
                        << rootHandle << "for add list";
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
                        ErrorReporter<ViewModelInstanceHandle>(
                            this,
                            rootHandle,
                            requestId,
                            CommandQueue::Message::viewModelError)
                            << "failed to find list on view model "
                               "isntance for "
                               "remove at path "
                            << path;
                    }
                }
                else
                {
                    ErrorReporter<ViewModelInstanceHandle>(
                        this,
                        rootHandle,
                        requestId,
                        CommandQueue::Message::viewModelError)
                        << "failed to find view model instance " << rootHandle
                        << " for remove list";
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
                        ErrorReporter<ViewModelInstanceHandle>(
                            this,
                            rootHandle,
                            requestId,
                            CommandQueue::Message::viewModelError)
                            << "failed to find list on view model "
                               "isntance for "
                               "swap at path "
                            << path;
                    }
                }
                else
                {
                    ErrorReporter<ViewModelInstanceHandle>(
                        this,
                        rootHandle,
                        requestId,
                        CommandQueue::Message::viewModelError)
                        << "failed to find view model instance " << rootHandle
                        << " for swap";
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    rootHandle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Property " << data.name
                                    << " not found when subscribing";
                            }
                        }
                        else
                        {
                            ErrorReporter<ViewModelInstanceHandle>(
                                this,
                                rootHandle,
                                requestId,
                                CommandQueue::Message::viewModelError)
                                << "Property type " << data.type
                                << " is not valid when "
                                << "subscribing";
                        }
                    }
                    else
                    {
                        ErrorReporter<ViewModelInstanceHandle>(
                            this,
                            rootHandle,
                            requestId,
                            CommandQueue::Message::viewModelError)
                            << "Root view model " << rootHandle
                            << " not found when subscribing "
                               "to property "
                            << data.name;
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
                        m_viewModels[nestedViewHandle] = nestedViewModel;
                    }
                    else
                    {
                        ErrorReporter<ViewModelInstanceHandle>(
                            this,
                            nestedViewHandle,
                            requestId,
                            CommandQueue::Message::viewModelError)
                            << "Nested view not found at path" << path
                            << " when refing nested "
                               "view model";
                    }
                }
                else
                {
                    ErrorReporter<ViewModelInstanceHandle>(
                        this,
                        nestedViewHandle,
                        requestId,
                        CommandQueue::Message::viewModelError)
                        << "Root view model " << rootViewHandle
                        << " not found when refing nested "
                           "view model at path "
                        << path;
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
                            m_viewModels[listViewHandle] = viewModelInstance;
                        }
                        else
                        {
                            ErrorReporter<ViewModelInstanceHandle>(
                                this,
                                rootViewHandle,
                                requestId,
                                CommandQueue::Message::viewModelError)
                                << "View model not found on list " << path
                                << " at index " << index;
                        }
                    }
                    else
                    {
                        ErrorReporter<ViewModelInstanceHandle>(
                            this,
                            rootViewHandle,
                            requestId,
                            CommandQueue::Message::viewModelError)
                            << "List not found at path " << path
                            << " when refing  view model at index " << index;
                    }
                }
                else
                {
                    ErrorReporter<ViewModelInstanceHandle>(
                        this,
                        rootViewHandle,
                        requestId,
                        CommandQueue::Message::viewModelError)
                        << "Root view model" << rootViewHandle
                        << " not found when refing nested "
                           "view model at path "
                        << path;
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
                        assert(m_artboardDependencies.find(artboardHandle) !=
                               m_artboardDependencies.end());
                        m_artboardDependencies[artboardHandle].push_back(
                            handle);
                    }
                    else
                    {
                        ErrorReporter<ArtboardHandle>(
                            this,
                            artboardHandle,
                            requestId,
                            CommandQueue::Message::artboardError)
                            << "Could not create state "
                               "machine with name \""
                            << name << "\" because it was not found.";
                    }
                }
                else
                {
                    ErrorReporter<ArtboardHandle>(
                        this,
                        artboardHandle,
                        requestId,
                        CommandQueue::Message::artboardError)
                        << "Could not create state "
                           "machine with name \""
                        << name << "\" because the owning artboard "
                        << artboardHandle << " was not found.";
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
                        ErrorReporter<StateMachineHandle>(
                            this,
                            handle,
                            requestId,
                            CommandQueue::Message::stateMachineError)
                            << "View model instance " << viewModel
                            << " not found for when trying to bind "
                               "to a state machine";
                    }
                }
                else
                {
                    ErrorReporter<StateMachineHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::stateMachineError)
                        << "State machine " << handle
                        << " not found for binding view model.";
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
                    ErrorReporter<StateMachineHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::stateMachineError)
                        << "State machine " << handle
                        << " not found for "
                           "advance.";
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
                // We don't remove from the artboard dependencies here because
                // calling erase on a non existent key is fine.
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
                else
                {
                    ErrorReporter<FileHandle>(this,
                                              handle,
                                              requestId,
                                              CommandQueue::Message::fileError)
                        << "Invalid file handle " << handle
                        << " when getting list of artboards";
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
                else
                {
                    ErrorReporter<FileHandle>(this,
                                              handle,
                                              requestId,
                                              CommandQueue::Message::fileError)
                        << "Invalid file handle " << handle
                        << " when getting list of enums";
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
                else
                {
                    ErrorReporter<ArtboardHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::artboardError)
                        << "Invalid artboard handle " << handle
                        << " when getting list of state machines";
                }
                break;
            }

            case CommandQueue::Command::getDefaultViewModel:
            {
                FileHandle fileHandle;
                ArtboardHandle artboardHandle;
                uint64_t requestId;
                commandStream >> fileHandle;
                commandStream >> artboardHandle;
                commandStream >> requestId;
                lock.unlock();
                auto artboard = getArtboardInstance(artboardHandle);
                if (artboard)
                {
                    auto file = getFile(fileHandle);
                    if (file)
                    {
                        auto defaultViewModel =
                            file->defaultArtboardViewModel(artboard);

                        if (defaultViewModel)
                        {
                            auto defaultInstance =
                                defaultViewModel->createDefaultInstance();
                            if (defaultInstance)
                            {
                                std::unique_lock<std::mutex> messageLock(
                                    m_commandQueue->m_messageMutex);
                                messageStream << CommandQueue::Message::
                                        defaultViewModelReceived;
                                messageStream << artboardHandle;
                                messageStream << requestId;
                                m_commandQueue->m_messageNames
                                    << defaultViewModel->name();
                                m_commandQueue->m_messageNames
                                    << defaultInstance->name();
                            }
                            else
                            {
                                ErrorReporter<ArtboardHandle>(
                                    this,
                                    artboardHandle,
                                    requestId,
                                    CommandQueue::Message::artboardError)
                                    << "Could not find default view model "
                                       "instance for "
                                       "artboard"
                                    << " when getting default view model info";
                            }
                        }
                        else
                        {
                            ErrorReporter<ArtboardHandle>(
                                this,
                                artboardHandle,
                                requestId,
                                CommandQueue::Message::artboardError)
                                << "Could not find default view model for "
                                   "artboard"
                                << " when getting default view model info";
                        }
                    }
                    else
                    {
                        ErrorReporter<ArtboardHandle>(
                            this,
                            artboardHandle,
                            requestId,
                            CommandQueue::Message::artboardError)
                            << "Invalid file handle " << fileHandle
                            << " when getting default view model info";
                    }
                }
                else
                {
                    ErrorReporter<ArtboardHandle>(
                        this,
                        artboardHandle,
                        requestId,
                        CommandQueue::Message::artboardError)
                        << "Invalid artboard handle " << artboardHandle
                        << " when getting default view model info";
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
                else
                {
                    ErrorReporter<FileHandle>(this,
                                              handle,
                                              requestId,
                                              CommandQueue::Message::fileError)
                        << "Invalid file handle " << handle
                        << " when getting list of view models";
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
                    else
                    {
                        ErrorReporter<FileHandle>(
                            this,
                            handle,
                            requestId,
                            CommandQueue::Message::viewModelError)
                            << "Invalid view model name " << viewModelName
                            << " when getting list of view model instance "
                               "names";
                    }
                }
                else
                {
                    ErrorReporter<FileHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::viewModelError)
                        << "Invalid file handle " << handle
                        << " when getting list of view model instance names";
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
                        // Used to get meta data about enums.
                        auto modelInstance = model->createDefaultInstance();
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
                            if (property.type == DataType::enumType)
                            {
                                auto enumProperty =
                                    modelInstance->propertyEnum(property.name);
                                m_commandQueue->m_messageNames
                                    << enumProperty->enumType();
                            }
                            if (property.type == DataType::viewModel)
                            {
                                // Get the type of the view model property
                                auto viewModelTmp =
                                    modelInstance->propertyViewModel(
                                        property.name);
                                if (viewModelTmp)
                                {
                                    m_commandQueue->m_messageNames
                                        << viewModelTmp->instance()
                                               ->viewModel()
                                               ->name();
                                }
                                else
                                {
                                    // If we can't determine the name we still
                                    // need to send something because the
                                    // command queue is expecting a name.
                                    m_commandQueue->m_messageNames << "Unkown";
                                }
                            }
                        }
                    }
                    else
                    {
                        ErrorReporter<FileHandle>(
                            this,
                            handle,
                            requestId,
                            CommandQueue::Message::fileError)
                            << "Invalid view model name " << viewModelName
                            << " when getting list of view model properties";
                    }
                }
                else
                {
                    ErrorReporter<FileHandle>(this,
                                              handle,
                                              requestId,
                                              CommandQueue::Message::fileError)
                        << "Invalid file handle " << handle
                        << " when getting list of view model properties";
                }
                break;
            }

            case CommandQueue::Command::setViewModelInstanceValue:
            {
                ViewModelInstanceHandle handle = RIVE_NULL_HANDLE;
                ViewModelInstanceHandle nestedHandle = RIVE_NULL_HANDLE;
                RenderImageHandle imageHandle = RIVE_NULL_HANDLE;
                ArtboardHandle artboadHandle = RIVE_NULL_HANDLE;
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
                    case DataType::artboard:
                        commandStream >> artboadHandle;
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when setting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when setting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when setting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when setting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when setting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
                            }
                            break;
                        }
                        case DataType::enumType:
                        {
                            if (auto property = viewModelInstance->propertyEnum(
                                    value.metaData.name))
                            {
                                // As far as I can tell, there is no built in
                                // way to do this from the core runtime. So we
                                // just verify manually ourselves.
                                auto values = property->values();
                                if (std::find(values.begin(),
                                              values.end(),
                                              value.stringValue) !=
                                    values.end())
                                {
                                    property->value(value.stringValue);
                                }
                                else
                                {
                                    ErrorReporter<ViewModelInstanceHandle>(
                                        this,
                                        handle,
                                        requestId,
                                        CommandQueue::Message::viewModelError)
                                        << "Invalid enum value for "
                                           "property "
                                        << value.metaData.name
                                        << " when trying to set enum to "
                                        << value.stringValue
                                        << " possible values " << values;
                                }
                            }
                            else
                            {
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when setting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
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
                                    ErrorReporter<ViewModelInstanceHandle>(
                                        this,
                                        handle,
                                        requestId,
                                        CommandQueue::Message::viewModelError)
                                        << "Could not replace "
                                           "view model at path "
                                        << value.metaData.name;
                                }
                            }
                            else
                            {
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find nested view "
                                       "model "
                                       "with handle "
                                    << nestedHandle
                                    << " to set for view model instance when "
                                       "setting property with path "
                                    << value.metaData.name;
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
                                    ErrorReporter<ViewModelInstanceHandle>(
                                        this,
                                        handle,
                                        requestId,
                                        CommandQueue::Message::viewModelError)
                                        << "Could not find "
                                           "image property at path "
                                        << value.metaData.name;
                                }
                            }
                            else
                            {
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find image " << imageHandle
                                    << " to set for view model instance when "
                                       "setting property with path "
                                    << value.metaData.name;
                            }
                            break;
                        }
                        case DataType::artboard:
                        {
                            if (auto bindableArtboard =
                                    getBindableArtboard(artboadHandle))
                            {
                                if (auto artboardProperty =
                                        viewModelInstance->propertyArtboard(
                                            value.metaData.name))
                                {
                                    artboardProperty->value(bindableArtboard);
                                }
                                else
                                {
                                    ErrorReporter<ViewModelInstanceHandle>(
                                        this,
                                        handle,
                                        requestId,
                                        CommandQueue::Message::viewModelError)
                                        << "Could not find "
                                           "artboard property at path "
                                        << value.metaData.name;
                                }
                            }
                            else
                            {
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find artboard "
                                    << artboadHandle
                                    << " to set for view model instance when "
                                       "setting property with path "
                                    << value.metaData.name;
                            }
                            break;
                        }
                        default:
                            RIVE_UNREACHABLE();
                    }
                }
                else
                {
                    ErrorReporter<ViewModelInstanceHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::viewModelError)
                        << "Could not find view model instance when "
                           "setting property type "
                        << value.metaData.type << " with path "
                        << value.metaData.name;
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when getting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when getting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when getting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when getting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
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
                                ErrorReporter<ViewModelInstanceHandle>(
                                    this,
                                    handle,
                                    requestId,
                                    CommandQueue::Message::viewModelError)
                                    << "Could not find view model "
                                       "property "
                                       "instance when getting property type "
                                    << value.metaData.type << " with path "
                                    << value.metaData.name;
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
                    ErrorReporter<ViewModelInstanceHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::viewModelError)
                        << "Could not find view model instance when "
                           "getting property type "
                        << value.metaData.type << " with path "
                        << value.metaData.name;
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
                        ErrorReporter<ViewModelInstanceHandle>(
                            this,
                            handle,
                            requestId,
                            CommandQueue::Message::viewModelError)
                            << "failed to get list at path " << path
                            << " when getting list size";
                    }
                }
                else
                {
                    ErrorReporter<ViewModelInstanceHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::viewModelError)
                        << "failed to get view model " << handle
                        << " when getting list size";
                }

                break;
            }

            case CommandQueue::Command::pointerMove:
            {
                StateMachineHandle handle;
                uint64_t requestId;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                commandStream >> requestId;
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
                    ErrorReporter<StateMachineHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::stateMachineError)
                        << "State machine \"" << handle
                        << "\" not found for pointerMove.";
                }
                break;
            }

            case CommandQueue::Command::pointerDown:
            {
                StateMachineHandle handle;
                uint64_t requestId;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_pointerEvents >> pointerEvent;
                lock.unlock();
                if (auto stateMachine = getStateMachineInstance(handle))
                {
                    Vec2D position =
                        cursorPosForPointerEvent(stateMachine, pointerEvent);
                    stateMachine->pointerDown(position, 0);
                }
                else
                {
                    ErrorReporter<StateMachineHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::stateMachineError)
                        << "State machine \"" << handle
                        << "\" not found for pointerDown.";
                }
                break;
            }

            case CommandQueue::Command::pointerUp:
            {
                StateMachineHandle handle;
                uint64_t requestId;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_pointerEvents >> pointerEvent;
                lock.unlock();
                if (auto stateMachine = getStateMachineInstance(handle))
                {
                    Vec2D position =
                        cursorPosForPointerEvent(stateMachine, pointerEvent);
                    stateMachine->pointerUp(position, 0);
                }
                else
                {
                    ErrorReporter<StateMachineHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::stateMachineError)
                        << "State machine \"" << handle
                        << "\" not found for pointerUp.";
                }
                break;
            }

            case CommandQueue::Command::pointerExit:
            {
                StateMachineHandle handle;
                uint64_t requestId;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_pointerEvents >> pointerEvent;
                lock.unlock();
                if (auto stateMachine = getStateMachineInstance(handle))
                {
                    Vec2D position =
                        cursorPosForPointerEvent(stateMachine, pointerEvent);
                    stateMachine->pointerExit(position, 0);
                }
                else
                {
                    ErrorReporter<StateMachineHandle>(
                        this,
                        handle,
                        requestId,
                        CommandQueue::Message::stateMachineError)
                        << "State machine \"" << handle
                        << "\" not found for pointerExit.";
                }
                break;
            }

            case CommandQueue::Command::addImageFileAsset:
            {
                RenderImageHandle handle;
                std::string name;
                uint64_t requestId;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_names >> name;
                lock.unlock();
                if (handle && getImage(handle) != nullptr)
                {
                    m_fileAssetLoader->addRenderImage(std::move(name), handle);
                }
                break;
            }

            case CommandQueue::Command::removeImageFileAsset:
            {
                std::string name;
                uint64_t requestId;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> requestId;
                m_commandQueue->m_names >> name;
                lock.unlock();
                m_fileAssetLoader->removeRenderImage(std::move(name));
                break;
            }

            case CommandQueue::Command::addAudioFileAsset:
            {
                AudioSourceHandle handle;
                std::string name;
                uint64_t requestId;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_names >> name;
                lock.unlock();
                if (handle && getAudioSource(handle) != nullptr)
                {
                    m_fileAssetLoader->addAudioSource(std::move(name), handle);
                }
                break;
            }

            case CommandQueue::Command::removeAudioFileAsset:
            {
                std::string name;
                uint64_t requestId;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> requestId;
                m_commandQueue->m_names >> name;
                lock.unlock();
                m_fileAssetLoader->removeAudioSource(std::move(name));
                break;
            }

            case CommandQueue::Command::addFontFileAsset:
            {
                FontHandle handle;
                std::string name;
                uint64_t requestId;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> handle;
                commandStream >> requestId;
                m_commandQueue->m_names >> name;
                lock.unlock();
                if (handle && getFont(handle) != nullptr)
                {
                    m_fileAssetLoader->addFont(std::move(name), handle);
                }
                break;
            }

            case CommandQueue::Command::removeFontFileAsset:
            {
                std::string name;
                uint64_t requestId;
                CommandQueue::PointerEvent pointerEvent;
                commandStream >> requestId;
                m_commandQueue->m_names >> name;
                lock.unlock();
                m_fileAssetLoader->removeFont(std::move(name));
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

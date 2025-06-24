/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/command_queue.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <type_traits>

namespace rive
{

// Server-side worker that executes commands from a CommandQueue.
class CommandServer
{
public:
    CommandServer(rcp<CommandQueue>, Factory*);
    virtual ~CommandServer();

    Factory* factory() const { return m_factory; }

    File* getFile(FileHandle) const;
    bool getWasDisconnected() const { return m_wasDisconnectReceived; }
    RenderImage* getImage(RenderImageHandle) const;
    AudioSource* getAudioSource(AudioSourceHandle) const;
    Font* getFont(FontHandle) const;
    ArtboardInstance* getArtboardInstance(ArtboardHandle) const;
    StateMachineInstance* getStateMachineInstance(StateMachineHandle) const;
    ViewModelInstanceRuntime* getViewModelInstance(
        ViewModelInstanceHandle) const;
    // Wait for queue to not be empty, then returns pollMessages.
    bool waitCommands();
    // Returns imidiatly after checking messages. If there are none just returns
    // returns !m_wasDisconnectReceived.
    bool processCommands();
    // Blocks and runs waitMessages until disconnect is received.
    void serveUntilDisconnect();

    struct Subscription
    {
        // The request Id for sbuscribing to this particular property.
        uint64_t requestId;
        // Information about the property we want to "subsribe" to.
        PropertyData data;
        // The root view model of from the perspective of the path in data.name.
        ViewModelInstanceHandle rootViewModel;
    };

#ifdef TESTING
    // Expose cursorPosForPointerEvent for testing.
    Vec2D testing_cursorPosForPointerEvent(StateMachineInstance* instance,
                                           CommandQueue::PointerEvent event)
    {
        return cursorPosForPointerEvent(instance, event);
    }

    const std::vector<Subscription>& testing_getSubsciptions() const
    {
        return m_propertySubscriptions;
    }

    RenderImageHandle testing_globalImageNamed(std::string name);
    AudioSourceHandle testing_globalAudioNamed(std::string name);
    FontHandle testing_globalFontNamed(std::string name);

    bool testing_globalImageContains(std::string name);
    bool testing_globalAudioContains(std::string name);
    bool testing_globalFontContains(std::string name);
#endif

private:
    friend class CommandQueue;

    template <typename HandleType> class ErrorReporter
    {
    public:
        ErrorReporter(CommandServer* server,
                      HandleType handle,
                      CommandQueue::Message message) :
            m_server(server), m_lock(m_server->m_commandQueue->m_messageMutex)
        {
            assert(server);
            assert(message >= CommandQueue::Message::fileError &&
                   message <= CommandQueue::Message::stateMachineError);

            std::cerr << "ERROR : ";
            m_server->m_commandQueue->m_messageStream << message;
            m_server->m_commandQueue->m_messageStream << handle;
        }

        ErrorReporter& operator<<(std::vector<std::string> vector)
        {
            m_ostringstream << "{ ";
            for (auto& s : vector)
                m_ostringstream << s << ",";
            m_ostringstream << "} ";
            return *this;
        }

        template <typename T> ErrorReporter& operator<<(const T& t)
        {
            m_ostringstream << t;
            return *this;
        }

        ~ErrorReporter()
        {
            std::string str = m_ostringstream.str();
            assert(!str.empty());
            std::cerr << str << "\n";
            m_server->m_commandQueue->m_messageNames << std::move(str);
        }

        CommandServer* m_server;
        std::unique_lock<std::mutex> m_lock;
        std::ostringstream m_ostringstream;
    };

    void checkPropertySubscriptions();

    Vec2D cursorPosForPointerEvent(StateMachineInstance*,
                                   const CommandQueue::PointerEvent&);

    bool m_wasDisconnectReceived = false;
    const rcp<CommandQueue> m_commandQueue;
    Factory* const m_factory;
#ifndef NDEBUG
    const std::thread::id m_threadID;
#endif

    // Vector to iterate on for subscriptions. This is a vector instead of a map
    // because we iterate through the entire vector every frame anyway.
    std::vector<Subscription> m_propertySubscriptions;

    std::unordered_map<FileHandle, std::unique_ptr<File>> m_files;
    std::unordered_map<FontHandle, rcp<Font>> m_fonts;
    std::unordered_map<RenderImageHandle, rcp<RenderImage>> m_images;
    std::unordered_map<AudioSourceHandle, rcp<AudioSource>> m_audioSources;
    std::unordered_map<ArtboardHandle, std::unique_ptr<ArtboardInstance>>
        m_artboards;
    std::unordered_map<ViewModelInstanceHandle, rcp<ViewModelInstanceRuntime>>
        m_viewModels;
    std::unordered_map<StateMachineHandle,
                       std::unique_ptr<StateMachineInstance>>
        m_stateMachines;

    std::unordered_map<DrawKey, CommandServerDrawCallback> m_uniqueDraws;

    class CommandFileAssetLoader;
    rcp<CommandFileAssetLoader> m_fileAssetLoader;
};
}; // namespace rive

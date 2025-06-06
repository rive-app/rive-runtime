/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/command_queue.hpp"
#include <thread>
#include <unordered_map>

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

#ifdef TESTING
    // Expose cursorPosForPointerEvent for testing.
    Vec2D testing_cursorPosForPointerEvent(StateMachineInstance* instance,
                                           CommandQueue::PointerEvent event)
    {
        return cursorPosForPointerEvent(instance, event);
    }
#endif

private:
    friend class CommandQueue;

    Vec2D cursorPosForPointerEvent(StateMachineInstance*,
                                   const CommandQueue::PointerEvent&);

    bool m_wasDisconnectReceived = false;
    const rcp<CommandQueue> m_commandQueue;
    Factory* const m_factory;
#ifndef NDEBUG
    const std::thread::id m_threadID;
#endif

    std::unordered_map<FileHandle, std::unique_ptr<File>> m_files;
    std::unordered_map<ArtboardHandle, std::unique_ptr<ArtboardInstance>>
        m_artboards;
    std::unordered_map<ViewModelInstanceHandle, rcp<ViewModelInstanceRuntime>>
        m_viewModels;
    std::unordered_map<StateMachineHandle,
                       std::unique_ptr<StateMachineInstance>>
        m_stateMachines;

    std::unordered_map<DrawKey, CommandServerDrawCallback> m_uniqueDraws;
};
}; // namespace rive

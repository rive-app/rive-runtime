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
    // Wait for queue to not be empty, then returns pollMessages.
    bool waitMessages();
    // Returns imidiatly after checking messages. If there are none just returns
    // returns !m_wasDisconnectReceived.
    bool pollMessages();
    // Blocks and runs waitMessages until disconnect is received.
    void serveUntilDisconnect();

private:
    friend class CommandQueue;

    bool m_wasDisconnectReceived = false;
    const rcp<CommandQueue> m_commandBuffer;
    Factory* const m_factory;
#ifndef NDEBUG
    const std::thread::id m_threadID;
#endif

    std::unordered_map<FileHandle, std::unique_ptr<File>> m_files;
    std::unordered_map<ArtboardHandle, std::unique_ptr<ArtboardInstance>>
        m_artboards;
    std::unordered_map<StateMachineHandle,
                       std::unique_ptr<StateMachineInstance>>
        m_stateMachines;

    std::unordered_map<DrawKey, CommandServerDrawCallback> m_uniqueDraws;
};
}; // namespace rive

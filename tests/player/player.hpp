/*
 * Copyright 2024 Rive
 */

#pragma once

#include "common/frame_runner.hpp"
#include "rive/refcnt.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace rive
{
class ArtboardInstance;
class File;
class Scene;
} // namespace rive

class Player : public FrameRunner
{
public:
    enum class InputMode : uint8_t
    {
        chars,
        consolecommands
    };

    Player();
    ~Player() override;

    bool parseArgs(int argc,
                   const char* const argv[],
                   LaunchOptions& options) override;

    void init() override;

    void init(std::string rivName, std::vector<uint8_t> rivBytes);

    bool doFrame() override;

    void keyPressed(char key);

    int monitorIdx() const { return m_monitorIdx; }

private:
    int m_copiesLeft = 0;
    int m_copiesAbove = 0;
    int m_copiesRight = 0;
    int m_copiesBelow = 0;
    int m_rotations90 = 0;
    int m_zoomLevel = 0;
    int m_spacing = 0;
    int m_monitorIdx = 0;
    int m_paintStyle = 0;
    bool m_wireframe = false;
    bool m_paused = false;
    bool m_forceFixedDeltaTime = false;
    bool m_quit = false;
    InputMode m_inputMode = InputMode::chars;
    bool m_hotloadShaders = false;

    int m_keyMultiplier = 0;
    bool m_seenDigit = false;
    bool m_seenBang = false;

    std::vector<uint8_t> m_pendingRivBytes;

    std::string m_rivName;
    rive::rcp<rive::File> m_file;
    std::unique_ptr<rive::ArtboardInstance> m_artboard;
    std::unique_ptr<rive::Scene> m_scene;

    int m_lastReportedCopyCount = 0;
    bool m_lastReportedPauseState = false;

    struct FPSOverlay;
    std::unique_ptr<FPSOverlay> m_fps;

    std::chrono::high_resolution_clock::time_point m_timestampPrevFrame;
};

/*
 * Copyright 2022 Rive
 */

#pragma once

#include "common/frame_runner.hpp"

#include <string>
#include <vector>

class GoldensRunner : public FrameRunner
{
public:
    bool rendersIntoHostTarget() const override { return false; }

    bool parseArgs(int argc,
                   const char* const argv[],
                   LaunchOptions& options) override;

    void init() override;

    bool doFrame() override;

    int exitCode() const { return m_exitCode; }

private:
    int m_cellSize = 0;

    bool m_fromTestHarness = false;

    std::vector<std::string> m_localFiles;
    size_t m_nextFile = 0;

    int m_exitCode = 0;
};

/*
 * Copyright 2022 Rive
 */

#pragma once

#include "common/frame_runner.hpp"

#include <functional>
#include <string>
#include <vector>

namespace rivegm
{
class GM;
}

class GMRunner : public FrameRunner
{
public:
    bool rendersIntoHostTarget() const override { return false; }

    bool parseArgs(int argc,
                   const char* const argv[],
                   LaunchOptions& options) override;

    void init() override;

    bool doFrame() override;

    void setVerbose(bool verbose) { m_verbose = verbose; }

    int parityFailures() const { return m_parityFailures; }

private:
    void dumpGM(rivegm::GM* gm, const std::string& gmName);
    void runParityGM(
        const std::vector<std::function<rivegm::GM*(void)>>& makers,
        const std::string& name);

    bool m_verbose = false;
    int m_loopCount = 1;
    std::string m_match;
    bool m_interactive = false;
    int m_pngThreads = 2;

    size_t m_nextGM = 0;
    size_t m_nextParityGM = 0;

    int m_parityFailures = 0;
    // Zero demands byte identical frames. Atomic backends rasterize in a
    // nondeterministic order run to run, so they get the same small tolerance
    // the golden diffs allow instead of exactness no two of their frames ever
    // had.
    int m_parityMaxChannelDiff = 0;
};

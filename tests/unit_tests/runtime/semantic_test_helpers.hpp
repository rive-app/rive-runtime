/*
 * Copyright 2026 Rive
 */

// Shared helpers for the C++ semantic test suite. Groups the fixture-load +
// bind + settle boilerplate, the tiny diff-query utilities, and the
// per-test constants that were previously duplicated across the five
// semantic_*_test.cpp files.

#ifndef _RIVE_SEMANTIC_TEST_HELPERS_HPP_
#define _RIVE_SEMANTIC_TEST_HELPERS_HPP_

#include <rive/animation/state_machine_instance.hpp>
#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/math/vec2d.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/semantic/semantic_node.hpp>
#include <rive/semantic/semantic_role.hpp>
#include <rive/semantic/semantic_snapshot.hpp>
#include <rive/semantic/semantic_state.hpp>
#include <rive/viewmodel/runtime/viewmodel_instance_runtime.hpp>
#include "rive_file_reader.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace rive
{
namespace semantic_test
{

// Default settle duration: 10 frames at 0.1s each = 1s of simulated time.
// Long enough for layout, transitions, and data-binding-driven list-item
// spawns to reach steady state in the current fixtures.
constexpr int kSettleFrames = 10;
constexpr float kSettleDt = 0.1f;

// Shared fixture labels (authored into .riv; drift here fails tests in
// multiple files at once).
constexpr const char* kDropdownLabel = "Select a fandom";

inline void advance(StateMachineInstance* sm,
                    int frames = kSettleFrames,
                    float dt = kSettleDt)
{
    for (int i = 0; i < frames; ++i)
    {
        sm->advanceAndApply(dt);
    }
}

inline SemanticsDiff settleAndDrainDiff(StateMachineInstance* sm,
                                        int frames = kSettleFrames,
                                        float dt = kSettleDt)
{
    advance(sm, frames, dt);
    auto* mgr = sm->semanticManager();
    return mgr != nullptr ? mgr->drainDiff() : SemanticsDiff{};
}

inline const SemanticsDiffNode* findAddedByLabel(const SemanticsDiff& diff,
                                                 const std::string& label)
{
    for (const auto& n : diff.added)
    {
        if (n.label == label)
        {
            return &n;
        }
    }
    return nullptr;
}

inline const SemanticsDiffNode* findAddedByRole(const SemanticsDiff& diff,
                                                SemanticRole role)
{
    const uint32_t asU = static_cast<uint32_t>(role);
    for (const auto& n : diff.added)
    {
        if (n.role == asU)
        {
            return &n;
        }
    }
    return nullptr;
}

inline Vec2D centerOf(const SemanticsDiffNode& node)
{
    return Vec2D((node.minX + node.maxX) * 0.5f,
                 (node.minY + node.maxY) * 0.5f);
}

inline bool isSelected(const SemanticNode* node)
{
    return node != nullptr &&
           hasSemanticState(node->stateFlags(), SemanticState::Selected);
}

// Holds a loaded .riv + its default artboard, state machine, and bound view
// model instance, with semantics enabled and the tree already settled.
struct Fixture
{
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    std::unique_ptr<StateMachineInstance> sm;
    rcp<ViewModelInstance> vmi;
};

// Load a fixture, enable semantics, bind the default VMI (if any), advance
// to settle. Every fixture-driven test in the suite uses this.
inline Fixture loadFixture(const char* path)
{
    Fixture f;
    f.file = ReadRiveFile(path);
    f.artboard = f.file->artboardDefault();
    f.sm = f.artboard->stateMachineAt(0);
    if (f.sm != nullptr)
    {
        f.sm->enableSemantics();
    }
    f.vmi = f.file->createDefaultViewModelInstance(f.artboard.get());
    if (f.vmi != nullptr)
    {
        f.artboard->bindViewModelInstance(f.vmi);
        if (f.sm != nullptr)
        {
            f.sm->bindViewModelInstance(f.vmi);
        }
    }
    if (f.sm != nullptr)
    {
        advance(f.sm.get());
    }
    return f;
}

} // namespace semantic_test
} // namespace rive

#endif

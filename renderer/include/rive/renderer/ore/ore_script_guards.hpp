#pragma once

// Guard messages shared by the Luau binding and the wasm host, so a script
// author reads the same cause on either lane.
namespace rive::ore
{
constexpr char kGuardSetPipelineBeforeDraw[] =
    "setPipeline must be called before draw";
constexpr char kGuardVertexSlotRangeFormat[] =
    "setVertexBuffer: slot must be 0-%u (got %u)";
constexpr char kGuardBaseVertexFormat[] =
    "%s: baseVertex=%d requires the drawBaseInstance feature, which the "
    "active backend does not support";
constexpr char kGuardFirstInstanceFormat[] =
    "%s: firstInstance=%u requires the drawBaseInstance feature, which the "
    "active backend does not support";
} // namespace rive::ore

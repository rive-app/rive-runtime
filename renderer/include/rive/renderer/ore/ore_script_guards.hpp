#pragma once

#include <cstdint>
#include <string>

// Guard messages shared by the Luau binding and the wasm host, so a script
// author reads the same cause on either lane.
namespace rive::ore
{
class Context;
struct PipelineDesc;
struct SamplerDesc;

constexpr char
    kGuardSetPipelineBeforeDraw[] = "setPipeline must be called before draw";
// The most dynamic offsets one setBindGroup takes.
constexpr uint32_t kMaxDynamicOffsets = 8;
constexpr char kGuardDynamicOffsetCountFormat
    [] = "setBindGroup: dynamicOffsets count %u exceeds maximum of %u";
constexpr char kGuardVertexSlotRangeFormat
    [] = "setVertexBuffer: slot must be 0-%u (got %u)";
constexpr char kGuardVertexLayoutCountFormat
    [] = "GPUPipeline.new: vertexLayout count %u exceeds maximum of %u";
constexpr char kGuardBaseVertexFormat
    [] = "%s: baseVertex=%d requires the drawBaseInstance feature, which the "
         "active backend does not support";
constexpr char kGuardFirstInstanceFormat[] =
    "%s: firstInstance=%u requires the drawBaseInstance feature, which the "
    "active backend does not support";

// Checks both lanes run on what a script hands the device; each returns false
// with the message in outError.

// A power of two, within the device's maximum once its features are known.
bool validateSampleCount(
    const Context* context,
    uint32_t sampleCount,
    std::string* outError);

// An ordered LOD range and a power of two anisotropy up to 16, which needs
// anisotropic filtering once the device's features are known.
bool validateSamplerDesc(
    const Context* context,
    const SamplerDesc& desc,
    std::string* outError);

// Vertex buffers within the slot table and attribute slots within the
// device's count, or the portable default before it is known; a module
// leaves the slots to its host.
bool validateVertexLayout(
    const Context* context,
    const PipelineDesc& desc,
    std::string* outError);
} // namespace rive::ore

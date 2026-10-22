/*
 * Copyright 2026 Rive
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rive::ore
{
constexpr uint32_t kSpirvVertexModel = 0;
constexpr uint32_t kSpirvFragmentModel = 4;

// Buffers a cut reuses, so cutting only allocates while they grow.
struct SpirvEntryScratch
{
  struct Function
  {
    size_t end;
    uint32_t callsBegin;
    uint32_t callsEnd;
  };
  std::vector<uint8_t> flags;
  std::vector<uint32_t> functionIndex;
  std::vector<Function> functions;
  std::vector<uint32_t> calls;
  std::vector<uint32_t> pending;
  std::vector<uint32_t> words;
};

// Reads code at any alignment.
size_t spirvEntryPointCount(const void* code, size_t wordCount);

// Cuts the module down to one entry point into scratch.words, since Adreno
// fails to link multi entry modules. False when it cannot cut safely.
bool spirvEntryModule(
    const uint32_t* words,
    size_t count,
    uint32_t executionModel,
    const char* name,
    SpirvEntryScratch& scratch);
} // namespace rive::ore

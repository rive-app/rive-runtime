/*
 * Copyright 2026 Rive
 */

#include "ore_spirv_entry_split.hpp"

#include <cstring>

namespace rive::ore
{
namespace
{
enum Op : uint32_t
{
  OpUndef = 1,
  OpName = 5,
  OpMemberName = 6,
  OpExtInstImport = 11,
  OpEntryPoint = 15,
  OpExecutionMode = 16,
  OpCapability = 17,
  OpTypeVoid = 19,
  OpTypePipe = 38,
  OpConstantTrue = 41,
  OpSpecConstantOp = 52,
  OpFunction = 54,
  OpFunctionEnd = 56,
  OpFunctionCall = 57,
  OpVariable = 59,
  OpDecorate = 71,
  OpMemberDecorate = 72,
  OpDecorationGroup = 73,
  OpGroupDecorate = 74,
  OpGroupMemberDecorate = 75,
  OpExecutionModeId = 331,
  OpDecorateId = 332,
  OpDecorateString = 5632,
  OpMemberDecorateString = 5633,
};

enum Capability : uint32_t
{
  CapabilityKernel = 6,
  CapabilityCooperativeMatrixReductionsNV = 5430,
  CapabilityCooperativeMatrixPerElementOperationsNV = 5432,
  CapabilityCooperativeMatrixBlockLoadsNV = 5434,
  CapabilityCooperativeMatrixDecodeVectorNV = 5447,
  CapabilityFunctionPointersINTEL = 5603,
  CapabilityFunctionVariantsINTEL = 6246,
};

constexpr uint32_t kMagic = 0x07230203u;
constexpr size_t kHeaderWords = 5;
constexpr uint32_t kStorageInput = 1;
constexpr uint32_t kStorageOutput = 3;
constexpr uint32_t kNoFunction = ~0u;

enum IdFlag : uint8_t
{
  kDeclared = 1,
  kFunction = 2,
  kInterface = 4,
  kKept = 8,
};

// The id a declaration outside functions introduces, 0 when it is not one of
// the declarations this knows.
uint32_t declaredId(uint32_t op, const uint32_t* w, uint32_t length)
{
  if (length >= 2 &&
      ((op >= OpTypeVoid && op <= OpTypePipe) || op == OpExtInstImport))
  {
    return w[1];
  }
  if (length >= 3 && ((op >= OpConstantTrue && op <= OpSpecConstantOp) ||
                      op == OpUndef || op == OpVariable))
  {
    return w[2];
  }
  return 0;
}

bool decorates(uint32_t op)
{
  switch (op)
  {
    case OpDecorate:
    case OpMemberDecorate:
    case OpDecorateId:
    case OpDecorateString:
    case OpMemberDecorateString:
      return true;
    default:
      return false;
  }
}

// A function or interface variable the kept entry point does not reach.
bool dropped(const std::vector<uint8_t>& flags, uint32_t id)
{
  return id < flags.size() && (flags[id] & (kFunction | kInterface)) &&
         !(flags[id] & kKept);
}

// Debug info sets reference functions and variables from anywhere, so the
// math set is the only one safe to cut around.
bool importsMath(const uint32_t* w, uint32_t length)
{
  return length == 6 && memcmp(w + 2, "GLSL.std.450", 13) == 0;
}

// These name functions other than through OpFunctionCall, which the
// reachability walk follows.
bool namesFunctionsIndirectly(uint32_t capability)
{
  switch (capability)
  {
    case CapabilityKernel:
    case CapabilityCooperativeMatrixReductionsNV:
    case CapabilityCooperativeMatrixPerElementOperationsNV:
    case CapabilityCooperativeMatrixBlockLoadsNV:
    case CapabilityCooperativeMatrixDecodeVectorNV:
    case CapabilityFunctionPointersINTEL:
    case CapabilityFunctionVariantsINTEL:
      return true;
    default:
      return false;
  }
}

uint32_t wordAt(const void* code, size_t index)
{
  uint32_t word;
  memcpy(
      &word,
      static_cast<const uint8_t*>(code) + index * sizeof(uint32_t),
      sizeof(word));
  return word;
}

bool validHeader(const void* code, size_t count)
{
  return code != nullptr && count >= kHeaderWords && wordAt(code, 0) == kMagic;
}
} // namespace

size_t spirvEntryPointCount(const void* code, size_t wordCount)
{
  if (!validHeader(code, wordCount))
  {
    return 0;
  }
  size_t entries = 0;
  for (size_t at = kHeaderWords; at < wordCount;)
  {
    const uint32_t word = wordAt(code, at);
    const uint32_t length = word >> 16;
    const uint32_t op = word & 0xFFFF;
    if (length == 0 || op == OpFunction)
    {
      break;
    }
    entries += op == OpEntryPoint;
    at += length;
  }
  return entries;
}

bool spirvEntryModule(
    const uint32_t* words,
    size_t count,
    uint32_t executionModel,
    const char* name,
    SpirvEntryScratch& scratch)
{
  if (!validHeader(words, count) || name == nullptr)
  {
    return false;
  }
  // Caps the per id tables at the module's size; sparser modules stay whole.
  const uint32_t bound = words[3];
  if (bound == 0 || bound > count)
  {
    return false;
  }
  std::vector<uint8_t>& flags = scratch.flags;
  std::vector<uint32_t>& functionIndex = scratch.functionIndex;
  std::vector<SpirvEntryScratch::Function>& functions = scratch.functions;
  std::vector<uint32_t>& calls = scratch.calls;
  flags.assign(bound, 0);
  functionIndex.assign(bound, kNoFunction);
  functions.clear();
  calls.clear();
  size_t entries = 0;
  size_t entryAt = 0;
  uint32_t entryFunction = 0;
  size_t interfaceBegin = 0;
  size_t interfaceEnd = 0;
  bool inFunction = false;
  for (size_t at = kHeaderWords; at < count;)
  {
    const uint32_t length = words[at] >> 16;
    const uint32_t op = words[at] & 0xFFFF;
    if (length == 0 || at + length > count)
    {
      return false;
    }
    const uint32_t* w = words + at;
    if (op == OpFunction)
    {
      if (inFunction || length < 3 || w[2] >= bound || flags[w[2]] != 0)
      {
        return false;
      }
      flags[w[2]] |= kDeclared | kFunction;
      functionIndex[w[2]] = static_cast<uint32_t>(functions.size());
      functions.push_back({0, static_cast<uint32_t>(calls.size()), 0});
      inFunction = true;
    }
    else if (op == OpFunctionEnd)
    {
      if (!inFunction)
      {
        return false;
      }
      functions.back().end = at + length;
      functions.back().callsEnd = static_cast<uint32_t>(calls.size());
      inFunction = false;
    }
    else if (inFunction)
    {
      if (op == OpFunctionCall && length >= 4)
      {
        calls.push_back(w[3]);
      }
    }
    else if (
        op == OpDecorationGroup || op == OpGroupDecorate ||
        op == OpGroupMemberDecorate ||
        (op == OpExtInstImport && !importsMath(w, length)) ||
        (op == OpCapability && length >= 2 && namesFunctionsIndirectly(w[1])))
    {
      return false;
    }
    else if (op == OpEntryPoint)
    {
      if (length < 4)
      {
        return false;
      }
      const char* entryName = reinterpret_cast<const char*>(w + 3);
      const size_t maxBytes = (length - 3) * sizeof(uint32_t);
      const char* end = static_cast<const char*>(
          memchr(entryName, 0, maxBytes));
      if (end == nullptr)
      {
        return false;
      }
      const size_t nameBytes = end - entryName;
      entries++;
      if (w[1] == executionModel && strcmp(entryName, name) == 0)
      {
        entryAt = at;
        entryFunction = w[2];
        interfaceBegin = at + 3 + nameBytes / sizeof(uint32_t) + 1;
        interfaceEnd = at + length;
      }
    }
    else if (uint32_t id = declaredId(op, w, length))
    {
      if (id >= bound)
      {
        return false;
      }
      flags[id] |= kDeclared;
      if (op == OpVariable && length >= 4 &&
          (w[3] == kStorageInput || w[3] == kStorageOutput))
      {
        flags[id] |= kInterface;
      }
    }
    at += length;
  }
  if (inFunction || entries < 2 || entryAt == 0 || entryFunction >= bound ||
      functionIndex[entryFunction] == kNoFunction)
  {
    return false;
  }

  std::vector<uint32_t>& pending = scratch.pending;
  pending.assign(1, entryFunction);
  while (!pending.empty())
  {
    const uint32_t id = pending.back();
    pending.pop_back();
    if (id >= bound || (flags[id] & kKept) || functionIndex[id] == kNoFunction)
    {
      continue;
    }
    flags[id] |= kKept;
    const SpirvEntryScratch::Function& function = functions[functionIndex[id]];
    pending.insert(
        pending.end(),
        calls.begin() + function.callsBegin,
        calls.begin() + function.callsEnd);
  }
  for (size_t i = interfaceBegin; i < interfaceEnd; i++)
  {
    if (words[i] < bound)
    {
      flags[words[i]] |= kKept;
    }
  }

  std::vector<uint32_t>& out = scratch.words;
  out.assign(words, words + kHeaderWords);
  out.reserve(count);
  for (size_t at = kHeaderWords; at < count;)
  {
    const uint32_t length = words[at] >> 16;
    const uint32_t op = words[at] & 0xFFFF;
    const uint32_t* w = words + at;
    if (op == OpFunction)
    {
      const size_t end = functions[functionIndex[w[2]]].end;
      if (flags[w[2]] & kKept)
      {
        out.insert(out.end(), w, words + end);
      }
      at = end;
      continue;
    }
    bool copy = true;
    if (op == OpEntryPoint)
    {
      copy = at == entryAt;
    }
    else if (op == OpExecutionMode || op == OpExecutionModeId)
    {
      copy = length >= 2 && w[1] == entryFunction;
    }
    else if (op == OpVariable)
    {
      copy = length < 3 || !dropped(flags, w[2]);
    }
    // Debug builds of naga name ids inside functions, which the cut does not
    // track.
    else if (op == OpName || op == OpMemberName)
    {
      copy = false;
    }
    else if (decorates(op) && length >= 2)
    {
      if (dropped(flags, w[1]))
      {
        copy = false;
      }
      // Decorating an id inside a function, which only dropping that
      // function's whole body could account for.
      else if (w[1] >= bound || !(flags[w[1]] & kDeclared))
      {
        return false;
      }
      for (uint32_t i = 3; op == OpDecorateId && i < length; i++)
      {
        if (dropped(flags, w[i]))
        {
          return false;
        }
      }
    }
    if (copy)
    {
      out.insert(out.end(), w, w + length);
    }
    at += length;
  }
  return true;
}
} // namespace rive::ore

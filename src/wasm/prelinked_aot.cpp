/*
 * Copyright 2026 Rive
 */

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/prelinked_aot.hpp"

#if !defined(__EMSCRIPTEN__) && WASM_ENABLE_PRELINKED_AOT != 0
// AOT_MAGIC_NUMBER / AOT_CURRENT_VERSION for container validation.
#include "aot_runtime.h"
#include "wasm_export.h"
#include <cstdio>
#include <cstring>
#endif

#include <vector>

namespace rive
{

// Function-local so registrations from static constructors are safe.
static std::vector<PrelinkedAotModule>& prelinkedRegistry()
{
  static std::vector<PrelinkedAotModule> registry;
  return registry;
}

void registerPrelinkedAotModule(const PrelinkedAotModule& module)
{
  prelinkedRegistry().push_back(module);
}

const PrelinkedAotModule* findPrelinkedAotModule(
    uint64_t moduleKey,
    size_t wasmSize)
{
  for (const PrelinkedAotModule& module : prelinkedRegistry())
  {
    if (module.moduleKey == moduleKey && module.wasmSize == wasmSize)
    {
      return &module;
    }
  }
  return nullptr;
}

#if !defined(__EMSCRIPTEN__) && WASM_ENABLE_PRELINKED_AOT != 0
// The relocation section is dropped: the native linker applied every fixup,
// which is what makes the text usable without runtime executable memory.
void* loadPrelinkedAotModule(
    const PrelinkedAotModule& prelinked,
    char* error,
    uint32_t errorSize)
{
  constexpr uint32_t kSectionText = 2;
  constexpr uint32_t kSectionRelocation = 5;
  // The from sections loader never sees the container's magic and version,
  // so an artifact from an incompatible wamrc is rejected here.
  if (prelinked.aotSize < 8)
  {
    snprintf(error, errorSize, "prelinked aot container too small");
    return nullptr;
  }
  uint32_t magic;
  uint32_t version;
  memcpy(&magic, prelinked.aot, 4);
  memcpy(&version, prelinked.aot + 4, 4);
  if (magic != AOT_MAGIC_NUMBER || version != AOT_CURRENT_VERSION)
  {
    snprintf(
        error,
        errorSize,
        "prelinked aot container magic/version mismatch "
        "(%08x v%u, runtime expects v%u)",
        magic,
        version,
        (uint32_t)AOT_CURRENT_VERSION);
    return nullptr;
  }
  const uint8_t* p = prelinked.aot + 8;
  const uint8_t* end = prelinked.aot + prelinked.aotSize;
  std::vector<wasm_section_t> sections;
  while (true)
  {
    // Section headers are 4 byte aligned; compared as integers, since the
    // aligned address may lie past end, where pointers are undefined.
    uintptr_t aligned = ((uintptr_t)p + 3) & ~(uintptr_t)3;
    if (aligned > (uintptr_t)end || (uintptr_t)end - aligned < 8)
    {
      break;
    }
    p = (const uint8_t*)aligned;
    uint32_t type;
    uint32_t size;
    memcpy(&type, p, 4);
    memcpy(&size, p + 4, 4);
    p += 8;
    if (size > (size_t)(end - p))
    {
      snprintf(error, errorSize, "prelinked aot container truncated");
      return nullptr;
    }
    if (type != kSectionRelocation)
    {
      wasm_section_t section = {};
      section.section_type = (int)type;
      if (type == kSectionText)
      {
        section.section_body = const_cast<uint8_t*>(prelinked.text);
        section.section_body_size = (uint32_t)prelinked.textSize;
      }
      else
      {
        section.section_body = const_cast<uint8_t*>(p);
        section.section_body_size = size;
      }
      sections.push_back(section);
    }
    p += size;
  }
  if (sections.empty())
  {
    snprintf(error, errorSize, "prelinked aot container empty");
    return nullptr;
  }
  for (size_t i = 0; i + 1 < sections.size(); i++)
  {
    sections[i].next = &sections[i + 1];
  }
  sections.back().next = nullptr;
  return wasm_runtime_load_from_sections(
      sections.data(),
      true,
      error,
      errorSize);
}
#endif

} // namespace rive

#endif

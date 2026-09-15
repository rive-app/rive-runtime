/*
 * Copyright 2026 Rive
 */

// Registry of wasm modules whose AOT code was compiled offline and linked
// into this binary. Platforms without runtime executable memory load these
// through wasm_runtime_load_from_sections, pointing the text section at the
// linked copy; everything else falls back to the interpreter.

#ifndef _RIVE_PRELINKED_AOT_HPP_
#define _RIVE_PRELINKED_AOT_HPP_

#include <cstddef>
#include <cstdint>

namespace rive
{

struct PrelinkedAotModule
{
    // FNV-1a 64 of the wasm module bytes, matching WasmScriptingVM's key.
    uint64_t moduleKey;
    // The full .aot container; metadata sections are read from here.
    const uint8_t* aot;
    size_t aotSize;
    // The linked, executable copy of the container's text payload.
    const uint8_t* text;
    size_t textSize;
    // Byte length of the wasm module the key was computed from; checked at
    // lookup so a hash collision cannot select the wrong module's code.
    size_t wasmSize;
};

void registerPrelinkedAotModule(const PrelinkedAotModule& module);
const PrelinkedAotModule* findPrelinkedAotModule(uint64_t moduleKey,
                                                 size_t wasmSize);

} // namespace rive

#endif

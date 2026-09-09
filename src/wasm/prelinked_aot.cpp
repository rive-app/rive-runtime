/*
 * Copyright 2026 Rive
 */

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/prelinked_aot.hpp"

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

const PrelinkedAotModule* findPrelinkedAotModule(uint64_t moduleKey,
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

} // namespace rive

#endif

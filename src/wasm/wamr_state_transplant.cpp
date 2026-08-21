#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/wamr_state_transplant.hpp"

#include "aot_runtime.h"
#include "wasm_runtime.h"

#include <cstring>
#include <vector>

namespace rive
{

namespace
{

struct GlobalRef
{
    uint8_t type = 0;
    uint32_t offset = 0;
};

static size_t globalSize(uint8_t type)
{
    switch (type)
    {
        case VALUE_TYPE_I32:
        case VALUE_TYPE_F32:
            return 4;
        case VALUE_TYPE_I64:
        case VALUE_TYPE_F64:
            return 8;
#if WASM_ENABLE_SIMD != 0 || WASM_ENABLE_SIMDE != 0
        case VALUE_TYPE_V128:
            return 16;
#endif
        default:
            return 0;
    }
}

// The two representations agree on the wasm's global index space but not on
// data offsets: the interp loader assigns natural-size offsets at load, the
// aot compiler assigns its own at compile. Enumerate through each side's own
// metadata.
static bool collectGlobals(WASMModuleInstance* inst,
                           std::vector<GlobalRef>& out,
                           std::string& error)
{
    if (inst->module_type == Wasm_Module_Bytecode)
    {
        WASMModuleInstanceExtra* extra = (WASMModuleInstanceExtra*)inst->e;
        for (uint32_t i = 0; i < extra->global_count; i++)
        {
            out.push_back(
                {extra->globals[i].type, extra->globals[i].data_offset});
        }
        return true;
    }
    if (inst->module_type == Wasm_Module_AoT)
    {
        AOTModule* module = (AOTModule*)((AOTModuleInstance*)inst)->module;
        for (uint32_t i = 0; i < module->import_global_count; i++)
        {
            out.push_back({module->import_globals[i].type.val_type,
                           module->import_globals[i].data_offset});
        }
        for (uint32_t i = 0; i < module->global_count; i++)
        {
            out.push_back({module->globals[i].type.val_type,
                           module->globals[i].data_offset});
        }
        return true;
    }
    error = "unknown module representation";
    return false;
}

} // namespace

bool wamrTransplantState(wasm_module_inst_t source,
                         wasm_module_inst_t destination,
                         std::string& error)
{
    WASMModuleInstance* src = (WASMModuleInstance*)source;
    WASMModuleInstance* dst = (WASMModuleInstance*)destination;

    // Memories: grow the fresh instance up to the source's live size, then
    // copy bytes.
    if (src->memory_count != dst->memory_count)
    {
        error = "memory count mismatch";
        return false;
    }
    for (uint32_t i = 0; i < src->memory_count; i++)
    {
        WASMMemoryInstance* srcMem = src->memories[i];
        WASMMemoryInstance* dstMem = dst->memories[i];
        if (srcMem->cur_page_count > dstMem->cur_page_count)
        {
            if (!wasm_runtime_enlarge_memory(destination,
                                             srcMem->cur_page_count -
                                                 dstMem->cur_page_count))
            {
                error = "destination memory grow failed";
                return false;
            }
            // Growth may reallocate the instance's memory table entry.
            dstMem = dst->memories[i];
        }
        if (dstMem->memory_data_size < srcMem->memory_data_size)
        {
            error = "destination memory smaller than source after grow";
            return false;
        }
        memcpy(dstMem->memory_data,
               srcMem->memory_data,
               srcMem->memory_data_size);
    }

    // Globals: per-global typed copy through each side's own offsets.
    std::vector<GlobalRef> srcGlobals;
    std::vector<GlobalRef> dstGlobals;
    if (!collectGlobals(src, srcGlobals, error) ||
        !collectGlobals(dst, dstGlobals, error))
    {
        return false;
    }
    if (srcGlobals.size() != dstGlobals.size())
    {
        error = "global count mismatch";
        return false;
    }
    for (size_t i = 0; i < srcGlobals.size(); i++)
    {
        if (srcGlobals[i].type != dstGlobals[i].type)
        {
            error = "global type mismatch";
            return false;
        }
        size_t size = globalSize(srcGlobals[i].type);
        if (size == 0)
        {
            error = "unsupported global type";
            return false;
        }
        memcpy(dst->global_data + dstGlobals[i].offset,
               src->global_data + srcGlobals[i].offset,
               size);
    }

    // Tables: entries are function indexes in both representations (non-GC
    // builds), so index copy is representation-neutral.
    if (src->table_count != dst->table_count)
    {
        error = "table count mismatch";
        return false;
    }
    for (uint32_t i = 0; i < src->table_count; i++)
    {
        WASMTableInstance* srcTable = src->tables[i];
        WASMTableInstance* dstTable = dst->tables[i];
        if (dstTable->cur_size < srcTable->cur_size)
        {
            error = "destination table smaller than source";
            return false;
        }
        memcpy(dstTable->elems,
               srcTable->elems,
               sizeof(table_elem_type_t) * srcTable->cur_size);
    }

    return true;
}

} // namespace rive

#endif // WITH_RIVE_SCRIPTING_WASM

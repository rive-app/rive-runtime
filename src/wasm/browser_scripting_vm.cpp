#if defined(WITH_RIVE_SCRIPTING_WASM) && defined(__EMSCRIPTEN__)

#include "rive/wasm/browser_scripting_vm.hpp"

#include <emscripten/em_js.h>

using namespace rive;

// The page-side slot table (globalThis.__riveScriptInstances) is owned by
// the JS module runner, which instantiates each script module against the
// import object web/rive_module_imports.mjs builds.
// clang-format off
EM_JS(uint32_t, riveWebInstantiate, (const uint8_t* bytes, uint32_t size), {
    // TODO(web-backend): WebAssembly.instantiate against the generated
    // import object, mint a slot in globalThis.__riveScriptInstances.
    return 0;
});

EM_JS(uint32_t, riveWebCallModule,
      (uint32_t slot, const char* name, uint32_t argc, uint32_t* argv), {
    const table = globalThis.__riveScriptInstances;
    const instance = table && table.get(slot);
    const fn = instance && instance.exports[UTF8ToString(name)];
    if (!fn)
    {
        return 0;
    }
    const args = [];
    for (let i = 0; i < argc; i++)
    {
        args.push(HEAPU32[(argv >> 2) + i]);
    }
    // TODO(web-backend): trap handling — catch, report, return 0.
    return fn.apply(null, args) >>> 0;
});

EM_JS(void, riveWebReadMemory,
      (uint32_t slot, uint32_t appAddr, uint8_t* dst, uint32_t size), {
    const table = globalThis.__riveScriptInstances;
    const instance = table && table.get(slot);
    if (!instance)
    {
        return;
    }
    HEAPU8.set(
        new Uint8Array(instance.exports.memory.buffer, appAddr, size), dst);
});

EM_JS(void, riveWebWriteMemory,
      (uint32_t slot, uint32_t appAddr, const uint8_t* src, uint32_t size), {
    const table = globalThis.__riveScriptInstances;
    const instance = table && table.get(slot);
    if (!instance)
    {
        return;
    }
    new Uint8Array(instance.exports.memory.buffer, appAddr, size)
        .set(HEAPU8.subarray(src, src + size));
});

EM_JS(void, riveWebReleaseInstance, (uint32_t slot), {
    const table = globalThis.__riveScriptInstances;
    if (table)
    {
        table.delete(slot);
    }
});
// clang-format on

std::unique_ptr<BrowserScriptingVM> BrowserScriptingVM::make(
    Span<const uint8_t> module,
    Factory* factory,
    std::string& outError)
{
    std::unique_ptr<BrowserScriptingVM> vm(new BrowserScriptingVM());
    vm->m_factory = factory;
    if (!vm->boot(module))
    {
        outError = vm->m_lastError;
        return nullptr;
    }
    return vm;
}

bool BrowserScriptingVM::boot(Span<const uint8_t> module)
{
    m_instanceSlot = riveWebInstantiate(module.data(), (uint32_t)module.size());
    if (m_instanceSlot == 0)
    {
        m_lastError = "browser module instantiation failed";
        return false;
    }
    // TODO(web-backend): mirror WasmScriptingVM::init's boot sequence
    // (__wasm_call_ctors, host_newstate into m_L, the host_install_* set,
    // host_register_embedded, host_seal_env) through callModule.
    m_lastError = "browser backend boot not implemented";
    return false;
}

BrowserScriptingVM::~BrowserScriptingVM()
{
    if (m_instanceSlot != 0)
    {
        riveWebReleaseInstance(m_instanceSlot);
    }
}

bool BrowserScriptingVM::valid() const
{
    return m_instanceSlot != 0 && m_L != 0;
}

void* BrowserScriptingVM::resolveModulePtr(uint32_t appAddr, uint32_t size)
{
    // TODO(web-backend): bounds-check against the instance's memory size
    // and track dirty ranges so clean reads skip the write-back.
    m_staged.emplace_back();
    StagedRange& range = m_staged.back();
    range.appAddr = appAddr;
    range.bytes.resize(size);
    riveWebReadMemory(m_instanceSlot, appAddr, range.bytes.data(), size);
    return range.bytes.data();
}

uint32_t BrowserScriptingVM::callModule(const char* name,
                                        uint32_t argc,
                                        uint32_t* argv)
{
    // Writes through staged pointers must land before the module runs.
    for (StagedRange& range : m_staged)
    {
        riveWebWriteMemory(m_instanceSlot,
                           range.appAddr,
                           range.bytes.data(),
                           (uint32_t)range.bytes.size());
    }
    m_staged.clear();
    return riveWebCallModule(m_instanceSlot, name, argc, argv);
}

void BrowserScriptingVM::raiseModuleError(const char* message)
{
    // TODO(web-backend): abort the in-flight module call; for now the error
    // is only recorded.
    m_lastError = message;
}

#endif

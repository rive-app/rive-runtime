#if defined(WITH_RIVE_SCRIPTING_WASM) && defined(__EMSCRIPTEN__)

#include "rive/wasm/browser_scripting_vm.hpp"

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#include <emscripten/stack.h>

#include <cassert>
#include <cstdlib>

using namespace rive;

// clang-format off
EM_JS_DEPS(riveWebScripting,
           "$UTF8ToString,$stringToUTF8,$stringToNewUTF8,"
           "$stackSave,$stackAlloc,$stackRestore,malloc,free");

// The runner is web/rive_script_runner.mjs, installed by the host. Only code
// inside librive can see its heap and exports, so they are handed over here.
EM_JS(int, riveWebRegister, (uint32_t vm, const uint8_t* bytes, uint32_t size), {
    const runner = Module['riveScripting'];
    if (!runner)
    {
        return 0;
    }
    runner.attach({
        exports: Module,
        heapU8: () => HEAPU8,
        heapF64: () => HEAPF64,
        stackSave: stackSave,
        stackAlloc: stackAlloc,
        stackRestore: stackRestore,
        malloc: _malloc,
        free: _free,
        utf8: (ptr) => UTF8ToString(ptr),
        writeUtf8: (text, ptr, capacity) => stringToUTF8(text, ptr, capacity),
    });
    runner.register(vm, HEAPU8.subarray(bytes, bytes + size));
    return 1;
});

EM_JS(void, riveWebRelease, (uint32_t vm), {
    Module['riveScripting']?.release(vm);
});

// Null when the VM has an instance, else a malloc'd error.
EM_JS(char*, riveWebStart, (uint32_t vm), {
    const error = Module['riveScripting'].start(vm);
    return error === null ? 0 : stringToNewUTF8(error);
});

EM_JS(int, riveWebPreparing, (uint32_t vm), {
    return Module['riveScripting'].preparing(vm);
});

EM_JS(int, riveWebHasExport, (uint32_t vm, const char* name), {
    return Module['riveScripting'].hasExport(vm, name);
});

EM_JS(int, riveWebCall,
      (uint32_t vm, const char* name, uint32_t argc, const double* argv,
       double* result, char* trap, uint32_t trapCapacity), {
    return Module['riveScripting'].call(
        vm, name, argc, argv, result, trap, trapCapacity);
});

EM_JS(void, riveWebRaise, (uint32_t vm, const char* message), {
    Module['riveScripting'].raise(vm, message);
});

EM_JS(int, riveWebReadMemory,
      (uint32_t vm, uint32_t appAddr, uint8_t* dst, uint32_t size), {
    return Module['riveScripting'].read(vm, appAddr, dst, size);
});

EM_JS(int, riveWebWriteMemory,
      (uint32_t vm, uint32_t appAddr, const uint8_t* src, uint32_t size), {
    return Module['riveScripting'].write(vm, appAddr, src, size);
});

EM_JS(int, riveWebStringLength, (uint32_t vm, uint32_t appAddr), {
    return Module['riveScripting'].stringLength(vm, appAddr);
});

EM_JS(uint32_t, riveWebMemoryPages, (uint32_t vm), {
    return Module['riveScripting'].pages(vm);
});
// clang-format on

extern "C" EMSCRIPTEN_KEEPALIVE void rive_web_vm_module_export(uint32_t vm,
                                                               const char* name)
{
    ((BrowserScriptingVM*)(uintptr_t)vm)->noteModuleExport(name);
}

// The runner's outcome when JS unwound librive frames.
static constexpr int kHostFault = 3;

// Script and host calls nest on librive's small stack, which release builds
// never check, so a call needs this much left to cross and come back.
static constexpr size_t kCallStackReserve = 32 * 1024;

static uint32_t handleOf(const BrowserScriptingVM* vm)
{
    return (uint32_t)(uintptr_t)vm;
}

std::unique_ptr<BrowserScriptingVM> BrowserScriptingVM::make(
    Span<const uint8_t> module,
    Factory* factory)
{
    std::unique_ptr<BrowserScriptingVM> vm(new BrowserScriptingVM());
    vm->m_factory = factory;
#ifdef __EMSCRIPTEN_PTHREADS__
    vm->m_owner = pthread_self();
#endif
    if (!riveWebRegister(handleOf(vm.get()),
                         module.data(),
                         (uint32_t)module.size()))
    {
        fprintf(stderr,
                "wasm script module skipped: the page has not installed the "
                "rive script runner\n");
        return nullptr;
    }
    return vm;
}

BrowserScriptingVM::~BrowserScriptingVM()
{
    assertOwnerThread();
    riveWebRelease(handleOf(this));
}

void BrowserScriptingVM::assertOwnerThread() const
{
#ifdef __EMSCRIPTEN_PTHREADS__
    assert(pthread_equal(m_owner, pthread_self()));
#endif
}

BrowserScriptingVM::Start BrowserScriptingVM::start()
{
    if (m_started)
    {
        return m_L != 0 ? Start::started : Start::failed;
    }
    if (char* error = riveWebStart(handleOf(this)))
    {
        m_lastError = std::string("module instantiate failed: ") + error;
        free(error);
        return riveWebPreparing(handleOf(this)) ? Start::preparing
                                                : Start::failed;
    }
    m_started = true;
    if (hostBudget())
    {
        fprintf(stderr,
                "wasm script module expects a watchdog to bound its calls, "
                "which the browser does not have: a runaway script will hang "
                "the page\n");
    }
    return bootModule() ? Start::started : Start::failed;
}

bool BrowserScriptingVM::valid() const
{
    return m_started && m_L != 0 && !m_hostFault;
}

void* BrowserScriptingVM::resolveModulePtr(uint32_t appAddr, uint32_t size)
{
    if (!fitsModule(appAddr, size))
    {
        return nullptr;
    }
    std::vector<uint8_t> bytes(size);
    if (!riveWebReadMemory(handleOf(this), appAddr, bytes.data(), size))
    {
        return nullptr;
    }
    m_staged.push_back({appAddr, bytes, std::move(bytes)});
    return m_staged.back().bytes.data();
}

void* BrowserScriptingVM::resolveModuleWritePtr(uint32_t appAddr, uint32_t size)
{
    if (!fitsModule(appAddr, size))
    {
        return nullptr;
    }
    // An empty original never matches, so the whole range is written back.
    m_staged.push_back({appAddr, std::vector<uint8_t>(size), {}});
    return m_staged.back().bytes.data();
}

// A script picks the size, so check it fits before allocating it here.
bool BrowserScriptingVM::fitsModule(uint32_t appAddr, uint32_t size)
{
    uint64_t limit = (uint64_t)riveWebMemoryPages(handleOf(this)) * 65536;
    return (uint64_t)appAddr + size <= limit;
}

bool BrowserScriptingVM::copyToModule(uint32_t appAddr,
                                      const void* src,
                                      uint32_t size)
{
    return riveWebWriteMemory(handleOf(this),
                              appAddr,
                              (const uint8_t*)src,
                              size) != 0;
}

const char* BrowserScriptingVM::resolveModuleString(uint32_t appAddr)
{
    int length = riveWebStringLength(handleOf(this), appAddr);
    return length < 0
               ? nullptr
               : (const char*)resolveModulePtr(appAddr, (uint32_t)length + 1);
}

WasmScriptingVM::CallOutcome BrowserScriptingVM::callF64(const char* name,
                                                         const double* args,
                                                         uint32_t argc,
                                                         double* result)
{
    assertOwnerThread();
    if (m_hostFault)
    {
        return CallOutcome::trapped;
    }
    // Writes through staged pointers must land before the module runs.
    for (StagedRange& range : m_staged)
    {
        if (range.bytes == range.original)
        {
            continue;
        }
        riveWebWriteMemory(handleOf(this),
                           range.appAddr,
                           range.bytes.data(),
                           (uint32_t)range.bytes.size());
    }
    m_staged.clear();
    if (emscripten_stack_get_free() < kCallStackReserve)
    {
        recordTrap(name, "script calls nest too deep for the host stack");
        return CallOutcome::trapped;
    }

    char trap[512];
    trap[0] = 0;
    int outcome;
    {
        ScriptCallScope callScope(this);
        outcome = riveWebCall(handleOf(this),
                              name,
                              argc,
                              args,
                              result,
                              trap,
                              sizeof(trap));
    }
    if (outcome == kHostFault)
    {
        m_hostFault = true;
        m_lastError = std::string("librive faulted in a host call, so its "
                                  "script module stops: ") +
                      trap;
        recordTrap(name, m_lastError.c_str());
        return CallOutcome::trapped;
    }
    if (outcome == (int)CallOutcome::trapped)
    {
        recordTrap(name, trap);
    }
    return (CallOutcome)outcome;
}

WasmScriptingVM::CallOutcome BrowserScriptingVM::callModuleChecked(
    const char* name,
    uint32_t argc,
    uint32_t* argv,
    uint32_t* result)
{
    double args[8];
    assert(argc <= 8);
    for (uint32_t i = 0; i < argc; i++)
    {
        args[i] = argv[i];
    }
    double value = 0;
    CallOutcome outcome = callF64(name, args, argc, &value);
    if (outcome == CallOutcome::ok)
    {
        *result = (uint32_t)(int64_t)value;
    }
    return outcome;
}

bool BrowserScriptingVM::hasExport(const char* name)
{
    if (!m_started)
    {
        return false;
    }
    auto it = m_exports.find(name);
    if (it == m_exports.end())
    {
        it =
            m_exports.emplace(name, riveWebHasExport(handleOf(this), name) != 0)
                .first;
    }
    return it->second;
}

void BrowserScriptingVM::raiseModuleError(const char* message)
{
    m_moduleErrorDetail = message;
    riveWebRaise(handleOf(this), message);
}

bool BrowserScriptingVM::memoryLimits(uint32_t* pages, uint32_t* maxPages) const
{
    *pages = riveWebMemoryPages(handleOf(this));
    // The declared ceiling is not readable from the page.
    *maxPages = 0;
    return *pages != 0;
}

#endif

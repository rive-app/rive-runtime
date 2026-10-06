#ifndef _RIVE_BROWSER_SCRIPTING_VM_HPP_
#define _RIVE_BROWSER_SCRIPTING_VM_HPP_

#if defined(WITH_RIVE_SCRIPTING_WASM) && defined(__EMSCRIPTEN__)

#include "rive/wasm/wasm_scripting_vm.hpp"

#ifdef __EMSCRIPTEN_PTHREADS__
#include <pthread.h>
#endif

namespace rive
{

/// Runs the script module on the browser's own wasm engine. The page side
/// lives in web/rive_script_runner.mjs, which the host installs; librive
/// reaches the module only through the seams. Impl cores and the seam
/// routed ScriptBackend bodies are inherited from the shared backend.
///
/// Starting is two steps so the host can instantiate asynchronously in
/// between: make() registers the module bytes with the runner, start()
/// claims the instance the host prepared, or instantiates inline when the
/// host prepared none, then boots the module.
class BrowserScriptingVM : public WasmScriptingVM
{
public:
    static std::unique_ptr<BrowserScriptingVM> make(Span<const uint8_t> module,
                                                    Factory* factory);
    ~BrowserScriptingVM() override;

    /// Preparing while the page still instantiates the module, so a later
    /// call can start it; failed with the reason in lastError().
    enum class Start
    {
        started,
        preparing,
        failed,
    };
    Start start();

    /// The runner announces the module's flag exports before it starts.
    using WasmScriptingVM::noteModuleExport;

    bool valid() const override;
    void* resolveModulePtr(uint32_t appAddr, uint32_t size) override;
    void* resolveModuleWritePtr(uint32_t appAddr, uint32_t size) override;
    bool copyToModule(uint32_t appAddr,
                      const void* src,
                      uint32_t size) override;
    const char* resolveModuleString(uint32_t appAddr) override;
    CallOutcome callModuleChecked(const char* name,
                                  uint32_t argc,
                                  uint32_t* argv,
                                  uint32_t* result) override;
    bool hasExport(const char* name) override;
    void raiseModuleError(const char* message) override;
    bool memoryLimits(uint32_t* pages, uint32_t* maxPages) const override;

private:
    BrowserScriptingVM() = default;

    /// Every entry into the module.
    CallOutcome callF64(const char* name,
                        const double* args,
                        uint32_t argc,
                        double* result) override;

    // The runner keeps its modules per JS realm, so a VM stays on the thread
    // that made it.
    void assertOwnerThread() const;
#ifdef __EMSCRIPTEN_PTHREADS__
    pthread_t m_owner;
#endif
    bool m_started = false;
    /// Set once JS unwound librive frames; the module never runs again.
    bool m_hostFault = false;
    std::unordered_map<std::string, bool> m_exports;
    /// Module memory ranges copied into the librive heap. At the next call
    /// into the module they go away, and the ones the host wrote through
    /// write back. A range only read must not: natives stage while the
    /// module runs, and it may have reused that memory by then.
    struct StagedRange
    {
        uint32_t appAddr = 0;
        std::vector<uint8_t> bytes;
        std::vector<uint8_t> original;
    };
    std::vector<StagedRange> m_staged;
    bool fitsModule(uint32_t appAddr, uint32_t size);
};

} // namespace rive

#endif
#endif

#ifndef _RIVE_BROWSER_SCRIPTING_VM_HPP_
#define _RIVE_BROWSER_SCRIPTING_VM_HPP_

#if defined(WITH_RIVE_SCRIPTING_WASM) && defined(__EMSCRIPTEN__)

#include "rive/wasm/wasm_scripting_vm.hpp"

namespace rive
{

/// Runs the script module on the browser's own wasm engine: the module is
/// instantiated JS-side into a slot table and librive reaches it only
/// through the seams — resolveModulePtr returns copies staged through the
/// librive heap and flushed at the next callModule, callModule dispatches
/// over EM_JS to the slot's exports. Impl cores and the seam-routed
/// ScriptBackend bodies are inherited from the WAMR backend; the entry
/// points that call WAMR with typed f64 args (callUserInit, callAdvance,
/// callPointerEvent, callLayoutResize, setInputNumber) still need browser
/// overrides before this backend can go live. Not wired into File::import
/// yet.
class BrowserScriptingVM : public WasmScriptingVM
{
public:
    static std::unique_ptr<BrowserScriptingVM> make(Span<const uint8_t> module,
                                                    Factory* factory,
                                                    std::string& outError);
    ~BrowserScriptingVM() override;

    bool valid() const override;
    void* resolveModulePtr(uint32_t appAddr, uint32_t size) override;
    uint32_t callModule(const char* name,
                        uint32_t argc,
                        uint32_t* argv) override;
    void raiseModuleError(const char* message) override;

private:
    BrowserScriptingVM() = default;
    bool boot(Span<const uint8_t> module);

    /// JS-side instance table slot; 0 means not instantiated.
    uint32_t m_instanceSlot = 0;
    /// Module-memory ranges staged into the librive heap; they flush back
    /// and invalidate at the next callModule, the seam's coherence
    /// contract.
    struct StagedRange
    {
        uint32_t appAddr = 0;
        std::vector<uint8_t> bytes;
    };
    std::vector<StagedRange> m_staged;
};

} // namespace rive

#endif
#endif

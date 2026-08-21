#ifndef _RIVE_SCRIPT_MODULE_ASSET_HPP_
#define _RIVE_SCRIPT_MODULE_ASSET_HPP_
#include "rive/generated/assets/script_module_asset_base.hpp"
#include "rive/simple_array.hpp"

namespace rive
{
/// A self contained wasm module (VM + bindings + compiled scripts) for one
/// source language, produced by the scripting workspace's compileWasm link.
/// ScriptAssets reference into it by registered module name; the wasm backend
/// instantiates it per file instance.
class ScriptModuleAsset : public ScriptModuleAssetBase
{
public:
    enum class Language : uint32_t
    {
        luau = 0,
        assemblyScript = 1,
    };

    bool decode(SimpleArray<uint8_t>& data, Factory* factory) override;
    std::string fileExtension() const override { return "wasm"; }

    Span<const uint8_t> module() const { return m_module; }

private:
    SimpleArray<uint8_t> m_module;
};
} // namespace rive

#endif

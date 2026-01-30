#ifndef _RIVE_SCRIPT_ASSET_HPP_
#define _RIVE_SCRIPT_ASSET_HPP_
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/file.hpp"
#include "rive/generated/assets/script_asset_base.hpp"
#include "rive/simple_array.hpp"
#include <stdio.h>

#ifdef WITH_RIVE_SCRIPTING
struct lua_State;
#endif

namespace rive
{
class Artboard;
class Component;
class DataBind;
class ScriptedObject;

enum ScriptProtocol
{
    utility,
    node,
    layout,
    converter,
    pathEffect,
    listenerAction,
    transitionCondition
};

#ifdef WITH_RIVE_SCRIPTING
class ScriptAssetImporter;
#endif

class ScriptInput
{
protected:
    ScriptedObject* m_scriptedObject = nullptr;
    DataBind* m_dataBind = nullptr;
    bool m_ownsDataBind = false;

public:
    virtual ~ScriptInput();
    virtual void initScriptedValue();
    virtual bool validateForScriptInit() = 0;
    static ScriptInput* from(Core* component);
    DataBind* dataBind() { return m_dataBind; }
    void dataBind(DataBind* dataBind, bool ownsDataBind = false)
    {
        m_dataBind = dataBind;
        m_ownsDataBind = ownsDataBind;
    }
    ScriptedObject* scriptedObject() { return m_scriptedObject; }
    void scriptedObject(ScriptedObject* object) { m_scriptedObject = object; }
};

class OptionalScriptedMethods
{
private:
    static const int m_advancesBit = 1 << 0;
    static const int m_updatesBit = 1 << 1;
    static const int m_measuresBit = 1 << 2;
    static const int m_wantsPointerDownBit = 1 << 3;
    static const int m_wantsPointerMoveBit = 1 << 4;
    static const int m_wantsPointerUpBit = 1 << 5;
    static const int m_wantsPointerExitBit = 1 << 6;
    static const int m_wantsPointerCancelBit = 1 << 7;
    static const int m_drawsBit = 1 << 8;
    static const int m_initsBit = 1 << 9;
    static const int m_dataConvertsBit = 1 << 10;
    static const int m_dataReverseConvertsBit = 1 << 11;
    static const int m_resizesBit = 1 << 12;

    int m_implementedMethods = 0;

protected:
#ifdef WITH_RIVE_SCRIPTING
    bool verifyImplementation(ScriptedObject* object, lua_State* state);
#endif

public:
    int implementedMethods() { return m_implementedMethods; }
    void implementedMethods(int implemented)
    {
        m_implementedMethods = implemented;
    }
    bool listensToPointerEvents()
    {
        return (m_implementedMethods &
                (m_wantsPointerDownBit | m_wantsPointerMoveBit |
                 m_wantsPointerUpBit | m_wantsPointerExitBit |
                 m_wantsPointerCancelBit)) != 0;
    }
    bool advances() { return (m_implementedMethods & m_advancesBit) != 0; }
    bool updates() { return (m_implementedMethods & m_updatesBit) != 0; }
    bool measures() { return (m_implementedMethods & m_measuresBit) != 0; }
    bool resizes() { return (m_implementedMethods & m_resizesBit) != 0; }
    bool wantsPointerDown()
    {
        return (m_implementedMethods & m_wantsPointerDownBit) != 0;
    }
    bool wantsPointerMove()
    {
        return (m_implementedMethods & m_wantsPointerMoveBit) != 0;
    }
    bool wantsPointerUp()
    {
        return (m_implementedMethods & m_wantsPointerUpBit) != 0;
    }
    bool wantsPointerExit()
    {
        return (m_implementedMethods & m_wantsPointerExitBit) != 0;
    }
    bool wantsPointerCancel()
    {
        return (m_implementedMethods & m_wantsPointerCancelBit) != 0;
    }
    bool draws() { return (m_implementedMethods & m_drawsBit) != 0; }
    bool inits() { return (m_implementedMethods & m_initsBit) != 0; }
    bool dataConverts()
    {
        return (m_implementedMethods & m_dataConvertsBit) != 0;
    }
    bool dataReverseConverts()
    {
        return (m_implementedMethods & m_dataReverseConvertsBit) != 0;
    }
};

class ModuleDetails
{
public:
    virtual ~ModuleDetails() = default;
    virtual std::string moduleName() = 0;
    virtual void registrationComplete(int ref) {}
    virtual Span<uint8_t> moduleBytecode() { return Span<uint8_t>(); }
    virtual bool isProtocolScript() = 0;
    virtual bool verified() const { return false; }
    void addMissingDependency(std::string name) { m_dependencies.insert(name); }
    void clearMissingDependency(std::string name)
    {
        auto it = m_dependencies.find(name);
        if (it != m_dependencies.end())
        {
            m_dependencies.erase(it);
        }
    }
    std::unordered_set<std::string> missingDependencies()
    {
        return m_dependencies;
    }

private:
    std::unordered_set<std::string> m_dependencies;
};

class ScriptAsset : public ScriptAssetBase,
                    public OptionalScriptedMethods,
                    public ModuleDetails
{

public:
#ifdef WITH_RIVE_SCRIPTING
    friend class ScriptAssetImporter;

    bool verified() const override { return m_verified; }
    Span<uint8_t> moduleBytecode() override { return m_bytecode; }
#endif

    bool initScriptedObject(ScriptedObject* object);

    /// Sets the bytecode from data with header format:
    ///   [flags:1] [signature:64 if signed] [luau_bytecode:N]
    /// Flags byte: bits 0-6 = version, bit 7 = isSigned
    /// Returns true if bytecode was set (verification is separate from return).
    bool bytecode(Span<uint8_t> data);

    /// Bytecode provided via decode should only happen with in-band bytecode.
    /// The signature will later be verified once file loading completes, so it
    /// is immediately marked as unverified.
    bool decode(SimpleArray<uint8_t>& data, Factory* factory) override;

    std::string fileExtension() const override { return "lua"; }
    void file(File* value) { m_file = value; }
    File* file() const { return m_file; }
#ifdef WITH_RIVE_SCRIPTING
    lua_State* vm();
    void registrationComplete(int ref) override;
#endif
    std::string moduleName() override
    {
        return folderPath() == "" ? name() : folderPath() + "/" + name();
    }
    bool isProtocolScript() override { return !isModule(); }

private:
    File* m_file = nullptr;
#ifdef WITH_RIVE_SCRIPTING
    bool m_scriptRegistered = false;
    bool m_verified = false;
    SimpleArray<uint8_t> m_bytecode;
    bool m_initted = false;
#endif

    bool initScriptedObjectWith(ScriptedObject* object);
};
} // namespace rive

#endif
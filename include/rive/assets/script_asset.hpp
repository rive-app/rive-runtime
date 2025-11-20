#ifndef _RIVE_SCRIPT_ASSET_HPP_
#define _RIVE_SCRIPT_ASSET_HPP_
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/file.hpp"
#include "rive/generated/assets/script_asset_base.hpp"
#include "rive/simple_array.hpp"
#include <stdio.h>
namespace rive
{
class Artboard;
class Component;
class DataBind;
class File;
class ScriptedObject;

enum ScriptProtocol
{
    utility,
    node,
    layout,
    converter,
    pathEffect
};

#ifdef WITH_RIVE_SCRIPTING
class ScriptAssetImporter;
#endif

class ScriptInput
{
protected:
    ScriptedObject* m_scriptedObject = nullptr;
    DataBind* m_dataBind = nullptr;

public:
    virtual void initScriptedValue();
    virtual bool validateForScriptInit() = 0;
    static ScriptInput* from(Core* component);
    DataBind* dataBind() { return m_dataBind; }
    void dataBind(DataBind* dataBind) { m_dataBind = dataBind; }
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

    int m_implementedMethods = 0;

protected:
    bool verifyImplementation(ScriptProtocol scriptProtocol,
                              LuaState* luaState);

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
};

class ScriptAsset : public ScriptAssetBase, public OptionalScriptedMethods
{

public:
#ifdef WITH_RIVE_SCRIPTING
    friend class ScriptAssetImporter;

    bool verified() const { return m_verified; }
    Span<uint8_t> bytecode() { return m_bytecode; }
#endif

    bool initScriptedObject(ScriptedObject* object);

    /// Sets the bytecode if the signature verifies.
    bool bytecode(Span<uint8_t> bytecode, Span<uint8_t> signature);

    /// Bytecode provided via decode should only happen with in-band bytecode.
    /// The signature will later be verified once file loading completes, so it
    /// is immediately marked as unverified.
    bool decode(SimpleArray<uint8_t>& data, Factory* factory) override;

    std::string fileExtension() const override { return "lua"; }
    void file(File* value) { m_file = value; }
    File* file() const { return m_file; }
#ifdef WITH_RIVE_SCRIPTING
    LuaState* vm()
    {
        if (m_file == nullptr)
        {
            return nullptr;
        }
        return m_file->scriptingVM();
    }
#endif

private:
    File* m_file = nullptr;
#ifdef WITH_RIVE_SCRIPTING
    bool m_verified = false;
    SimpleArray<uint8_t> m_bytecode;
    bool m_initted = false;
    ScriptProtocol m_scriptProtocol = ScriptProtocol::utility;
#endif

    bool initScriptedObjectWith(ScriptedObject* object);
};
} // namespace rive

#endif
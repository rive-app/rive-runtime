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
class File;
class ScriptedObject;

enum ScriptType
{
    none,
    drawing,
    layout
};

class ScriptInput
{
protected:
    ScriptedObject* m_scriptedObject = nullptr;

public:
    virtual void initScriptedValue();
    virtual bool validateForScriptInit() = 0;
    static ScriptInput* from(Core* component);
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

    int m_implementedMethods = 0;

protected:
    bool verifyImplementation(ScriptType scriptType, LuaState* luaState);

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
};

class ScriptAsset : public ScriptAssetBase, public OptionalScriptedMethods
{
private:
    File* m_file = nullptr;
    // SimpleArray<uint8_t>& m_bytes;
    bool initScriptedObjectWith(ScriptedObject* object);
#ifdef WITH_RIVE_SCRIPTING
    bool m_initted = false;
    ScriptType m_scriptType = ScriptType::none;
#endif

public:
    bool initScriptedObject(ScriptedObject* object);
    bool decode(SimpleArray<uint8_t>& data, Factory* factory) override
    {
        // m_bytes = data;
        return true;
    }
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
};
} // namespace rive

#endif
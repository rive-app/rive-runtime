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
    drawing
};

class ScriptInput
{
public:
    virtual void initScriptedValue() = 0;
    virtual bool validateForScriptInit() = 0;
    static ScriptInput* from(Component* component);
};

class ScriptAsset : public ScriptAssetBase
{
private:
    File* m_file = nullptr;
    // SimpleArray<uint8_t>& m_bytes;
    bool initScriptedObjectWith(ScriptedObject* object);

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
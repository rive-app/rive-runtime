#ifndef _RIVE_SCRIPTED_OBJECT_HPP_
#define _RIVE_SCRIPTED_OBJECT_HPP_
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/custom_property.hpp"
#include "rive/custom_property_container.hpp"
#include "rive/refcnt.hpp"
#include "rive/generated/assets/script_asset_base.hpp"
#include <stdio.h>
namespace rive
{
class Artboard;
class Component;
class DataContext;
class ScriptAsset;
class ViewModelInstanceValue;

class ScriptedObject : public FileAssetReferencer,
                       public CustomPropertyContainer
{
private:
#ifdef WITH_RIVE_SCRIPTING
    bool m_advances = false;
    bool m_updates = false;
#endif

protected:
    int m_self = 0;
#ifdef WITH_RIVE_SCRIPTING
    LuaState* m_state = nullptr;

    virtual void verifyInterface(LuaState* luaState);
#endif

public:
    ScriptAsset* scriptAsset() const;
    void setArtboardInput(std::string name, Artboard* artboard);
    void setBooleanInput(std::string name, bool value);
    void setIntegerInput(std::string name, int value);
    void setNumberInput(std::string name, float value);
    void setStringInput(std::string name, std::string value);
    void setViewModelInput(std::string name, ViewModelInstanceValue* value);
    void trigger(std::string name);
    bool scriptAdvance(float elapsedSeconds);
    void scriptUpdate();
    void reinit();
    virtual DataContext* dataContext() { return nullptr; }
#ifdef WITH_RIVE_SCRIPTING
    virtual bool scriptInit(LuaState* state);
#endif
    void scriptDispose();
    virtual bool addScriptedDirt(ComponentDirt value, bool recurse = false) = 0;
    void setAsset(rcp<FileAsset> asset) override;
    static ScriptedObject* from(Core* object);
};
} // namespace rive

#endif
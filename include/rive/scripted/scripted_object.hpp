#ifndef _RIVE_SCRIPTED_OBJECT_HPP_
#define _RIVE_SCRIPTED_OBJECT_HPP_
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/custom_property.hpp"
#include "rive/custom_property_container.hpp"
#include "rive/refcnt.hpp"
#include "rive/generated/assets/script_asset_base.hpp"
#include <stdio.h>

#ifdef WITH_RIVE_SCRIPTING
struct lua_State;
#endif

namespace rive
{
class Artboard;
class Component;
class DataContext;
class ViewModelInstanceValue;

class ScriptedObject : public FileAssetReferencer,
                       public CustomPropertyContainer,
                       public OptionalScriptedMethods
{
protected:
    int m_self = 0;
    int m_context = 0;
    virtual void disposeScriptInputs();
#ifdef WITH_RIVE_SCRIPTING
    lua_State* m_state = nullptr;
#endif
#ifdef WITH_RIVE_TOOLS
    bool hasValidVM();
#endif
private:
    DataContext* m_dataContext = nullptr;

public:
    virtual ~ScriptedObject() { scriptDispose(); }
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
    virtual void markNeedsUpdate();
    virtual DataContext* dataContext() { return m_dataContext; }
    void dataContext(DataContext* value) { m_dataContext = value; }
#ifdef WITH_RIVE_SCRIPTING
    virtual bool scriptInit(lua_State* state);
    lua_State* state() { return m_state; }
#endif
    void scriptDispose();
    virtual bool addScriptedDirt(ComponentDirt value, bool recurse = false) = 0;
    void setAsset(rcp<FileAsset> asset) override;
    static ScriptedObject* from(Core* object);
    virtual ScriptProtocol scriptProtocol() = 0;
    int self() { return m_self; }
    virtual Component* component() = 0;
    virtual ScriptedObject* cloneScriptedObject() const { return nullptr; }
};
} // namespace rive

#endif
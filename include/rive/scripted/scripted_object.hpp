#ifndef _RIVE_SCRIPTED_OBJECT_HPP_
#define _RIVE_SCRIPTED_OBJECT_HPP_
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/custom_property.hpp"
#include "rive/custom_property_container.hpp"
#include "rive/refcnt.hpp"
#include "rive/generated/assets/script_asset_base.hpp"
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/scripting_vm.hpp"
#endif
#include <algorithm>
#include <vector>

namespace rive
{
class Artboard;
class Component;
class DataContext;
class ViewModelInstanceValue;
class DataBindContainer;
class ScriptedProperty;
class ScriptedContext;

class ScriptedObject : public FileAssetReferencer,
                       public CustomPropertyContainer,
                       public OptionalScriptedMethods
{
protected:
    int m_self = 0;
    int m_context = 0;
    ScriptedContext* m_contextPtr = nullptr;
    virtual void disposeScriptInputs();
#ifdef WITH_RIVE_SCRIPTING
#ifdef WITH_RIVE_TOOLS
    rcp<ScriptingVM> m_vm = nullptr; // Ref-counted for editor
#else
    ScriptingVM* m_vm = nullptr; // Non-owning for runtime
#endif
#endif
private:
    rcp<DataContext> m_dataContext = nullptr;
    std::vector<ScriptedProperty*> m_trackedScriptedProperties;
#ifdef WITH_RIVE_SCRIPTING
    bool m_userLuaInitDone = false;
    bool tryLuaUserInit(lua_State* L);
#endif
    void disposeScriptedContext();
    void disposeTrackedProperties();

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
    void scriptDrawCanvas();
    void scriptUpdate();
    void reinit();
#ifdef WITH_RIVE_SCRIPTING
    bool userLuaInitDone() { return m_userLuaInitDone; }
#else
    bool userLuaInitDone() { return true; }
#endif
    virtual void markNeedsUpdate();
    virtual rcp<DataContext> dataContext() { return m_dataContext; }
    void dataContext(rcp<DataContext> value) { m_dataContext = value; }
#ifdef WITH_RIVE_SCRIPTING
    /// Load Lua factory result into m_self once per VM; does not push inputs or
    /// call Lua init(); use hydrateScriptInputs() after data bind.
    bool ensureScriptInitialized(ScriptingVM* vm);
    /// Resolve inputs from DataContext and push into Lua; runs Lua init() once
    /// after first successful hydration when implemented.
    bool hydrateScriptInputs();
    lua_State* state() const { return m_vm ? m_vm->state() : nullptr; }
#else
    bool hydrateScriptInputs() { return true; }
#endif
    void scriptDispose();
    virtual bool addScriptedDirt(ComponentDirt value, bool recurse = false) = 0;
    void setAsset(rcp<FileAsset> asset) override;
    static ScriptedObject* from(Core* object);
    virtual ScriptProtocol scriptProtocol() = 0;
    int self() { return m_self; }
    virtual Component* component() = 0;
    virtual ScriptedObject* cloneScriptedObject(DataBindContainer*) const
    {
        return nullptr;
    }
    void cloneProperties(CustomPropertyContainer*, DataBindContainer*) const;
    void addTrackedScriptedProperty(ScriptedProperty* property)
    {
        if (property != nullptr)
        {
            m_trackedScriptedProperties.push_back(property);
        }
    }
    void removeTrackedScriptedProperty(ScriptedProperty* property)
    {
        auto it = std::remove(m_trackedScriptedProperties.begin(),
                              m_trackedScriptedProperties.end(),
                              property);
        m_trackedScriptedProperties.erase(it,
                                          m_trackedScriptedProperties.end());
    }
    const std::vector<ScriptedProperty*>& trackedScriptedProperties() const
    {
        return m_trackedScriptedProperties;
    }
    virtual bool addDataBindFromScriptedObject(DataBind*) { return false; }
#ifdef WITH_RIVE_SCRIPTING
    /// Called after hydrateScriptInputs() succeeds;
    virtual void didHydrateScriptInputs() {}
#endif
};
} // namespace rive

#endif
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
#include <stdio.h>

namespace rive
{
class Artboard;
class Component;
class DataContext;
class ViewModelInstanceValue;
class DataBindContainer;

class ScriptedObject : public FileAssetReferencer,
                       public CustomPropertyContainer,
                       public OptionalScriptedMethods
{
protected:
    int m_self = 0;
    int m_context = 0;
    virtual void disposeScriptInputs();
#ifdef WITH_RIVE_SCRIPTING
#ifdef WITH_RIVE_TOOLS
    rcp<ScriptingVM> m_vm; // Ref-counted for editor
#else
    ScriptingVM* m_vm = nullptr; // Non-owning for runtime
#endif
#endif
private:
    rcp<DataContext> m_dataContext = nullptr;
    void disposeScriptedContext();

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
    virtual rcp<DataContext> dataContext() { return m_dataContext; }
    void dataContext(rcp<DataContext> value) { m_dataContext = value; }
#ifdef WITH_RIVE_SCRIPTING
    virtual bool scriptInit(ScriptingVM* vm);
    lua_State* state() const { return m_vm ? m_vm->state() : nullptr; }
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
};
} // namespace rive

#endif
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/viewmodel/viewmodel_property_number.hpp"
#include "rive/viewmodel/viewmodel_property_trigger.hpp"
#include "rive/viewmodel/viewmodel_property_list.hpp"

#include <math.h>
#include <stdio.h>

using namespace rive;

static void pushViewModelInstanceValue(lua_State* L,
                                       ViewModelInstanceValue* propValue)
{
    switch (propValue->coreType())
    {
        case ViewModelInstanceNumberBase::typeKey:
            lua_newrive<ScriptedPropertyNumber>(
                L,
                L,
                ref_rcp(propValue->as<ViewModelInstanceNumber>()));
            break;
        case ViewModelInstanceTriggerBase::typeKey:
            lua_newrive<ScriptedPropertyTrigger>(
                L,
                L,
                ref_rcp(propValue->as<ViewModelInstanceTrigger>()));
            break;
        case ViewModelInstanceListBase::typeKey:
            lua_newrive<ScriptedPropertyList>(
                L,
                L,
                ref_rcp(propValue->as<ViewModelInstanceList>()));
            break;
        default:
            lua_pushnil(L);
            break;
    }
}

// int ScriptedPropertyViewModel::pushValueOfType(const char* name, int
// coreType)
// {
//     auto vmi = m_instanceValue->referenceViewModelInstance();
//     if (!vmi)
//     {
//         lua_pushnil(m_state);
//     }
//     else
//     {
//         auto propValue = vmi->propertyValue(name);
//         if (propValue->coreType() != coreType)
//         {
//             lua_pushnil(m_state);
//         }
//         else
//         {
//             pushViewModelInstanceValue(m_state, propValue);
//         }
//     }
//     return 1;
// }

ScriptedProperty::~ScriptedProperty()
{
    m_instanceValue->removeDelegate(this);
    clearListeners();
}

ScriptedPropertyViewModel::~ScriptedPropertyViewModel()
{
    lua_unref(m_state, m_valueRef);
}

void ScriptedProperty::clearListeners()
{
    for (auto itr = m_listeners.begin(); itr != m_listeners.end(); ++itr)
    {
        const ScriptedListener& listener = *itr;
        lua_unref(m_state, listener.function);
        if (listener.userdata)
        {
            lua_unref(m_state, listener.userdata);
        }
    }
    m_listeners.clear();
}

ScriptedProperty::ScriptedProperty(lua_State* L,
                                   rcp<ViewModelInstanceValue> value) :
    m_state(L), m_instanceValue(std::move(value))
{
    m_instanceValue->addDelegate(this);
}

void ScriptedProperty::valueChanged()
{
    // This works because we don't actually call as we go (or we could
    // invalidate listeners if a callback registers a new or removes a
    // listener). Instead, we build up the call stack and then call for each
    // callback on the stack.
    for (auto itr = m_listeners.rbegin(); itr != m_listeners.rend(); itr++)
    {
        const ScriptedListener& listener = *itr;
        lua_rawgeti(m_state, LUA_REGISTRYINDEX, listener.function);
        if (listener.userdata)
        {
            lua_rawgeti(m_state, LUA_REGISTRYINDEX, listener.userdata);
        }
        else
        {
            // push nil so we have a constant number of args per call
            lua_pushnil(m_state);
        }
    }
    size_t calls = m_listeners.size();
    for (size_t i = 0; i < calls; i++)
    {
        lua_pcall(m_state, 1, 0, 0);
    }
}

int ScriptedProperty::addListener()
{
    if (lua_isfunction(m_state, 2))
    {
        int callbackRef = lua_ref(m_state, 2);
        m_listeners.push_back({callbackRef});
        return 0;
    }
    else if (lua_isfunction(m_state, 3))
    {
        int userdataRef = lua_ref(m_state, 2);
        int callbackRef = lua_ref(m_state, 3);
        m_listeners.push_back({callbackRef, userdataRef});
        return 0;
    }

    luaL_typeerrorL(m_state, 2, lua_typename(m_state, LUA_TFUNCTION));
    return 0;
}

int ScriptedProperty::removeListener()
{
    int checkIndex;
    if (lua_isfunction(m_state, 2))
    {
        checkIndex = 2;
    }
    else if (lua_isfunction(m_state, 3))
    {
        checkIndex = 3;
    }
    else
    {
        luaL_typeerrorL(m_state, 2, lua_typename(m_state, LUA_TFUNCTION));
    }

    for (auto itr = m_listeners.begin(); itr != m_listeners.end();)
    {
        const ScriptedListener& listener = *itr;
        lua_rawgeti(m_state, LUA_REGISTRYINDEX, listener.function);
        if (lua_rawequal(m_state, -1, checkIndex))
        {
            lua_unref(m_state, listener.function);
            if (listener.userdata)
            {
                lua_unref(m_state, listener.userdata);
            }
            itr = m_listeners.erase(itr);
        }
        else
        {
            ++itr;
        }
        lua_pop(m_state, 1);
    }
    return 0;
}

ScriptedPropertyViewModel::ScriptedPropertyViewModel(
    lua_State* L,
    rcp<ViewModel> viewModel,
    rcp<ViewModelInstanceViewModel> value) :
    ScriptedProperty(L, std::move(value)), m_viewModel(std::move(viewModel))
{}

ScriptedViewModel::ScriptedViewModel(lua_State* L,
                                     rcp<ViewModel> viewModel,
                                     rcp<ViewModelInstance> viewModelInstance) :
    m_state(L), m_viewModel(viewModel), m_viewModelInstance(viewModelInstance)
{}

ScriptedViewModel::~ScriptedViewModel()
{
    for (auto itr : m_propertyRefs)
    {
        lua_unref(m_state, itr.second);
    }
}

int ScriptedViewModel::pushValue(const char* name, int coreType)
{
    auto itr = m_propertyRefs.find(name);
    if (itr != m_propertyRefs.end())
    {
        lua_rawgeti(m_state, LUA_REGISTRYINDEX, itr->second);
        return 1;
    }
    // To be fully typesafe at runtime we should check the property in the
    // viewmodel and make sure the one in the value matches the same type or
    // return a default based on the type of the viewmodel, but this incurs
    // extra performance and since our type system at edit-time lets the user be
    // intentional, for now we runtime error if the type is un-expected and we
    // optimize for the best case.

    auto vmi = m_viewModelInstance;
    if (!vmi)
    {
        // Get type from viewmodel if we have one and user didn't request a type
        // dynamically.
        if (coreType != 0 || !m_viewModel)
        {
            lua_pushnil(m_state);
        }
        else
        {
            auto prop = m_viewModel->property(name);
            switch (prop->coreType())
            {
                case ViewModelPropertyNumberBase::typeKey:
                    lua_newrive<ScriptedPropertyNumber>(m_state,
                                                        m_state,
                                                        nullptr);
                    break;
                case ViewModelPropertyTriggerBase::typeKey:
                    lua_newrive<ScriptedPropertyTrigger>(m_state,
                                                         m_state,
                                                         nullptr);
                    break;
                case ViewModelPropertyListBase::typeKey:
                    lua_newrive<ScriptedPropertyList>(m_state,
                                                      m_state,
                                                      nullptr);
                    break;
            }
        }
    }
    else
    {
        auto propValue = vmi->propertyValue(name);
        if (propValue == nullptr ||
            (coreType != 0 && propValue->coreType() != coreType))
        {
            lua_pushnil(m_state);
        }
        else
        {
            pushViewModelInstanceValue(m_state, propValue);
        }
    }
    m_propertyRefs[name] = lua_ref(m_state, -1);
    return 1;
}

int ScriptedPropertyViewModel::pushValue()
{
    // N.B. we'll need to invalidate m_valueRef if the viewmodel's value on the
    // property changes.
    if (m_valueRef != 0)
    {
        lua_rawgeti(m_state, LUA_REGISTRYINDEX, m_valueRef);
        return 1;
    }
    if (m_instanceValue)
    {
        lua_newrive<ScriptedViewModel>(
            m_state,
            m_state,
            m_viewModel,
            m_instanceValue->as<ViewModelInstanceViewModel>()
                ->referenceViewModelInstance());
    }
    else
    {
        // Push nil or push an empty model ref? For now empty/with def values.
        lua_newrive<ScriptedViewModel>(m_state, m_state, m_viewModel, nullptr);
    }
    m_valueRef = lua_ref(m_state, -1);
    return 1;
}

static int property_vm_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto vmProp = lua_torive<ScriptedPropertyViewModel>(L, 1);
    assert(vmProp->state() == L);
    switch (atom)
    {
        case (int)LuaAtoms::value:
            return vmProp->pushValue();
        default:
            return 0;
    }
}

static int vm_index(lua_State* L)
{
    auto vm = lua_torive<ScriptedViewModel>(L, 1);
    size_t namelen = 0;
    const char* name = luaL_checklstring(L, 2, &namelen);
    assert(vm->state() == L);
    return vm->pushValue(name);
}

static int property_namecall_atom(lua_State* L,
                                  ScriptedProperty* property,
                                  int atom,
                                  bool& error)
{
    switch (atom)
    {
        case (int)LuaAtoms::addListener:
        {
            assert(property->state() == L);
            return property->addListener();
        }
        case (int)LuaAtoms::removeListener:
        {
            assert(property->state() == L);
            return property->removeListener();
        }
        case (int)LuaAtoms::fire:
        {
            auto value = property->instanceValue();
            if (value != nullptr && value->is<ViewModelInstanceTrigger>())
            {
                value->as<ViewModelInstanceTrigger>()->trigger();
                return 0;
            }
        }
    }
    error = true;
    return 0;
}

static int vm_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        auto vm = lua_torive<ScriptedViewModel>(L, 1);
        switch (atom)
        {
            case (int)LuaAtoms::getNumber:
            {
                size_t namelen = 0;
                const char* name = luaL_checklstring(L, 2, &namelen);
                assert(vm->state() == L);
                return vm->pushValue(name,
                                     ViewModelInstanceNumberBase::typeKey);
            }
            case (int)LuaAtoms::getTrigger:
            {
                size_t namelen = 0;
                const char* name = luaL_checklstring(L, 2, &namelen);
                assert(vm->state() == L);
                return vm->pushValue(name,
                                     ViewModelInstanceTriggerBase::typeKey);
            }
            default:
                break;
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedPropertyViewModel::luaName);
    return 0;
}

static int property_vm_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        auto vm = lua_torive<ScriptedPropertyViewModel>(L, 1);

        bool error = false;
        int stackChange = property_namecall_atom(L, vm, atom, error);
        if (!error)
        {
            return stackChange;
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedPropertyViewModel::luaName);
    return 0;
}

static int property_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    const char* name = "Property";
    if (str != nullptr)
    {
        auto tag = lua_userdatatag(L, 1);
        switch (tag)
        {
            case ScriptedPropertyNumber::luaTag:
                name = ScriptedPropertyNumber::luaName;
                break;
            case ScriptedPropertyTrigger::luaTag:
                name = ScriptedPropertyTrigger::luaName;
                break;
            default:
                luaL_typeerror(L, 1, name);
                break;
        }
        auto vm = (ScriptedProperty*)lua_touserdata(L, 1);
        bool error = false;
        int stackChange = property_namecall_atom(L, vm, atom, error);
        if (!error)
        {
            return stackChange;
        }
    }
    luaL_error(L, "%s is not a valid method of %s", str, name);
    return 0;
}

ScriptedPropertyNumber::ScriptedPropertyNumber(
    lua_State* L,
    rcp<ViewModelInstanceNumber> value) :
    ScriptedProperty(L, std::move(value))
{}

ScriptedPropertyTrigger::ScriptedPropertyTrigger(
    lua_State* L,
    rcp<ViewModelInstanceTrigger> value) :
    ScriptedProperty(L, std::move(value))
{}

void ScriptedPropertyNumber::setValue(float value)
{
    if (m_instanceValue)
    {
        m_instanceValue->as<ViewModelInstanceNumber>()->propertyValue(value);
    }
}

int ScriptedPropertyNumber::pushValue()
{
    if (m_instanceValue)
    {
        lua_pushnumber(
            m_state,
            m_instanceValue->as<ViewModelInstanceNumber>()->propertyValue());
    }
    else
    {
        lua_pushnumber(m_state, 0);
    }
    return 1;
}

ScriptedPropertyList::ScriptedPropertyList(lua_State* L,
                                           rcp<ViewModelInstanceList> value) :
    ScriptedProperty(L, std::move(value))
{}

ScriptedPropertyList::~ScriptedPropertyList()
{
    for (const auto& pair : m_propertyRefs)
    {
        lua_unref(m_state, pair.second);
    }
}

void ScriptedPropertyList::valueChanged()
{
    m_changed = true;
    ScriptedProperty::valueChanged();
}

int ScriptedPropertyList::pushLength()
{
    if (m_instanceValue)
    {
        lua_pushinteger(m_state,
                        (int)m_instanceValue->as<ViewModelInstanceList>()
                            ->listItems()
                            .size());
    }
    else
    {
        lua_pushinteger(m_state, 0);
    }
    return 1;
}

int ScriptedPropertyList::pushValue(int index)
{
    if (m_instanceValue)
    {
        auto items = m_instanceValue->as<ViewModelInstanceList>()->listItems();

        if (m_changed)
        {
            std::unordered_map<ViewModelInstance*, int> refs;

            // re-validate references.
            for (auto& item : items)
            {
                auto vmi = item->viewModelInstance().get();
                auto itr = m_propertyRefs.find(vmi);
                if (itr != m_propertyRefs.end())
                {
                    refs[vmi] = itr->second;
                    m_propertyRefs.erase(itr);
                }
            }

            for (const auto& pair : m_propertyRefs)
            {
                lua_unref(m_state, pair.second);
            }
            m_propertyRefs = refs;

            m_changed = false;
        }

        if (index >= items.size())
        {
            lua_pushnil(m_state);
        }
        else
        {

            auto listItem = items[index];
            auto vmi = listItem->viewModelInstance();
            auto itr = m_propertyRefs.find(vmi.get());
            if (itr != m_propertyRefs.end())
            {
                lua_rawgeti(m_state, LUA_REGISTRYINDEX, itr->second);
            }
            else
            {
                lua_newrive<ScriptedViewModel>(m_state,
                                               m_state,
                                               ref_rcp(vmi->viewModel()),
                                               vmi);
                m_propertyRefs[vmi.get()] = lua_ref(m_state, -1);
            }
        }
    }
    else
    {
        lua_pushnil(m_state);
    }
    return 1;
}

static int property_list_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    auto propertyList = lua_torive<ScriptedPropertyList>(L, 1);
    // if it's not an atom it should be an index into the array
    if (!key)
    {
        int index = luaL_checkinteger(L, 2);
        return propertyList->pushValue(index - 1);
    }

    switch (atom)
    {
        case (int)LuaAtoms::length:
            assert(propertyList->state() == L);
            return propertyList->pushLength();
        default:
            return 0;
    }
}

static int property_number_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto propertyNumber = lua_torive<ScriptedPropertyNumber>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::value:
            assert(propertyNumber->state() == L);
            return propertyNumber->pushValue();
        default:
            return 0;
    }
}

static int property_number_newindex(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto propertyNumber = lua_torive<ScriptedPropertyNumber>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::value:
            propertyNumber->setValue(float(luaL_checknumber(L, 3)));
        default:
            return 0;
    }

    return 0;
}

int luaopen_rive_properties(lua_State* L)
{
    {
        lua_register_rive<ScriptedViewModel>(L);

        lua_pushcfunction(L, vm_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, vm_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }

    {
        lua_register_rive<ScriptedPropertyViewModel>(L);

        lua_pushcfunction(L, property_vm_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, property_vm_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable
    }

    {
        lua_register_rive<ScriptedPropertyNumber>(L);

        lua_pushcfunction(L, property_number_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, property_number_newindex, nullptr);
        lua_setfield(L, -2, "__newindex");

        lua_pushcfunction(L, property_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable}
    }

    {
        lua_register_rive<ScriptedPropertyTrigger>(L);

        lua_pushcfunction(L, property_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable}
    }

    {
        lua_register_rive<ScriptedPropertyList>(L);

        lua_pushcfunction(L, property_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");

        lua_pushcfunction(L, property_list_index, nullptr);
        lua_setfield(L, -2, "__index");

        lua_setreadonly(L, -1, true);
        lua_pop(L, 1); // pop the metatable}
    }
    return 5;
}
#endif

#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel_property_number.hpp"
#include "rive/viewmodel/viewmodel_property_trigger.hpp"
#include "rive/node.hpp"
#include "rive/bones/root_bone.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/math/transform_components.hpp"

#include <math.h>
#include <stdio.h>

using namespace rive;

ScriptReffedArtboard::ScriptReffedArtboard(
    rcp<File> file,
    std::unique_ptr<ArtboardInstance>&& artboardInstance) :
    m_file(file),
    m_artboard(std::move(artboardInstance)),
    m_stateMachine(m_artboard->defaultStateMachine())
{
    m_viewModelInstance = m_file->createViewModelInstance(m_artboard.get());
    if (m_stateMachine && m_viewModelInstance)
    {
        m_stateMachine->bindViewModelInstance(m_viewModelInstance);
    }
}

ScriptReffedArtboard::~ScriptReffedArtboard()
{
    // Make sure artboard is deleted before file.
    m_artboard = nullptr;
    m_file = nullptr;
}

rive::rcp<rive::File> ScriptReffedArtboard::file() { return m_file; }

Artboard* ScriptReffedArtboard::artboard() { return m_artboard.get(); }

StateMachineInstance* ScriptReffedArtboard::stateMachine()
{
    return m_stateMachine.get();
}

static int artboard_draw(lua_State* L)
{
    auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
    auto scriptedRenderer = lua_torive<ScriptedRenderer>(L, 2);

    auto renderer = scriptedRenderer->validate(L);
    scriptedArtboard->artboard()->draw(renderer);

    return 0;
}

bool ScriptedArtboard::advance(float seconds)
{
    auto machine = stateMachine();
    if (machine)
    {
        return machine->advanceAndApply(seconds);
    }
    else
    {
        return artboard()->advance(seconds);
    }
}

static int apply_pointer_event(lua_State* L, int atom)
{
    auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
    auto pointerEvent = lua_torive<ScriptedPointerEvent>(L, 2);
    auto result = 0;
    if (scriptedArtboard->stateMachine())
    {
        switch (atom)
        {
            case (int)LuaAtoms::pointerDown:
                result = (int)scriptedArtboard->stateMachine()->pointerDown(
                    pointerEvent->m_position,
                    pointerEvent->m_id);
                break;
            case (int)LuaAtoms::pointerMove:
                result = (int)scriptedArtboard->stateMachine()->pointerMove(
                    pointerEvent->m_position,
                    0,
                    pointerEvent->m_id);
                break;
            case (int)LuaAtoms::pointerUp:
                result = (int)scriptedArtboard->stateMachine()->pointerUp(
                    pointerEvent->m_position,
                    pointerEvent->m_id);
                break;
            case (int)LuaAtoms::pointerExit:
                result = (int)scriptedArtboard->stateMachine()->pointerExit(
                    pointerEvent->m_position,
                    pointerEvent->m_id);
                break;
        }
    }
    lua_pushinteger(L, result);
    return 1;
}

static int artboard_advance(lua_State* L)
{
    auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
    auto seconds = float(luaL_checknumber(L, 2));

    bool advanced = scriptedArtboard->advance(seconds);
    lua_pushboolean(L, advanced ? 1 : 0);

    return 1;
}

static int artboard_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);

    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::draw:
                return artboard_draw(L);
            case (int)LuaAtoms::advance:
                return artboard_advance(L);
            case (int)LuaAtoms::instance:
            {
                auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
                return scriptedArtboard->instance(L);
            }
            case (int)LuaAtoms::addToPath:
            {
                int nargs = lua_gettop(L);
                auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
                auto scriptedPath = lua_torive<ScriptedPath>(L, 2);
                Mat2D* transform = nullptr;
                if (nargs == 3)
                {
                    auto matrix = lua_torive<ScriptedMat2D>(L, 3);
                    transform = &matrix->value;
                }

                scriptedArtboard->scriptReffedArtboard()
                    ->artboard()
                    ->addToRawPath(scriptedPath->rawPath, transform);
                scriptedPath->markDirty();

                return 0;
            }
            case (int)LuaAtoms::bounds:
            {
                auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
                const auto& bounds = scriptedArtboard->artboard()->bounds();
                lua_pushvec2d(L, bounds.min());
                lua_pushvec2d(L, bounds.max());
                return 2;
            }
            case (int)LuaAtoms::node:
            {
                auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
                const char* name = luaL_checkstring(L, 2);
                auto component =
                    scriptedArtboard->artboard()->find<TransformComponent>(
                        name);
                if (component == nullptr)
                {
                    lua_pushnil(L);
                    return 1;
                }
                lua_newrive<ScriptedNode>(
                    L,
                    scriptedArtboard->scriptReffedArtboard(),
                    component);
                return 1;
            }
            case (int)LuaAtoms::pointerDown:
            case (int)LuaAtoms::pointerUp:
            case (int)LuaAtoms::pointerMove:
            case (int)LuaAtoms::pointerExit:
            {
                return apply_pointer_event(L, atom);
            }
        }
    }

    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedArtboard::luaName);
    return 0;
}

int ScriptedArtboard::pushData(lua_State* L)
{
    if (m_dataRef != 0)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_dataRef);
        return 1;
    }
    auto vm = viewModelInstance();
    if (!vm)
    {
        lua_pushnil(L);
    }
    else
    {
        lua_newrive<ScriptedViewModel>(L, L, ref_rcp(vm->viewModel()), vm);
    }
    m_dataRef = lua_ref(L, -1);

    return 1;
}

int ScriptedArtboard::instance(lua_State* L)
{
    lua_newrive<ScriptedArtboard>(L,
                                  m_scriptReffedArtboard->file(),
                                  artboard()->instance());
    return 1;
}

ScriptedNode::ScriptedNode(rcp<ScriptReffedArtboard> artboard,
                           TransformComponent* component) :
    m_artboard(artboard), m_component(component)
{}

static int artboard_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::frameOrigin:
            lua_pushboolean(L,
                            scriptedArtboard->artboard()->frameOrigin() ? 1
                                                                        : 0);
            return 1;

        case (int)LuaAtoms::width:
            lua_pushnumber(L, scriptedArtboard->artboard()->width());
            return 1;
        case (int)LuaAtoms::height:
            lua_pushnumber(L, scriptedArtboard->artboard()->height());
            return 1;
        case (int)LuaAtoms::data:
            return scriptedArtboard->pushData(L);

        default:
            break;
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedArtboard::luaName);
    return 0;
}

static int artboard_newindex(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::frameOrigin:
            scriptedArtboard->artboard()->frameOrigin(luaL_checkboolean(L, 3) !=
                                                      0);
            return 0;
        case (int)LuaAtoms::width:
            scriptedArtboard->artboard()->width(luaL_checknumber(L, 3));
            return 0;
        case (int)LuaAtoms::height:
            scriptedArtboard->artboard()->height(luaL_checknumber(L, 3));
            return 0;
        default:
            break;
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedArtboard::luaName);

    return 0;
}

ScriptedArtboard::ScriptedArtboard(
    rcp<File> file,
    std::unique_ptr<ArtboardInstance>&& artboardInstance) :
    m_scriptReffedArtboard(
        make_rcp<ScriptReffedArtboard>(file, std::move(artboardInstance)))
{}

ScriptedArtboard::~ScriptedArtboard() { m_scriptReffedArtboard = nullptr; }

void ScriptedArtboard::cleanupDataRef(lua_State* L)
{
    if (m_dataRef != 0)
    {
        lua_unref(L, m_dataRef);
        m_dataRef = 0;
    }
}

static int node_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedNode = lua_torive<ScriptedNode>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::position:
            lua_pushvector2(L,
                            scriptedNode->component()->x(),
                            scriptedNode->component()->y());
            return 1;

        case (int)LuaAtoms::rotation:
            lua_pushnumber(L, scriptedNode->component()->rotation());
            return 1;

        case (int)LuaAtoms::scale:
            lua_pushvector2(L,
                            scriptedNode->component()->scaleX(),
                            scriptedNode->component()->scaleY());
            return 1;

        case (int)LuaAtoms::scaleX:
            lua_pushnumber(L, scriptedNode->component()->scaleX());
            return 1;

        case (int)LuaAtoms::scaleY:
            lua_pushnumber(L, scriptedNode->component()->scaleY());
            return 1;

        case (int)LuaAtoms::worldTransform:
            lua_newrive<ScriptedMat2D>(
                L,
                scriptedNode->component()->worldTransform());
            return 1;

        case (int)LuaAtoms::children:
        {
            auto component = scriptedNode->component();
            if (component->is<ContainerComponent>())
            {
                auto container = component->as<ContainerComponent>();
                // speculative pre-alloc the table
                auto& children = container->children();
                lua_createtable(L, (int)children.size(), 0);

                int index = 1;
                for (auto child : children)
                {
                    if (child->is<TransformComponent>())
                    {
                        lua_newrive<ScriptedNode>(
                            L,
                            scriptedNode->artboard(),
                            child->as<TransformComponent>());
                        // Set table[i] = value, pops the value
                        lua_rawseti(L, -2, index++);
                    }
                }
            }
            return 1;
        }

        case (int)LuaAtoms::parent:
        {
            auto parent = scriptedNode->component()->parent();
            if (parent != nullptr && parent->is<TransformComponent>())
            {
                lua_newrive<ScriptedNode>(L,
                                          scriptedNode->artboard(),
                                          parent->as<TransformComponent>());
            }
            else
            {
                lua_pushnil(L);
            }
            return 1;
        }

        default:
            switch (key[0])
            {
                case 'x':
                    lua_pushnumber(L, scriptedNode->component()->x());
                    return 1;
                case 'y':
                    lua_pushnumber(L, scriptedNode->component()->y());
                    return 1;
            }
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedNode::luaName);
    return 0;
}

static int node_newindex(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedNode = lua_torive<ScriptedNode>(L, 1);
    auto component = scriptedNode->component();
    switch (atom)
    {
        case (int)LuaAtoms::position:
        {
            const float* vec = luaL_checkvector(L, 3);

            if (component->is<Node>())
            {
                Node* node = component->as<Node>();
                node->x(vec[0]);
                node->y(vec[1]);
            }
            else if (component->is<RootBone>())
            {
                RootBone* bone = component->as<RootBone>();
                bone->x(vec[0]);
                bone->y(vec[1]);
            }
            return 0;
        }
        case (int)LuaAtoms::rotation:
            component->rotation(float(luaL_checknumber(L, 3)));
            return 0;

        case (int)LuaAtoms::scale:
        {
            const float* vec = luaL_checkvector(L, 3);
            component->scaleX(vec[0]);
            component->scaleY(vec[1]);
            return 0;
        }

        case (int)LuaAtoms::scaleX:
            component->scaleX(float(luaL_checknumber(L, 3)));
            return 0;

        case (int)LuaAtoms::scaleY:
            component->scaleY(float(luaL_checknumber(L, 3)));
            return 0;

        case (int)LuaAtoms::worldTransform:
        {
            Mat2D& transform = component->mutableWorldTransform();
            transform = *(Mat2D*)lua_torive<ScriptedMat2D>(L, 3);
            return 0;
        }

        default:
            switch (key[0])
            {
                case 'x':
                    if (component->is<Node>())
                    {
                        Node* node = component->as<Node>();
                        node->x(float(luaL_checknumber(L, 3)));
                    }
                    else if (component->is<RootBone>())
                    {
                        RootBone* bone = component->as<RootBone>();
                        bone->x(float(luaL_checknumber(L, 3)));
                    }
                    return 0;
                case 'y':
                    if (component->is<Node>())
                    {
                        Node* node = component->as<Node>();
                        node->y(float(luaL_checknumber(L, 3)));
                    }
                    else if (component->is<RootBone>())
                    {
                        RootBone* bone = component->as<RootBone>();
                        bone->y(float(luaL_checknumber(L, 3)));
                    }
                    return 0;
            }
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedNode::luaName);
    return 0;
}

static int node_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::decompose:
            {
                auto scriptedNode = lua_torive<ScriptedNode>(L, 1);
                auto component = scriptedNode->component();
                Mat2D world = getParentWorld(*component).invertOrIdentity() *
                              (*(Mat2D*)lua_torive<ScriptedMat2D>(L, 2));
                TransformComponents components = world.decompose();
                if (component->is<Node>())
                {
                    Node* node = component->as<Node>();
                    node->x(components.x());
                    node->y(components.y());
                }
                else if (component->is<RootBone>())
                {
                    RootBone* bone = component->as<RootBone>();
                    bone->x(components.x());
                    bone->y(components.y());
                }
                component->scaleX(components.scaleX());
                component->scaleY(components.scaleY());
                component->rotation(components.rotation());
                return 0;
            }
        }
    }

    luaL_error(L, "%s is not a valid method of %s", str, ScriptedNode::luaName);
    return 0;
}

static void scripted_artboard_dtor(lua_State* L, void* data)
{
    ScriptedArtboard* artboard = static_cast<ScriptedArtboard*>(data);
    // Clean up m_dataRef before calling the C++ destructor
    // (we can't do this in ~ScriptedArtboard() because it doesn't have access
    // to lua_State*)
    artboard->cleanupDataRef(L);
    // Call the C++ destructor
    artboard->~ScriptedArtboard();
}

int luaopen_rive_artboards(lua_State* L)
{
    lua_register_rive<ScriptedArtboard>(L);
    // Override the default destructor to clean up m_dataRef first
    lua_setuserdatadtor(L, ScriptedArtboard::luaTag, scripted_artboard_dtor);

    lua_pushcfunction(L, artboard_index, nullptr);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, artboard_newindex, nullptr);
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, artboard_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable

    lua_register_rive<ScriptedNode>(L);

    lua_pushcfunction(L, node_index, nullptr);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, node_newindex, nullptr);
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, node_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable

    return 0;
}
#endif

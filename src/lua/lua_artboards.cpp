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
    std::unique_ptr<ArtboardInstance>&& artboardInstance,
    rcp<ViewModelInstance> viewModelInstance,
    rcp<DataContext> parentDataContext) :
    m_file(file),
    m_artboard(std::move(artboardInstance)),
    m_stateMachine(m_artboard->defaultStateMachine())
{
    if (viewModelInstance)
    {
        m_viewModelInstance = viewModelInstance;
    }
    else
    {
        m_viewModelInstance = m_file->createViewModelInstance(m_artboard.get());
    }
    if (m_stateMachine && m_viewModelInstance)
    {
        if (parentDataContext)
        {
            auto dataContext = make_rcp<DataContext>(m_viewModelInstance);
            dataContext->parent(parentDataContext);
            m_stateMachine->bindDataContext(dataContext);
        }
        else
        {
            m_stateMachine->bindViewModelInstance(m_viewModelInstance);
        }
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
                int nargs = lua_gettop(L);
                auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
                if (nargs == 2)
                {

                    auto scriptedViewModel =
                        lua_torive<ScriptedViewModel>(L, 2);
                    return scriptedArtboard->instance(
                        L,
                        scriptedViewModel->viewModelInstance());
                }
                return scriptedArtboard->instance(L, nullptr);
            }
            case (int)LuaAtoms::animation:
            {
                auto scriptedArtboard = lua_torive<ScriptedArtboard>(L, 1);
                const char* animationName = luaL_checkstring(L, 2);
                return scriptedArtboard->animation(L, animationName);
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

int ScriptedArtboard::instance(lua_State* L,
                               rcp<ViewModelInstance> viewModelInstance)
{
    auto artboardInstance = artboard()->instance();
    artboardInstance->frameOrigin(false);
    lua_newrive<ScriptedArtboard>(L,
                                  L,
                                  m_scriptReffedArtboard->file(),
                                  std::move(artboardInstance),
                                  viewModelInstance,
                                  m_dataContext);
    return 1;
}

int ScriptedArtboard::animation(lua_State* L, const char* animationName)
{
    if (artboard()->isInstance())
    {

        auto animation = static_cast<ArtboardInstance*>(artboard())
                             ->animationNamed(animationName);
        if (animation)
        {
            lua_newrive<ScriptedAnimation>(L, L, std::move(animation));
            return 1;
        }
    }
    return 0;
}

ScriptedNode::ScriptedNode(rcp<ScriptReffedArtboard> artboard,
                           TransformComponent* component) :
    m_artboard(artboard), m_component(component)
{}

const ShapePaint* ScriptedNode::shapePaint()
{
    if (m_shapePaint)
    {
        return m_shapePaint;
    }
    if (m_component->is<ShapePaint>())
    {
        return m_component->as<ShapePaint>();
    }
    return nullptr;
}

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
    lua_State* L,
    rcp<File> file,
    std::unique_ptr<ArtboardInstance>&& artboardInstance,
    rcp<ViewModelInstance> viewModelInstance,
    rcp<DataContext> dataContext) :
    m_state(L),
    m_scriptReffedArtboard(
        make_rcp<ScriptReffedArtboard>(file,
                                       std::move(artboardInstance),
                                       viewModelInstance,
                                       dataContext)),
    m_dataContext(dataContext)
{}

ScriptedArtboard::~ScriptedArtboard()
{
    lua_unref(m_state, m_dataRef);
    m_scriptReffedArtboard = nullptr;
}

ScriptedAnimation::ScriptedAnimation(
    lua_State* L,
    std::unique_ptr<LinearAnimationInstance> animation) :
    m_state(L), m_animation(std::move(animation))
{}

float ScriptedAnimation::duration()
{
    auto durationFrames = (float)m_animation->duration();
    auto fps = (float)m_animation->fps();
    return durationFrames / fps;
}

int ScriptedAnimation::advance()
{
    auto seconds = float(luaL_checknumber(m_state, 2));

    bool advanced = m_animation->advance(seconds);
    m_animation->apply();
    lua_pushboolean(m_state, advanced ? 1 : 0);
    return 1;
}

int ScriptedAnimation::setTime(std::string mode)
{
    float seconds = 0;
    if (mode == "seconds")
    {
        seconds = float(luaL_checknumber(m_state, 2));
    }
    else if (mode == "frames")
    {
        auto frames = float(luaL_checknumber(m_state, 2));

        seconds = frames / m_animation->fps();
    }
    else if (mode == "percentage")
    {
        auto percentage = float(luaL_checknumber(m_state, 2));
        seconds = percentage * duration();
    }

    m_animation->time(m_animation->animation()->globalToLocalSeconds(seconds));
    m_animation->apply();
    return 0;
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
        case (int)LuaAtoms::paint:
        {
            auto shapePaint = scriptedNode->shapePaint();
            if (shapePaint)
            {
                lua_newrive<ScriptedPaintData>(L, shapePaint);
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

            case (int)LuaAtoms::asPath:
            {

                auto scriptedNode = lua_torive<ScriptedNode>(L, 1);
                auto component = scriptedNode->component();
                if (component->is<Path>())
                {
                    lua_newrive<ScriptedPathData>(
                        L,
                        &component->as<Path>()->rawPath());
                }
                else
                {
                    lua_pushnil(L);
                }
                return 1;
            }

            case (int)LuaAtoms::asPaint:
            {
                auto scriptedNode = lua_torive<ScriptedNode>(L, 1);
                auto shapePaint = scriptedNode->shapePaint();
                if (shapePaint)
                {
                    lua_newrive<ScriptedPaintData>(L, shapePaint);
                }
                else
                {
                    lua_pushnil(L);
                }
                return 1;
            }
        }
    }

    luaL_error(L, "%s is not a valid method of %s", str, ScriptedNode::luaName);
    return 0;
}

static int animation_index(lua_State* L)
{
    int atom;
    const char* key = lua_tostringatom(L, 2, &atom);
    if (!key)
    {
        luaL_typeerrorL(L, 2, lua_typename(L, LUA_TSTRING));
        return 0;
    }

    auto scriptedAnimation = lua_torive<ScriptedAnimation>(L, 1);
    switch (atom)
    {
        case (int)LuaAtoms::duration:
            lua_pushnumber(L, scriptedAnimation->duration());
            return 1;
    }

    luaL_error(L,
               "'%s' is not a valid index of %s",
               key,
               ScriptedNode::luaName);
    return 0;
}

static int animation_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        auto scriptedAnimation = lua_torive<ScriptedAnimation>(L, 1);
        switch (atom)
        {
            case (int)LuaAtoms::advance:
            {
                return scriptedAnimation->advance();
            }
            case (int)LuaAtoms::setTime:
            {
                return scriptedAnimation->setTime("seconds");
            }
            case (int)LuaAtoms::setTimeFrames:
            {
                return scriptedAnimation->setTime("frames");
            }
            case (int)LuaAtoms::setTimePercentage:
            {
                return scriptedAnimation->setTime("percentage");
            }
        }
    }

    luaL_error(L, "%s is not a valid method of %s", str, ScriptedNode::luaName);
    return 0;
}

int luaopen_rive_artboards(lua_State* L)
{
    lua_register_rive<ScriptedArtboard>(L);

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

    lua_register_rive<ScriptedAnimation>(L);
    lua_pushcfunction(L, animation_index, nullptr);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, animation_namecall, nullptr);
    lua_setfield(L, -2, "__namecall");
    lua_pop(L, 1); // pop the metatable

    return 0;
}
#endif

#ifdef WITH_RIVE_SCRIPTING
// ScriptingVM's implementation of the ScriptBackend seam: the Lua mechanics
// that used to live inline in the scripted core objects.
#include "rive/lua/rive_lua_libs.hpp"

#include "rive/animation/listener_invocation.hpp"
#include "rive/artboard.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/file.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/shapes/paint/shape_paint.hpp"

using namespace rive;

void ScriptingVM::releaseRef(int ref)
{
    if (m_state != nullptr && ref != 0)
    {
        lua_unref(m_state, ref);
    }
}

int ScriptingVM::resolveGeneratorRef(int ref) const
{
#ifdef WITH_RIVE_TOOLS
    if (m_state != nullptr)
    {
        auto context =
            static_cast<ScriptingContext*>(lua_getthreaddata(m_state));
        if (context != nullptr && context->hasGeneratorRef(ref))
        {
            return context->getGeneratorRef(ref);
        }
    }
#endif
    return ref;
}

int ScriptingVM::instantiate(int generatorRef,
                             ScriptedObject* object,
                             int* outContextRef,
                             ScriptedContext** outContextPtr)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, generatorRef);

    // Create the Context userdata before calling the generator so scripts
    // can request resources at construction time (e.g.
    // `return { canvas = context:gpuCanvas() }`). The same Context is reused
    // when init(self, context) is called, so a script only ever sees one
    // Context per scripted-object lifetime.
    // Stack: [generator]
    ScriptedContext* contextPtr = lua_newrive<ScriptedContext>(L, object);
    // Stack: [generator, context]
    int contextRef = lua_ref(L, -1);
    // Stack: [generator, context]  (lua_ref does not pop)

    auto disposeContext = [&]() {
        contextPtr->clearScriptedObject();
        lua_unref(L, contextRef);
    };

    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 1, 1)) !=
        LUA_OK)
    {
        disposeContext();
        rive_lua_pop(L, 1);
        return 0;
    }
    if (static_cast<lua_Type>(lua_type(L, -1)) != LUA_TTABLE)
    {
        disposeContext();
        rive_lua_pop(L, 1);
        return 0;
    }
    int selfRef = lua_ref(L, -1);
    rive_lua_pop(L, 1);
    *outContextRef = contextRef;
    *outContextPtr = contextPtr;
    return selfRef;
}

ScriptBackend::InitResult ScriptingVM::callUserInit(ScriptedObject* object,
                                                    int selfRef,
                                                    int contextRef)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    if (static_cast<lua_Type>(lua_getfield(L, -1, "init")) != LUA_TFUNCTION)
    {
        // init is optional and not implemented (assumed for legacy files);
        // nothing to run — the object is considered initialized.
        rive_lua_pop(L, 2); // non-function field + self
        return InitResult::notImplemented;
    }
    // Stack: [self, field]
    lua_pushvalue(L, -2);
    // Stack: [self, field, self]
    rive_lua_pushRef(L, contextRef);
    // Stack: [self, field, self, ScriptedContext]
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 2, 1)) !=
        LUA_OK)
    {
        // Stack: [self, status]
        rive_lua_pop(L, 2);
        return InitResult::failed;
    }
    if (!lua_toboolean(L, -1) || object->m_contextPtr->missingRequestedData())
    {
        rive_lua_pop(L, 2);
        return InitResult::failed;
    }
    // Stack: [self, result]
    rive_lua_pop(L, 1);
    assert(static_cast<lua_Type>(lua_type(L, -1)) == LUA_TTABLE);
    // Stack: [self]
    rive_lua_pop(L, 1);
    return InitResult::succeeded;
}

bool ScriptingVM::callAdvance(ScriptedObject* object,
                              int selfRef,
                              float elapsedSeconds)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    // implementedMethods may be assumed for legacy files (all-bits default);
    // if the field isn't actually a function, treat it as not implemented.
    if (static_cast<lua_Type>(lua_getfield(L, -1, "advance")) != LUA_TFUNCTION)
    {
        rive_lua_pop(L, 2); // non-function field + self
        return false;
    }
    lua_pushvalue(L, -2);
    lua_pushnumber(L, elapsedSeconds);
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 2, 1)) !=
        LUA_OK)
    {
        rive_lua_pop(L, 2);
        return false;
    }
    bool result = lua_toboolean(L, -1);
    rive_lua_pop(L, 2);
    return result;
}

void ScriptingVM::callUpdate(ScriptedObject* object, int selfRef)
{
    lua_State* L = m_state;
    // Stack: []
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    if (static_cast<lua_Type>(lua_getfield(L, -1, "update")) != LUA_TFUNCTION)
    {
        // Not actually implemented (assumed for legacy files); no-op. The
        // update phase never started, so there's no flag to reset.
        rive_lua_pop(L, 2); // non-function field + self
        return;
    }
    // Only inside the update phase while the callback actually runs.
    object->setInUpdatePhase(true);
    // Stack: [self, field] Swap self and field
    lua_insert(L, -2);
    // Stack: [field, self]
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 1, 0)) !=
        LUA_OK)
    {
        rive_lua_pop(L, 1);
    }
    object->setInUpdatePhase(false);
}

void ScriptingVM::callTrigger(ScriptedObject* object,
                              int selfRef,
                              const char* name)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    if (static_cast<lua_Type>(lua_getfield(L, -1, name)) != LUA_TFUNCTION)
    {
        rive_lua_pop(L, 2);
        return;
    }
    lua_pushvalue(L, -2);
    rive_lua_pcall_with_context(L, object, 1, 0);
    rive_lua_pop(L, 1);
}

bool ScriptingVM::callNumberMethod(ScriptedObject* object,
                                   int selfRef,
                                   const char* name,
                                   const float* args,
                                   size_t argCount,
                                   float* outResult)
{
    lua_State* L = m_state;
    // Stack: []
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    if (static_cast<lua_Type>(lua_getfield(L, -1, name)) != LUA_TFUNCTION)
    {
        rive_lua_pop(L, 2); // non-function field + self
        return false;
    }
    // Stack: [self, method]
    lua_pushvalue(L, -2);
    // Stack: [self, method, self]
    for (size_t i = 0; i < argCount; i++)
    {
        lua_pushnumber(L, args[i]);
    }
    if (static_cast<lua_Status>(
            rive_lua_pcall_with_context(L, object, int(argCount) + 1, 1)) !=
        LUA_OK)
    {
        // Stack: [self, errorMessage]
        rive_lua_pop(L, 2);
        return false;
    }
    // Stack: [self, result]
    *outResult = static_cast<float>(lua_tonumber(L, -1));
    rive_lua_pop(L, 2);
    return true;
}

bool ScriptingVM::callBooleanMethod(ScriptedObject* object,
                                    int selfRef,
                                    const char* name)
{
    lua_State* L = m_state;
    // Stack: []
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    lua_getfield(L, -1, name);
    // Stack: [self, field]
    lua_insert(L, -2); // Swap self and field
    // Stack: [field, self]
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 1, 1)) !=
        LUA_OK)
    {
        // Stack: [status]
        rive_lua_pop(L, 1);
        return false;
    }
    bool result = lua_isboolean(L, -1) && lua_toboolean(L, -1);
    // Stack: [result]
    rive_lua_pop(L, 1);
    return result;
}

bool ScriptingVM::callPathEffectUpdate(ScriptedObject* object,
                                       int selfRef,
                                       const RawPath& sourcePath,
                                       const ShapePaint* shapePaint,
                                       RawPath* outPath)
{
    lua_State* L = m_state;
    // Stack: []
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    lua_getfield(L, -1, "update");
    // Stack: [self, "update"]
    lua_pushvalue(L, -2);
    // Stack: [self, "update", self]
    lua_newrive<ScriptedPathData>(L, &sourcePath);
    // Stack: [self, "update", self, pathData]
    lua_newrive<ScriptedNode>(L,
                              nullptr,
                              shapePaint->parentTransformComponent());
    auto scriptedNode = lua_torive<ScriptedNode>(L, -1);
    scriptedNode->shapePaint(shapePaint);
    // Stack: [self, "update", self, pathData, node]
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 3, 1)) !=
        LUA_OK)
    {
        // Stack: [self, status]
        rive_lua_pop(L, 2);
        return false;
    }
    // Stack: [self, outputPathData]
    auto scriptedPath = (ScriptedPathData*)lua_touserdata(L, -1);
    outPath->addPath(scriptedPath->rawPath);
    rive_lua_pop(L, 2);
    return true;
}

bool ScriptingVM::callDataConvert(ScriptedObject* object,
                                  int selfRef,
                                  const char* method,
                                  DataValue* input,
                                  ScriptDataResult* outResult)
{
    lua_State* L = m_state;
    // Stack: []
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    if (static_cast<lua_Type>(lua_getfield(L, -1, method)) != LUA_TFUNCTION)
    {
        // Assumed for legacy files but not implemented; the caller passes the
        // value through unchanged (same as !dataConverts()).
        rive_lua_pop(L, 2); // non-function field + self
        return false;
    }
    // Stack: [self, field]
    lua_pushvalue(L, -2);
    // Stack: [self, field, self]
    if (input->is<DataValueNumber>())
    {
        lua_newrive<ScriptedDataValueNumber>(
            L,
            L,
            input->as<DataValueNumber>()->value());
    }
    else if (input->is<DataValueString>())
    {
        lua_newrive<ScriptedDataValueString>(
            L,
            L,
            input->as<DataValueString>()->value());
    }
    else if (input->is<DataValueBoolean>())
    {
        lua_newrive<ScriptedDataValueBoolean>(
            L,
            L,
            input->as<DataValueBoolean>()->value());
    }
    else if (input->is<DataValueColor>())
    {
        lua_newrive<ScriptedDataValueColor>(
            L,
            L,
            input->as<DataValueColor>()->value());
    }
    else
    {
        // Stack: [self, field, self]
        rive_lua_pop(L, 3);
        return true;
    }
    // Stack: [self, field, self, ScriptedData]
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 2, 1)) ==
        LUA_OK)
    {
        auto result = (ScriptedDataValue*)lua_touserdata(L, -1);
        if (result->isNumber())
        {
            outResult->kind = ScriptDataResult::Kind::number;
            outResult->number =
                result->dataValue()->as<DataValueNumber>()->value();
        }
        else if (result->isString())
        {
            outResult->kind = ScriptDataResult::Kind::string;
            outResult->string =
                result->dataValue()->as<DataValueString>()->value();
        }
        else if (result->isBoolean())
        {
            outResult->kind = ScriptDataResult::Kind::boolean;
            outResult->boolean =
                result->dataValue()->as<DataValueBoolean>()->value();
        }
        else if (result->isColor())
        {
            outResult->kind = ScriptDataResult::Kind::color;
            outResult->color =
                result->dataValue()->as<DataValueColor>()->value();
        }
    }
    // Stack: [self, status] or [self, result]
    rive_lua_pop(L, 2);
    return true;
}

void ScriptingVM::callListenerPerform(ScriptedObject* object,
                                      int selfRef,
                                      const ListenerInvocation& invocation)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    // Probe the fields directly (rather than gating on performs()/
    // performsAction(), which are assumed-present for legacy files):
    // performAction takes precedence over perform, matching prior behavior.
    if (static_cast<lua_Type>(lua_getfield(L, -1, "performAction")) ==
        LUA_TFUNCTION)
    {
        // Stack: [self, performAction]
        lua_pushvalue(L, -2);
        rive_lua_push_scripted_invocation(L, invocation);
        // Stack: [self, performAction, self, invocation]
        if (static_cast<lua_Status>(
                rive_lua_pcall_with_context(L, object, 2, 0)) != LUA_OK)
        {
            // Stack: [self, status]
            lua_pop(L, 1);
        }
        // Stack: [self]
    }
    else
    {
        lua_pop(L, 1); // non-function performAction field -> [self]
        if (static_cast<lua_Type>(lua_getfield(L, -1, "perform")) ==
            LUA_TFUNCTION)
        {
            // Stack: [self, perform]
            lua_pushvalue(L, -2);
            // Stack: [self, perform, self]
            rive_lua_push_pointer_arg_for_perform(L, invocation);
            // Stack: [self, perform, self, pointerEvent]
            if (static_cast<lua_Status>(
                    rive_lua_pcall_with_context(L, object, 2, 0)) != LUA_OK)
            {
                // Stack: [self, status]
                lua_pop(L, 1);
            }
            // Stack: [self]
        }
        else
        {
            lua_pop(L, 1); // non-function perform field -> [self]
        }
    }
    // Stack: [self]
    lua_pop(L, 1);
}

void ScriptingVM::callDraw(ScriptedObject* object,
                           int selfRef,
                           Renderer* renderer)
{
    lua_State* L = m_state;
    // Stack: []
    auto scriptedRenderer = lua_newrive<ScriptedRenderer>(L, renderer);
    // Stack: [scriptedRenderer]
    rive_lua_pushRef(L, selfRef);
    // Stack: [scriptedRenderer, self]
    if (static_cast<lua_Type>(lua_getfield(L, -1, "draw")) == LUA_TFUNCTION)
    {
        // Stack: [scriptedRenderer, self, "draw"]
        lua_pushvalue(L, -2);
        // Stack: [scriptedRenderer, self, "draw", self]
        lua_pushvalue(L, -4);
        // Stack: [scriptedRenderer, self, "draw", self, scriptedRenderer]
        if (static_cast<lua_Status>(
                rive_lua_pcall_with_context(L, object, 2, 0)) != LUA_OK)
        {
            // Stack: [scriptedRenderer, self, status]
            rive_lua_pop(L, 1);
        }
    }
    else
    {
        // draw is assumed for legacy files but not implemented; no-op.
        rive_lua_pop(L, 1); // non-function field
    }
    scriptedRenderer->end();
    // Stack: [scriptedRenderer, self]
    rive_lua_pop(L, 2);
}

bool ScriptingVM::callGamepadEvent(ScriptedObject* object,
                                   int selfRef,
                                   const char* method,
                                   const ListenerInvocation& invocation)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    lua_getfield(L, -1, method);
    if (static_cast<lua_Type>(lua_type(L, -1)) != LUA_TFUNCTION)
    {
        rive_lua_pop(L, 2);
        return false;
    }
    lua_pushvalue(L, -2);
    if (const GamepadConnectedInvocation* c = invocation.asGamepadConnected())
    {
        lua_newrive<ScriptedGamepadConnected>(L, c->snapshot);
    }
    else if (const GamepadEventInvocation* e = invocation.asGamepadEvent())
    {
        lua_newrive<ScriptedGamepadEvent>(L, *e);
    }
    else if (const GamepadDisconnectedInvocation* d =
                 invocation.asGamepadDisconnected())
    {
        lua_newrive<ScriptedGamepadDisconnected>(L, d->deviceId);
    }
    else
    {
        rive_lua_pop(L, 3);
        return false;
    }
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 2, 0)) !=
        LUA_OK)
    {
        rive_lua_pop(L, 1);
    }
    rive_lua_pop(L, 1);
    return true;
}

bool ScriptingVM::callPointerEvent(ScriptedObject* object,
                                   int selfRef,
                                   const char* method,
                                   int pointerId,
                                   Vec2D localPosition,
                                   HitResult* outResult)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    if (static_cast<lua_Type>(lua_getfield(L, -1, method)) != LUA_TFUNCTION)
    {
        // The pointer handler is assumed present for legacy files (all-bits
        // default) but isn't actually implemented: the caller reports "not
        // hit" so the state machine keeps walking other hit targets.
        rive_lua_pop(L, 2);
        return false;
    }
    lua_pushvalue(L, -2);
    auto pointerEvent =
        lua_newrive<ScriptedPointerEvent>(L, pointerId, localPosition);
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 2, 0)) !=
        LUA_OK)
    {
        fprintf(stderr, "%s failed\n", method);
        rive_lua_pop(L, 1);
    }
    *outResult = pointerEvent->m_hitResult;
    rive_lua_pop(L, 1);
    return true;
}

bool ScriptingVM::callKeyboardEvent(ScriptedObject* object,
                                    int selfRef,
                                    Key key,
                                    KeyModifiers modifiers,
                                    bool isPressed,
                                    bool isRepeat)
{
    lua_State* L = m_state;
    bool shouldStopPropagation = false;
    // Stack: []
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    if (static_cast<lua_Type>(lua_getfield(L, -1, "keyboardEvent")) !=
        LUA_TFUNCTION)
    {
        // Assumed for legacy files but not implemented; no-op.
        rive_lua_pop(L, 2); // non-function field + self
        return shouldStopPropagation;
    }
    // Stack: [self, field]
    lua_pushvalue(L, -2);
    // Stack: [self, field, self]
    lua_newrive<ScriptedKeyboardInvocation>(L,
                                            key,
                                            modifiers,
                                            isPressed,
                                            isRepeat);
    // Stack: [self, field, self, keyEvent]
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 2, 1)) !=
        LUA_OK)
    {
        fprintf(stderr, "%s failed\n", "keyboardEvent");
        // Stack: [self, status]
        rive_lua_pop(L, 1);
    }
    else
    {
        if (lua_isboolean(L, -1))
        {
            shouldStopPropagation = lua_toboolean(L, -1);
        }
        // Stack: [self, result]
        rive_lua_pop(L, 1);
    }
    // Stack: [self]
    rive_lua_pop(L, 1);
    return shouldStopPropagation;
}

bool ScriptingVM::callTextEvent(ScriptedObject* object,
                                int selfRef,
                                const std::string& text)
{
    lua_State* L = m_state;
    bool shouldStopPropagation = false;
    // Stack: []
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    if (static_cast<lua_Type>(lua_getfield(L, -1, "textEvent")) !=
        LUA_TFUNCTION)
    {
        // Assumed for legacy files but not implemented; no-op.
        rive_lua_pop(L, 2); // non-function field + self
        return shouldStopPropagation;
    }
    // Stack: [self, field]
    lua_pushvalue(L, -2);
    // Stack: [self, field, self]
    lua_newrive<ScriptedTextInputInvocation>(L, text);
    // Stack: [self, field, self, textInvocation]
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 2, 1)) !=
        LUA_OK)
    {
        fprintf(stderr, "%s failed\n", "textEvent");
        // Stack: [self, status]
        rive_lua_pop(L, 1);
    }
    else
    {
        if (lua_isboolean(L, -1))
        {
            shouldStopPropagation = lua_toboolean(L, -1);
        }
        // Stack: [self, result]
        rive_lua_pop(L, 1);
    }
    // Stack: [self]
    rive_lua_pop(L, 1);
    return shouldStopPropagation;
}

void ScriptingVM::callLayoutResize(ScriptedObject* object,
                                   int selfRef,
                                   Vec2D size)
{
    lua_State* L = m_state;
    // Stack: []
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    if (static_cast<lua_Type>(lua_getfield(L, -1, "resize")) != LUA_TFUNCTION)
    {
        // Assumed for legacy files but not implemented; no-op.
        rive_lua_pop(L, 2); // non-function field + self
        return;
    }
    // Stack: [self, function]
    lua_pushvalue(L, -2);
    // Stack: [self, function, self]
    lua_pushvec2d(L, size);
    lua_pushnumber(L, displayScale());
    // Stack: [self, function, self, size, scale]
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 3, 0)) !=
        LUA_OK)
    {
        // Stack: [self, status]
        fprintf(stderr,
                "resize failed: %s\n",
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
        rive_lua_pop(L, 1);
    }
    // Stack: [self]
    rive_lua_pop(L, 1);
}

bool ScriptingVM::callLayoutMeasure(ScriptedObject* object,
                                    int selfRef,
                                    Vec2D* outSize)
{
    lua_State* L = m_state;
    // Stack: []
    rive_lua_pushRef(L, selfRef);
    // Stack: [self]
    if (static_cast<lua_Type>(lua_getfield(L, -1, "measure")) != LUA_TFUNCTION)
    {
        // Assumed for legacy files but not implemented; report no
        // measurement (same as !measures()).
        rive_lua_pop(L, 2); // non-function field + self
        return false;
    }
    // Stack: [self, field]
    lua_pushvalue(L, -2);
    // Stack: [self, field, self]
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, object, 1, 1)) !=
        LUA_OK)
    {
        fprintf(stderr, "measure failed\n");
    }
    else if (static_cast<lua_Type>(lua_type(L, -1)) != LUA_TVECTOR)
    {
        fprintf(stderr, "expected measure to return a Vec2D\n");
    }
    else
    {
        *outSize = *lua_tovec2d(L, -1);
    }
    // Stack: [self, status] or Stack: [self, result]
    rive_lua_pop(L, 2);
    return true;
}

void ScriptingVM::setInputBoolean(int selfRef, const char* name, bool value)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    lua_pushboolean(L, value);
    lua_setfield(L, -2, name);
    rive_lua_pop(L, 1);
}

void ScriptingVM::setInputNumber(int selfRef, const char* name, float value)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    lua_pushnumber(L, value);
    lua_setfield(L, -2, name);
    rive_lua_pop(L, 1);
}

void ScriptingVM::setInputUnsigned(int selfRef,
                                   const char* name,
                                   uint32_t value)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    lua_pushunsigned(L, value);
    lua_setfield(L, -2, name);
    rive_lua_pop(L, 1);
}

void ScriptingVM::setInputString(int selfRef,
                                 const char* name,
                                 const char* value)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    lua_pushstring(L, value);
    lua_setfield(L, -2, name);
    rive_lua_pop(L, 1);
}

void ScriptingVM::setInputArtboard(int selfRef,
                                   const char* name,
                                   ScriptedObject* object,
                                   Artboard* artboard)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    auto artboardInstance = artboard->instance();
    artboardInstance->frameOrigin(false);
    lua_newrive<ScriptedArtboard>(L,
                                  L,
                                  object->scriptAsset()->file(),
                                  std::move(artboardInstance),
                                  nullptr,
                                  object->dataContext());
    lua_setfield(L, -2, name);
    rive_lua_pop(L, 1);
}

void ScriptingVM::setInputViewModel(int selfRef,
                                    const char* name,
                                    ViewModelInstanceValue* value)
{
    lua_State* L = m_state;
    rive_lua_pushRef(L, selfRef);
    switch (value->coreType())
    {
        case ViewModelInstanceViewModelBase::typeKey:
        {
            auto viewModel = value->as<ViewModelInstanceViewModel>();
            auto vmi = viewModel->referenceViewModelInstance();
            if (vmi == nullptr)
            {
                fprintf(stderr,
                        "setInputViewModel - passed in a "
                        "ViewModelInstanceViewModel with no associated "
                        "ViewModelInstance.\n");
                rive_lua_pop(L, 1);
                return;
            }

            lua_newrive<ScriptedViewModel>(L,
                                           L,
                                           ref_rcp(vmi->viewModel()),
                                           vmi);
            break;
        }
        default:
            // Nothing pushed; leave self untouched.
            rive_lua_pop(L, 1);
            return;
    }
    lua_setfield(L, -2, name);
    rive_lua_pop(L, 1);
}

#endif

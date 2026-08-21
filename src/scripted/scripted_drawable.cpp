#include "rive/animation/listener_invocation.hpp"
#include "rive/component_dirt.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_drawable.hpp"

using namespace rive;

#ifdef WITH_RIVE_SCRIPTING

void ScriptedDrawable::didHydrateScriptInputs()
{
    m_isAdvanceActive = true;
    addDirt(ComponentDirt::Paint);
}

void ScriptedDrawable::didReinit()
{
    // A paused editor never ticks scripts, so advance and update driven
    // content, like gpu canvas fills, would stay blank until play; force one
    // zero step and an update to re-record it.
    m_isAdvanceActive = true;
    m_forceAdvance = true;
    addDirt(ComponentDirt::Paint | ComponentDirt::ScriptUpdate);
}

void ScriptedDrawable::draw(Renderer* renderer)
{
    if (!draws() || m_vm == nullptr || !m_vm->valid())
    {
        return;
    }

    float opacity = renderOpacity();
    bool needsOpacitySave = (opacity != 1.0f);
    if (m_needsSaveOperation || needsOpacitySave)
    {
        renderer->save();
    }

    if (needsOpacitySave)
    {
        renderer->modulateOpacity(opacity);
    }

    renderer->transform(worldTransform());
    m_vm->callDraw(this, m_self, renderer);

    if (m_needsSaveOperation || needsOpacitySave)
    {
        renderer->restore();
    }
}

std::vector<HitComponent*> ScriptedDrawable::hitComponents(
    StateMachineInstance* sm)
{
    if (!listensToPointerEvents())
    {
        return {};
    }
    auto* hitComponent = new HitScriptedDrawable(this, sm);
    return std::vector<HitComponent*>{hitComponent};
}

bool ScriptedDrawable::gamepadDispatch(const ListenerInvocation& inv)
{
    if (m_vm == nullptr || !m_vm->valid())
    {
        return false;
    }
    const char* method = nullptr;
    switch (inv.kind())
    {
        case ListenerInvocationKind::gamepadConnected:
            method = "gamepadConnected";
            break;
        case ListenerInvocationKind::gamepadEvent:
            method = "gamepadEvent";
            break;
        case ListenerInvocationKind::gamepadDisconnected:
            method = "gamepadDisconnected";
            break;
        default:
            return false;
    }
    if (!m_vm->callGamepadEvent(this, m_self, method, inv))
    {
        return false;
    }
    wakeAdvance();
    return true;
}

HitResult HitScriptedDrawable::processEvent(Vec2D position,
                                            ListenerType hitType,
                                            bool canHit,
                                            float timeStamp,
                                            int pointerId)
{
    HitResult hitResult = HitResult::none;
    auto scriptAsset = m_drawable->scriptAsset();
    auto backend = m_drawable->backend();
    if (backend == nullptr || !backend->valid() || scriptAsset == nullptr ||
        !handlesEvent(canHit, hitType))
    {
        return HitResult::none;
    }
    Vec2D localPos;
    auto hasLocalPos = m_drawable->worldToLocal(position, &localPos);
    if (!hasLocalPos)
    {
        return hitResult;
    }
    auto mName = methodName(canHit, hitType);
    if (backend->callPointerEvent(m_drawable,
                                  m_drawable->self(),
                                  mName.c_str(),
                                  pointerId,
                                  localPos,
                                  &hitResult))
    {
        m_drawable->wakeAdvance();
    }
    return hitResult;
}

HitResult HitScriptedDrawable::processGamepadInvocation(
    const ListenerInvocation&,
    ScriptedDrawable*)
{
    // Gamepad dispatch to ScriptedDrawables runs through
    // StateMachineInstance::broadcastGamepadToScriptedDrawables's walk over
    // m_gamepadScriptedDrawables — that list includes gamepad-only scripts
    // (which never enter m_hitComponents because hitComponents() is gated on
    // listensToPointerEvents()). Returning none here avoids double-dispatch
    // for drawables that handle both pointer and gamepad.
    return HitResult::none;
}

bool ScriptedDrawable::keyInput(Key key,
                                KeyModifiers modifiers,
                                bool isPressed,
                                bool isRepeat)
{
    if (!wantsKeyboardInput())
    {
        return false;
    }
    if (m_vm == nullptr || !m_vm->valid())
    {
        return false;
    }
    bool shouldStopPropagation = m_vm->callKeyboardEvent(this,
                                                         self(),
                                                         key,
                                                         modifiers,
                                                         isPressed,
                                                         isRepeat);
    wakeAdvance();
    return shouldStopPropagation;
}

bool ScriptedDrawable::textInput(const std::string& text)
{
    if (!wantsTextInput())
    {
        return false;
    }
    if (m_vm == nullptr || !m_vm->valid())
    {
        return false;
    }
    bool shouldStopPropagation = m_vm->callTextEvent(this, self(), text);
    wakeAdvance();
    return shouldStopPropagation;
}

bool ScriptedDrawable::willDraw()
{
    return Super::willDraw() && m_vm != nullptr && draws();
}

#else
void ScriptedDrawable::draw(Renderer* renderer) {}

std::vector<HitComponent*> ScriptedDrawable::hitComponents(
    StateMachineInstance* sm)
{
    return {};
}

HitResult HitScriptedDrawable::processEvent(Vec2D position,
                                            ListenerType hitType,
                                            bool canHit,
                                            float timeStamp,
                                            int pointerId)
{
    return HitResult::none;
}

HitResult HitScriptedDrawable::processGamepadInvocation(
    const ListenerInvocation& invocation,
    ScriptedDrawable* alreadyDispatched)
{
    // TODO: implement
    return HitResult::none;
}

bool ScriptedDrawable::willDraw() { return Super::willDraw() && draws(); }

#endif

void ScriptedDrawable::update(ComponentDirt value)
{
    Super::update(value);
    if ((value & ComponentDirt::ScriptUpdate) == ComponentDirt::ScriptUpdate)
    {
        scriptUpdate();
        m_isAdvanceActive = true;
    }
}

void ScriptedDrawable::wakeAdvance()
{
    m_isAdvanceActive = true;
    addScriptedDirt(ComponentDirt::Paint);
}

Core* ScriptedDrawable::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

StatusCode ScriptedDrawable::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    artboard()->addScriptedObject(this);
    return StatusCode::Ok;
}

bool ScriptedDrawable::advanceComponent(float elapsedSeconds,
                                        AdvanceFlags flags)
{
    bool forced = m_forceAdvance;
    m_forceAdvance = false;
    if (elapsedSeconds == 0 && !forced)
    {
        return false;
    }
    if (!m_isAdvanceActive || isCollapsed())
    {
        return false;
    }
    m_isAdvanceActive = false;
    if (!enums::is_flag_set(flags, AdvanceFlags::AdvanceNested))
    {
        elapsedSeconds = 0;
    }
    auto advanced = scriptAdvance(elapsedSeconds);
    if (advanced)
    {
        m_isAdvanceActive = true;
        addScriptedDirt(ComponentDirt::Paint);
    }
    return advanced;
}

bool ScriptedDrawable::addScriptedDirt(ComponentDirt value, bool recurse)
{
    return Drawable::addDirt(value, recurse);
}

void ScriptedDrawable::addProperty(CustomProperty* prop)
{
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
        scriptInput->scriptedObject(this);
    }
    CustomPropertyContainer::addProperty(prop);
}

StatusCode ScriptedDrawable::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    return Super::import(importStack);
}

Core* ScriptedDrawable::clone() const
{
    ScriptedDrawable* twin =
        ScriptedDrawableBase::clone()->as<ScriptedDrawable>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}

void ScriptedDrawable::markNeedsUpdate()
{
    if (inUpdatePhase())
    {
        return;
    }
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

bool ScriptedDrawable::worldToLocal(Vec2D world, Vec2D* local)
{
    Mat2D toMountedArtboard;
    if (!worldTransform().invert(&toMountedArtboard))
    {
        return false;
    }

    *local = toMountedArtboard * world;

    return true;
}

bool HitScriptedDrawable::handlesEvent(bool canHit, ListenerType hitEvent)
{
    if (canHit)
    {
        switch (hitEvent)
        {
            case ListenerType::down:
                return m_drawable->wantsPointerDown();
            case ListenerType::up:
                return m_drawable->wantsPointerUp();
            case ListenerType::dragStart:
                return false;
            case ListenerType::dragEnd:
                return false;
            default:
                return m_drawable->wantsPointerMove();
        }
    }
    return m_drawable->wantsPointerExit();
}

std::string HitScriptedDrawable::methodName(bool canHit, ListenerType hitEvent)
{
    if (canHit)
    {
        switch (hitEvent)
        {
            case ListenerType::down:
                return "pointerDown";
            case ListenerType::up:
                return "pointerUp";
            case ListenerType::dragStart:
            case ListenerType::dragEnd:
                return "";
            default:
                return "pointerMove";
        }
    }
    return "pointerExit";
}
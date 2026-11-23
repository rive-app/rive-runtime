#include "rive/animation/listener_invocation.hpp"
#include "rive/artboard.hpp"
#include "rive/component_dirt.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/scripted/scripted_layout.hpp"

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
  if (needsSaveOperation() || needsOpacitySave)
  {
    renderer->save();
  }

  if (needsOpacitySave)
  {
    renderer->modulateOpacity(opacity);
  }

  renderer->transform(worldTransform());
  m_vm->callDraw(this, m_self, renderer);

  if (needsSaveOperation() || needsOpacitySave)
  {
    renderer->restore();
  }
}

// Checked on every hit test rather than when the hit component is made,
// because a script with inputs isn't instantiated until data is bound, which
// higher level runtimes do after creating the state machine.
bool ScriptedDrawable::hasPointerHandler()
{
  // A script that isn't running can't take the event.
  if (m_vm == nullptr || !m_vm->valid() || m_self == 0)
  {
    return false;
  }
  return wantsAnyPointer();
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

static uint8_t buttonBit(PointerButton button)
{
  return static_cast<uint8_t>(1 << static_cast<int>(button));
}

HitResult HitScriptedDrawable::processEvent(
    Vec2D position,
    ListenerType hitType,
    bool canHit,
    float timeStamp,
    int pointerId,
    PointerButton button)
{
  HitResult hitResult = HitResult::none;
  auto drawable = m_component->as<ScriptedDrawable>();
  auto scriptAsset = drawable->scriptAsset();
  auto backend = drawable->backend();
  if (backend == nullptr || !backend->valid() || scriptAsset == nullptr)
  {
    return HitResult::none;
  }
  // Drag passes reach no handler, so they don't put a pointer inside.
  if (canHit &&
      (hitType == ListenerType::dragStart || hitType == ListenerType::dragEnd))
  {
    return HitResult::none;
  }
  // A claim here stops the host's later hit components, so a hosted script
  // hears only where its artboard shows, a press that started there (claimed
  // or not), and one exit.
  bool leaving = false;
  auto artboard = drawable->artboard();
  if (artboard != nullptr && artboard->host() != nullptr)
  {
    auto isPointer = [pointerId](const HeldPointer& held) {
      return held.id == pointerId;
    };
    // Kept only while a button is pressed or the pointer is inside.
    HeldPointer* held = m_pointers.findIf(isPointer);
    const bool pressed = held != nullptr && held->buttons != 0;
    // A hover exit is a host shape covering the pointer, not a lift.
    const bool ended = hitType == ListenerType::exit &&
                       !m_stateMachineInstance->inHoverExit();
    if (ended && held != nullptr)
    {
      held->buttons = 0;
    }
    else if (hitType == ListenerType::up && held != nullptr)
    {
      held->buttons &= static_cast<uint8_t>(~buttonBit(button));
    }
    // An ended pointer the script held reaches pointerExit, once.
    if (ended || (!pressed && !showsAt(position)))
    {
      if (!pressed && (held == nullptr || !held->inside))
      {
        return HitResult::none;
      }
      leaving = true;
      canHit = false;
    }
    if (canHit && hitType != ListenerType::exit)
    {
      const uint8_t bit = hitType == ListenerType::down ? buttonBit(button) : 0;
      if (held == nullptr)
      {
        m_pointers.push_back({pointerId, bit, true});
      }
      else
      {
        held->buttons |= bit;
        held->inside = true;
      }
    }
    else if (held != nullptr)
    {
      held->inside = false;
      if (held->buttons == 0)
      {
        m_pointers.eraseIf(isPointer);
      }
    }
  }
  if (!handlesEvent(canHit, hitType))
  {
    return HitResult::none;
  }
  Vec2D localPos;
  auto hasLocalPos = drawable->worldToLocal(position, &localPos);
  if (!hasLocalPos)
  {
    return hitResult;
  }
  auto mName = methodName(canHit, hitType);
  if (backend->callPointerEvent(
          drawable,
          drawable->self(),
          mName.c_str(),
          pointerId,
          localPos,
          canHit ? hitType : ListenerType::exit,
          timeStamp,
          &hitResult))
  {
    drawable->wakeAdvance();
  }
  return leaving ? HitResult::none : hitResult;
}

HitResult HitScriptedDrawable::processScroll(
    Vec2D position,
    const ScrollEvent& event,
    float timeStamp,
    int pointerId)
{
  HitResult hitResult = HitResult::none;
  auto drawable = m_component->as<ScriptedDrawable>();
  auto backend = drawable->backend();
  Mat2D toLocal;
  if (backend == nullptr || !backend->valid() ||
      drawable->scriptAsset() == nullptr || drawable->isCollapsed() ||
      !drawable->worldTransform().invert(&toLocal))
  {
    drawable->scrollLatched(false);
    return hitResult;
  }
  Vec2D localPos = toLocal * position;
  ScrollEvent local = event;
  local.delta = toLocal * (position + event.delta) - localPos;
  if (backend->callScrollEvent(
          drawable,
          drawable->self(),
          pointerId,
          localPos,
          local,
          timeStamp,
          &hitResult))
  {
    drawable->wakeAdvance();
  }
  // A claim latches the gesture, which then stays until it ends; a wheel
  // detent never latches.
  bool latchable = event.phase == ScrollPhase::begin ||
                   event.phase == ScrollPhase::momentum ||
                   (event.phase == ScrollPhase::update && event.precise);
  drawable->scrollLatched(
      latchable && (hitResult != HitResult::none || drawable->scrollLatched()));
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

bool ScriptedDrawable::keyInput(
    Key key,
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
  bool shouldStopPropagation = m_vm->callKeyboardEvent(
      this,
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

bool ScriptedDrawable::hasPointerHandler() { return false; }

std::vector<HitComponent*> ScriptedDrawable::hitComponents(
    StateMachineInstance* sm)
{
  return {};
}

HitResult HitScriptedDrawable::processEvent(
    Vec2D position,
    ListenerType hitType,
    bool canHit,
    float timeStamp,
    int pointerId,
    PointerButton button)
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

HitResult HitScriptedDrawable::processScroll(
    Vec2D position,
    const ScrollEvent& event,
    float timeStamp,
    int pointerId)
{
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

bool ScriptedDrawable::advanceComponent(
    float elapsedSeconds,
    AdvanceFlags flags)
{
  bool forced = m_forceAdvance;
  m_forceAdvance = false;
  if (elapsedSeconds == 0 && !forced)
  {
    return false;
  }
  // A gesture that reports no end of its own releases the latch by going
  // quiet, as a ScrollConstraint's does.
  if (m_scrollLatched &&
      enums::is_flag_set(flags, AdvanceFlags::AdvanceNested) &&
      enums::is_flag_set(flags, AdvanceFlags::NewFrame))
  {
    m_scrollIdleSeconds += elapsedSeconds;
    m_scrollLatched = m_scrollIdleSeconds < scrollIdleSeconds;
  }
  if (!m_isAdvanceActive || isCollapsed())
  {
    return m_scrollLatched;
  }
  m_isAdvanceActive = false;
  if (!enums::is_flag_set(flags, AdvanceFlags::AdvanceNested))
  {
    elapsedSeconds = 0;
  }
  bool redraw = m_redrawAfterAdvance;
  m_redrawAfterAdvance = false;
  auto advanced = scriptAdvance(elapsedSeconds);
  if (advanced)
  {
    m_isAdvanceActive = true;
  }
  if (advanced || redraw)
  {
    addScriptedDirt(ComponentDirt::Paint);
  }
  return advanced || redraw || m_scrollLatched;
}

AdvancingComponent::QuietState ScriptedDrawable::quietState()
{
  return m_isAdvanceActive || m_forceAdvance || m_redrawAfterAdvance ||
                 m_scrollLatched
             ? QuietState::busy
             : QuietState::quiet;
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
  ScriptedDrawable* twin = ScriptedDrawableBase::clone()
                               ->as<ScriptedDrawable>();
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

bool HitScriptedDrawable::hitTestBounded(Vec2D position) const
{
  if (!m_component->as<ScriptedDrawable>()->hasPointerHandler())
  {
    return false;
  }
  return withinBounds(position);
}

// Box test shared by pointer hits and scroll.
bool HitScriptedDrawable::withinBounds(Vec2D position) const
{
  auto drawable = m_component->as<ScriptedDrawable>();
  if (!drawable->is<ScriptedLayout>())
  {
    // A scripted node has no box of its own; everywhere, like hitTest.
    return true;
  }
  Vec2D size;
  if (!drawable->as<ScriptedLayout>()->layoutSize(size))
  {
    return false;
  }
  Vec2D local;
  if (!drawable->worldToLocal(position, &local))
  {
    return false;
  }
  return local.x >= 0.0f && local.y >= 0.0f && local.x <= size.x &&
         local.y <= size.y;
}

bool HitScriptedDrawable::hasScrollTarget(Vec2D position)
{
  auto drawable = m_component->as<ScriptedDrawable>();
  if (!drawable->wantsPointerScroll() || drawable->isCollapsed())
  {
    return false;
  }
  if (drawable->is<ScriptedLayout>())
  {
    return withinBounds(position);
  }
  // A node has no box, so its artboard bounds it; hosts would otherwise
  // lose every wheel event to it.
  return inArtboard(position);
}

bool HitScriptedDrawable::inArtboard(Vec2D position) const
{
  auto artboard = m_component->artboard();
  return artboard != nullptr && artboard->bounds().contains(position);
}

// Also inside the clips a host draws the artboard in, as for its shapes.
bool HitScriptedDrawable::showsAt(Vec2D position) const
{
  return inArtboard(position) &&
         m_component->artboard()->hitTestPoint(position, true, false);
}

bool HitScriptedDrawable::scrollGestureActive()
{
  // Like a collapsed ScrollConstraint, a collapsed script ends its gesture.
  auto drawable = m_component->as<ScriptedDrawable>();
  if (drawable->isCollapsed())
  {
    drawable->scrollLatched(false);
  }
  return drawable->scrollLatched();
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
  auto drawable = m_component->as<ScriptedDrawable>();
  if (canHit)
  {
    switch (hitEvent)
    {
      case ListenerType::down:
        return drawable->wantsPointerDown();
      case ListenerType::up:
        return drawable->wantsPointerUp();
      case ListenerType::dragStart:
        return false;
      case ListenerType::dragEnd:
        return false;
      default:
        return drawable->wantsPointerMove();
    }
  }
  return drawable->wantsPointerExit();
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
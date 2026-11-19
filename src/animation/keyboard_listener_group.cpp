#include "rive/animation/keyboard_listener_group.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/listener_types/listener_input_type_keyboard.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/text/text_input.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/focus_data.hpp"

using namespace rive;

namespace
{
// Runs the listener's actions and reports whether one of them claimed the
// event: a scripted action's bare hit() on its key or text event, the same
// opaque hit that stops a pointer event. hit(true) only marks it seen.
bool performAndClaim(
    const StateMachineListener* listener,
    StateMachineInstance* stateMachineInstance,
    const ListenerInvocation& invocation)
{
  listener->performChanges(stateMachineInstance, invocation);
  return invocation.hitResult() == HitResult::hitOpaque;
}
} // namespace

KeyboardListenerGroup::KeyboardListenerGroup(
    FocusData* focusData,
    const StateMachineListener* listener,
    StateMachineInstance* stateMachineInstance)
    : m_focusData(focusData)
    , m_listener(listener)
    , m_stateMachineInstance(stateMachineInstance)
    ,
    // Read once: the editor can change the phase while this group lives, and
    // the destructor has to unregister from the lists it registered with.
    m_isCapture(listener != nullptr && listener->isCapture())
{
  // Register ourselves as a listener on the FocusData
  if (m_listener)
  {
    if (m_listener->hasListener(ListenerType::keyboard))
    {
      if (m_isCapture)
      {
        m_focusData->addCaptureKeyboardListener(this);
      }
      else
      {
        m_focusData->addKeyboardListener(this);
      }
    }
    if (m_listener->hasListener(ListenerType::textInput))
    {
      if (m_isCapture)
      {
        m_focusData->addCaptureTextInputListener(this);
      }
      else
      {
        m_focusData->addTextInputListener(this);
      }
    }
  }
  else
  {
    if (m_focusData && m_focusData->parent())
    {
      auto parent = m_focusData->parent();
      if (parent->is<ScriptedDrawable>())
      {
        auto scriptedObject = ScriptedObject::from(parent);
        if (scriptedObject->wantsKeyboardInput())
        {

          m_focusData->addKeyboardListener(this);
        }
        if (scriptedObject->wantsTextInput())
        {

          m_focusData->addTextInputListener(this);
        }
      }
    }
  }
}

KeyboardListenerGroup::~KeyboardListenerGroup()
{
  if (m_listener)
  {

    if (m_listener->hasListener(ListenerType::keyboard))
    {
      if (m_isCapture)
      {
        m_focusData->removeCaptureKeyboardListener(this);
      }
      else
      {
        m_focusData->removeKeyboardListener(this);
      }
    }
    if (m_listener->hasListener(ListenerType::textInput))
    {
      if (m_isCapture)
      {
        m_focusData->removeCaptureTextInputListener(this);
      }
      else
      {
        m_focusData->removeTextInputListener(this);
      }
    }
  }
  else
  {
    if (m_focusData && m_focusData->parent())
    {
      auto parent = m_focusData->parent();
      if (parent->is<ScriptedDrawable>())
      {
        auto scriptedObject = ScriptedObject::from(parent);
        if (scriptedObject->wantsKeyboardInput())
        {

          m_focusData->removeKeyboardListener(this);
        }
        if (scriptedObject->wantsTextInput())
        {

          m_focusData->removeTextInputListener(this);
        }
      }
    }
  }
}

bool KeyboardListenerGroup::keyInput(
    Key key,
    KeyModifiers modifiers,
    bool isPressed,
    bool isRepeat)
{
  // A TextInput's own editing runs from FocusData::keyInput, once per key,
  // ahead of the listeners on it. Each listener here only runs its own
  // actions.
#ifdef WITH_RIVE_SCRIPTING
  if (m_focusData && m_focusData->parent() && !listener())
  {
    auto parent = m_focusData->parent();
    if (parent->is<ScriptedDrawable>())
    {
      auto scriptedObject = ScriptedObject::from(parent);
      if (scriptedObject->wantsKeyboardInput())
      {

        return parent->as<ScriptedDrawable>()
            ->keyInput(key, modifiers, isPressed, isRepeat);
      }
    }
  }
#endif
  if (listener())
  {
    if (!ListenerInputTypeKeyboard::keyboardListenerConstraintsMet(
            listener(),
            key,
            modifiers,
            isPressed,
            isRepeat))
    {
      return false;
    }
    if (performAndClaim(
            listener(),
            m_stateMachineInstance,
            ListenerInvocation::keyboard(key, modifiers, isPressed, isRepeat)))
    {
      return true;
    }
    // On a TextInput, a listener that takes every key only watches them
    // go by, unless its action hit() one. Claiming would swallow what the
    // input itself leaves alone -- Tab, Escape, a single line's Enter --
    // which has to keep bubbling to the traversal around it. Whether the
    // input used the key is its own answer, which FocusData::keyInput
    // already reports.
    if (m_focusData && m_focusData->parent() &&
        m_focusData->parent()->is<TextInput>() &&
        !ListenerInputTypeKeyboard::keyboardListenerOwnsKey(
            listener(),
            key,
            modifiers))
    {
      return false;
    }
    // The listener asked for this exact key and its changes have run, so
    // the keystroke is spent: FocusManager::keyInput stops bubbling to
    // ancestors, and the host learns the scene consumed it rather than
    // also firing whatever the key means to the application around it.
    // Peers on the same FocusData still see it -- FocusData::keyInput
    // offers the key to all of them and only reports the claim upward.
    return true;
  }
  return false;
}

bool KeyboardListenerGroup::captureKeyInput(
    Key key,
    KeyModifiers modifiers,
    bool isPressed,
    bool isRepeat)
{
  if (!m_isCapture)
  {
    return false;
  }
  bool hit = false;
  if (ListenerInputTypeKeyboard::keyboardListenerConstraintsMet(
          listener(),
          key,
          modifiers,
          isPressed,
          isRepeat))
  {
    hit = performAndClaim(
        listener(),
        m_stateMachineInstance,
        ListenerInvocation::keyboard(key, modifiers, isPressed, isRepeat));
  }
  // A key named exactly is claimed in every phase, including the ones the
  // listener doesn't act on: an "up" listener that let the press through
  // would hand the target half a keystroke. A listener that takes any key
  // only observes, or it would swallow every key under it -- unless its
  // action hit() this one, which claims it (FocusManager then keeps the
  // rest of that keystroke away from the target too).
  return hit || ListenerInputTypeKeyboard::keyboardListenerOwnsKey(
                    listener(),
                    key,
                    modifiers);
}

bool KeyboardListenerGroup::captureTextInput(const std::string& text)
{
  if (!m_isCapture || !listener())
  {
    return false;
  }
  // Text has nothing to filter on, so a capture listener always runs and
  // only observes, unless its action hit() the text: then nothing deeper,
  // the TextInput included, ever sees it.
  return performAndClaim(
      listener(),
      m_stateMachineInstance,
      ListenerInvocation::textInput(text));
}

bool KeyboardListenerGroup::textInput(const std::string& text)
{
  // A TextInput's own insert runs from FocusData::textInput, once, ahead of
  // the listeners on it. Each listener here only runs its own actions.
#ifdef WITH_RIVE_SCRIPTING
  if (m_focusData && m_focusData->parent() && !listener())
  {
    auto parent = m_focusData->parent();
    if (parent->is<ScriptedDrawable>())
    {
      auto scriptedObject = ScriptedObject::from(parent);
      if (scriptedObject->wantsTextInput())
      {
        return parent->as<ScriptedDrawable>()->textInput(text);
      }
    }
  }
#endif
  if (listener())
  {
    return performAndClaim(
        listener(),
        m_stateMachineInstance,
        ListenerInvocation::textInput(text));
  }
  return false;
}
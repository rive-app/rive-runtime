#ifndef _RIVE_SCRIPT_BACKEND_HPP_
#define _RIVE_SCRIPT_BACKEND_HPP_

#include <stdint.h>

#include "rive/hit_result.hpp"
#include "rive/input/focusable.hpp"
#include "rive/math/vec2d.hpp"

#include <string>
#include <unordered_set>

namespace rive
{
class Artboard;
class DataValue;
class ListenerInvocation;
class RawPath;
class Renderer;
class ScriptedContext;
class ScriptedObject;
class ShapePaint;
class ViewModelInstanceValue;

/// The narrow waist between scripted core objects and a script execution
/// backend. Script values are held as opaque int refs minted and released by
/// the backend; ScriptedObject and friends never touch the engine directly.
/// Implementations: ScriptingVM (Luau) and, per the WAMR swap plan,
/// WasmScriptingVM.
class ScriptBackend
{
public:
    virtual ~ScriptBackend();

    /// ScriptedObject holds a non-owning ScriptBackend*. The backend tracks
    /// which objects point at it so teardown can null those pointers before
    /// cascading destruction reaches them.
    void registerScriptedObject(ScriptedObject* object);
    void unregisterScriptedObject(ScriptedObject* object);

    virtual bool valid() const = 0;
    virtual void releaseRef(int ref) = 0;

    /// The editor stores generator refs per context; backends without that
    /// remap return the ref unchanged.
    virtual int resolveGeneratorRef(int ref) const { return ref; }

    /// Runs the script's generator: creates the object's Context, calls
    /// generator(context), refs the returned table. Returns the self ref or 0
    /// on failure; outContextRef/outContextPtr carry the created Context.
    virtual int instantiate(int generatorRef,
                            ScriptedObject* object,
                            int* outContextRef,
                            ScriptedContext** outContextPtr) = 0;

    enum class InitResult : uint8_t
    {
        /// init is optional and absent; the object counts as initialized.
        notImplemented,
        succeeded,
        /// init errored or returned false / requested missing data; the
        /// caller tears the instance down.
        failed,
    };
    virtual InitResult callUserInit(ScriptedObject* object,
                                    int selfRef,
                                    int contextRef) = 0;

    /// self:advance(elapsedSeconds) -> bool; false when not implemented or on
    /// error.
    virtual bool callAdvance(ScriptedObject* object,
                             int selfRef,
                             float elapsedSeconds) = 0;
    virtual void callUpdate(ScriptedObject* object, int selfRef) = 0;
    /// self[name](self), for triggers.
    virtual void callTrigger(ScriptedObject* object,
                             int selfRef,
                             const char* name) = 0;
    /// self[name](self, args...) -> number. Returns false when the method is
    /// not implemented or errors, leaving outResult untouched so the caller's
    /// fallback stands.
    virtual bool callNumberMethod(ScriptedObject* object,
                                  int selfRef,
                                  const char* name,
                                  const float* args,
                                  size_t argCount,
                                  float* outResult) = 0;
    /// self[name](self) -> bool; false unless the call succeeds and returns
    /// an actual boolean true.
    virtual bool callBooleanMethod(ScriptedObject* object,
                                   int selfRef,
                                   const char* name) = 0;
    /// Path effect protocol: self.update(self, pathData(source),
    /// node(shapePaint)) -> pathData appended into outPath. False on error.
    virtual bool callPathEffectUpdate(ScriptedObject* object,
                                      int selfRef,
                                      const RawPath& sourcePath,
                                      const ShapePaint* shapePaint,
                                      RawPath* outPath) = 0;

    /// A script data value copied out of the engine before its holder can be
    /// collected; kind none when the call produced nothing usable.
    struct ScriptDataResult
    {
        enum class Kind : uint8_t
        {
            none,
            number,
            string,
            boolean,
            color,
        };
        Kind kind = Kind::none;
        float number = 0.0f;
        std::string string;
        bool boolean = false;
        int color = 0;
    };
    /// Data converter protocol: self[method](self, dataValue(input)).
    /// Returns false when the method is not implemented, meaning the input
    /// passes through unchanged.
    virtual bool callDataConvert(ScriptedObject* object,
                                 int selfRef,
                                 const char* method,
                                 DataValue* input,
                                 ScriptDataResult* outResult) = 0;

    /// Listener protocol: self.performAction(self, invocation) when defined,
    /// else legacy self.perform(self, pointerEvent).
    virtual void callListenerPerform(ScriptedObject* object,
                                     int selfRef,
                                     const ListenerInvocation& invocation) = 0;

    /// Drawable protocol: self.draw(self, renderer(renderer)); the renderer
    /// wrapper is scoped to this call. The caller owns save/transform/restore.
    virtual void callDraw(ScriptedObject* object,
                          int selfRef,
                          Renderer* renderer) = 0;
    /// Drawable protocol: gamepad connected/event/disconnected dispatch by
    /// invocation kind; false when the method isn't implemented or the kind
    /// carries no gamepad payload.
    virtual bool callGamepadEvent(ScriptedObject* object,
                                  int selfRef,
                                  const char* method,
                                  const ListenerInvocation& invocation) = 0;
    /// Drawable protocol: self[method](self, pointerEvent(pointerId, local)).
    /// False when not implemented; outResult carries the event's hit result.
    virtual bool callPointerEvent(ScriptedObject* object,
                                  int selfRef,
                                  const char* method,
                                  int pointerId,
                                  Vec2D localPosition,
                                  HitResult* outResult) = 0;
    /// Drawable protocol: self.keyboardEvent(self, invocation) -> stop
    /// propagation.
    virtual bool callKeyboardEvent(ScriptedObject* object,
                                   int selfRef,
                                   Key key,
                                   KeyModifiers modifiers,
                                   bool isPressed,
                                   bool isRepeat) = 0;
    /// Drawable protocol: self.textEvent(self, invocation) -> stop
    /// propagation.
    virtual bool callTextEvent(ScriptedObject* object,
                               int selfRef,
                               const std::string& text) = 0;

    /// Layout protocol: self.resize(self, size).
    virtual void callLayoutResize(ScriptedObject* object,
                                  int selfRef,
                                  Vec2D size) = 0;
    /// Layout protocol: self.measure(self) -> vector. False only when the
    /// method is not implemented; outSize is written only when the call
    /// returns an actual vector.
    virtual bool callLayoutMeasure(ScriptedObject* object,
                                   int selfRef,
                                   Vec2D* outSize) = 0;

    virtual void setInputBoolean(int selfRef, const char* name, bool value) = 0;
    virtual void setInputNumber(int selfRef, const char* name, float value) = 0;
    virtual void setInputUnsigned(int selfRef,
                                  const char* name,
                                  uint32_t value) = 0;
    virtual void setInputString(int selfRef,
                                const char* name,
                                const char* value) = 0;
    virtual void setInputArtboard(int selfRef,
                                  const char* name,
                                  ScriptedObject* object,
                                  Artboard* artboard) = 0;
    virtual void setInputViewModel(int selfRef,
                                   const char* name,
                                   ViewModelInstanceValue* value) = 0;

protected:
    /// Nulls every registered object's backend pointer, ahead of engine
    /// teardown cascades that would otherwise reach a freed backend.
    void detachScriptedObjects();

private:
    std::unordered_set<ScriptedObject*> m_scriptedObjects;
};

} // namespace rive

#endif

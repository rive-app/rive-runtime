#ifndef _RIVE_CORE_HPP_
#define _RIVE_CORE_HPP_

#include "rive/rive_types.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/status_code.hpp"
#include "rive/core/editor_hooks.hpp"

#ifdef WITH_RIVE_EDITOR
#include "rive/core/fractional_index.hpp"
#include <cstdio>
#include <string>
#endif

#ifdef DEBUG
#define DEBUG_PRINT(msg) fprintf(stderr, msg "\n");
#else
#define DEBUG_PRINT(msg)
#endif

namespace rive
{
class CoreContext;
class DataBind;
class ImportStack;
#ifdef WITH_RIVE_EDITOR
class ObjectArena;
#endif
class Core
{
public:
    Core() = default;

    // m_firstObserver is intrusive — observers cache `this` as their target
    // and chain through `m_nextObserver` rooted at this Core. Cloning a Core
    // must NOT inherit the original's observer list head: the new instance
    // has no observers of its own, and propagating the head pointer would
    // cause notifyPropertyChanged to dirty binds that point at the original,
    // and ~Core() on the clone to wrongly detach them.
    // m_arena is not inherited either: membership is established by
    // ObjectArena::add, so a Core built by copy or move has not been added
    // anywhere and would otherwise resolve handles against an arena it is
    // not in.
    // Nothing needs it to survive a copy — `createInstance` / `clone()`
    // build their result through the generated `copy()`, never the copy
    // constructor, and `copy()` is also where m_hasValidated propagates
    // (RIVE_EDITOR_COPY_VALIDATED).
#ifdef WITH_RIVE_EDITOR
    Core(const Core& other) :
        m_firstObserver(nullptr),
        m_arena(nullptr),
        m_hasValidated(other.m_hasValidated)
    {}
    Core& operator=(const Core& other)
    {
        // Observer list intentionally not copied — see the runtime
        // operator= below. m_arena stays the destination's: the slot
        // registration names this address, not the source's.
        m_hasValidated = other.m_hasValidated;
        return *this;
    }
    Core(Core&& other) noexcept :
        m_firstObserver(nullptr),
        m_arena(nullptr),
        m_hasValidated(other.m_hasValidated)
    {}
    Core& operator=(Core&& other) noexcept
    {
        m_hasValidated = other.m_hasValidated;
        return *this;
    }
#else
    Core(const Core&) : m_firstObserver(nullptr) {}
    Core& operator=(const Core&)
    {
        // Intentional no-op for the observer list: our existing observers
        // continue to point at us; theirs continue to point at them. Any
        // other state that's safe to copy lives in derived classes.
        return *this;
    }
    // Move ops follow the same rule: the observer chain is bound to the
    // original Core's address, so moving cannot transfer it. Default-init
    // the new instance's head; the moved-from instance keeps its observers
    // (and the responsibility to detach them at destruction).
    Core(Core&&) noexcept : m_firstObserver(nullptr) {}
    Core& operator=(Core&&) noexcept { return *this; }
#endif

    const uint32_t emptyId = -1;
    static const int invalidPropertyKey = 0;
    virtual ~Core();
    virtual uint16_t coreType() const = 0;
    virtual bool isTypeOf(uint16_t typeKey) const = 0;
    virtual bool deserialize(uint16_t propertyKey, BinaryReader& reader) = 0;

    template <typename T> inline bool is() const
    {
        return isTypeOf(T::typeKey);
    }
    template <typename T> inline T* as()
    {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    /// Make a shallow copy of the object.
    virtual Core* clone() const { return nullptr; }

    template <typename T> inline const T* as() const
    {
        assert(is<T>());
        return static_cast<const T*>(this);
    }

    /// Called to validate the object can be used at runtime.
    virtual bool validate(CoreContext* context) { return true; }

    /// Called when the object is first added to the context, other objects
    /// may not have resolved their dependencies yet. This is an opportunity
    /// to look up objects referenced by id, but not assume that they in
    /// turn have resolved their references yet. Called during
    /// load/instance.
    virtual StatusCode onAddedDirty(CoreContext* context)
    {
        return StatusCode::Ok;
    }

    /// Called when all the objects in the context have had onAddedDirty
    /// called. This is an opportunity to reference things referenced by
    /// dependencies. (A path should be able to find a Shape somewhere in
    /// its hierarchy, which may be multiple levels up).
    virtual StatusCode onAddedClean(CoreContext* context)
    {
        return StatusCode::Ok;
    }

    virtual StatusCode import(ImportStack& importStack)
    {
        return StatusCode::Ok;
    }

    // Push-notification hook for target→source data binds. Property setters
    // generated in *_base.hpp call this with the affected property key after a
    // value changes. Default impl walks the intrusive observer list and
    // dirties any DataBind that subscribed for `propertyKey` on this Core.
    //
    // Non-virtual: every Core inherits the same dispatch. The hot path
    // (no observers) is a single null-pointer test.
    void notifyPropertyChanged(uint16_t propertyKey);

    // Intrusive subscriber list management. Used by DataBind to subscribe to
    // target value changes without paying a per-Core map allocation. Each
    // Core holds the list head; each subscribing DataBind holds m_nextObserver
    // for the same (target, key) chain. Observer order is not guaranteed.
    void addPropertyObserver(DataBind* observer);
    void removePropertyObserver(DataBind* observer);

private:
    DataBind* m_firstObserver = nullptr;
#ifdef WITH_RIVE_EDITOR
public:
    /// Called by every generated setter just before mutating the backing
    /// field. The runtime build never compiles this — the generator
    /// emits the call site under the same WITH_RIVE_EDITOR guard.
    ///
    /// `oldValue` / `newValue` are pointers to the property's storage in
    /// its native field type (float, uint32_t, std::string, etc.).
    ///
    /// Routing: the default implementation forwards to a static callback
    /// that editor_native installs at startup via
    /// installPropertyChangingCallback. This means every generated
    /// setter on every runtime type automatically participates in the
    /// editor's ChangeTracker + undo/redo + ObjectCache refresh — no
    /// proprietary override required on per-type Cores.
    ///
    /// A proprietary Core may still override this directly to special-
    /// case its own mutations before/instead of the callback.
    virtual void onPropertyChanging(uint16_t propertyKey,
                                    const void* oldValue,
                                    const void* newValue)
    {
        if (s_onPropertyChanging != nullptr)
        {
            s_onPropertyChanging(this, propertyKey, oldValue, newValue);
        }
    }

    /// Global callback installed by editor_native at process startup.
    /// Takes (core, propertyKey, oldValue, newValue). Null when no
    /// editor is attached — then the virtual dispatch above is a no-op
    /// and the runtime pays zero cost per setter call.
    using OnPropertyChangingCallback = void (*)(Core*,
                                                uint16_t,
                                                const void*,
                                                const void*);

    static void installPropertyChangingCallback(
        OnPropertyChangingCallback callback)
    {
        s_onPropertyChanging = callback;
    }

    /// Walk this object's stored, non-default properties and fire the
    /// onPropertyChanging hook (or its typed variants) once per
    /// property with old == new == current. Used by the dispatcher's
    /// delete path to snapshot the object's state into the active
    /// JournalBatch before the object is removed — undo of the delete
    /// recreates with defaults, then replays these entries to restore
    /// the snapshot.
    ///
    /// Mirrors packages/core/lib/core.dart's `changeNonDefault()` which
    /// the Dart editor's removeObject calls before issuing its removeKey
    /// entry. The generator emits overrides per *Base class, one-line-
    /// per-stored-property; every override chains up to its `Super::`
    /// (and top-level types chain to `Core::`) so this body bottoms
    /// out the recursion as a safe no-op for any property keys that
    /// don't belong to a registered *Base.
    virtual void captureStateForJournal() {}

    /// Typed variant for String-valued properties.
    ///
    /// Motivation: the generic void-pointer hook can't safely read bytes
    /// out of `std::string*` vs `Span<const uint8_t>*` given only a
    /// field-type id — they share CoreStringType::id == CoreBytesType::id
    /// (the file-format ToC packs field ids in 2 bits, and both have
    /// the same "varuint length + bytes" skip pattern, so the shared
    /// id is correct). Instead, the generator emits a typed hook call
    /// for String-typed setters: the std::string reference is captured
    /// by value at the call site, no void-cast indirection needed.
    ///
    /// Bytes (encoded properties) don't flow through this path — they
    /// go through the hand-written decodeXxx() pure virtuals.
    virtual void onStringChanging(uint16_t propertyKey,
                                  const std::string& oldValue,
                                  const std::string& newValue)
    {
        if (s_onStringChanging != nullptr)
        {
            s_onStringChanging(this, propertyKey, oldValue, newValue);
        }
    }

    using OnStringChangingCallback = void (*)(Core*,
                                              uint16_t,
                                              const std::string&,
                                              const std::string&);

    static void installStringChangingCallback(OnStringChangingCallback callback)
    {
        s_onStringChanging = callback;
    }

    /// Typed variant for FractionalIndex-valued properties (currently
    /// `Component.childOrder`). The generic void-pointer hook can't
    /// encode a two-field struct safely — the (numerator, denominator)
    /// pair has no single representative pointer to carry across the
    /// void*. The typed hook fires with both old and new struct values
    /// captured by value; editor_native's encoder writes the wire bytes
    /// (two varuints) on each side.
    virtual void onFractionalIndexChanging(uint16_t propertyKey,
                                           FractionalIndex oldValue,
                                           FractionalIndex newValue)
    {
        if (s_onFractionalIndexChanging != nullptr)
        {
            s_onFractionalIndexChanging(this, propertyKey, oldValue, newValue);
        }
    }

    using OnFractionalIndexChangingCallback = void (*)(Core*,
                                                       uint16_t,
                                                       FractionalIndex,
                                                       FractionalIndex);

    static void installFractionalIndexChangingCallback(
        OnFractionalIndexChangingCallback callback)
    {
        s_onFractionalIndexChanging = callback;
    }

    /// Animation-context flag — editor_native's render loop flips
    /// this around `advanceAndApply` so the
    /// `handlePropertyChanging` hook (core_hook.cpp) can tell an
    /// animation-driven write apart from a user / coop / journal
    /// edit. The animation branch in the hook records a `(CoopId,
    /// propertyKey)` touch for the next frame's
    /// `revertTouchedClonesToMain` pass and skips the journal /
    /// coop / cross-clone propagation work — playback writes stay
    /// scoped to the playing clone.
    ///
    /// No setter branch in the generated code consults this any
    /// more (the override mechanism it used to gate was removed —
    /// see [[clone-only-playback-state]]). The flag lives here
    /// because the hook is editor-installed but it needs a
    /// process-wide signal to detect animation context across the
    /// runtime's per-Core setter dispatch.
    static bool isAnimationContextActive() { return s_animationContextActive; }
    static void setAnimationContextActive(bool active)
    {
        s_animationContextActive = active;
    }

private:
    static inline OnPropertyChangingCallback s_onPropertyChanging = nullptr;
    static inline OnStringChangingCallback s_onStringChanging = nullptr;
    static inline OnFractionalIndexChangingCallback
        s_onFractionalIndexChanging = nullptr;
    static inline bool s_animationContextActive = false;

public:
#ifdef WITH_RIVE_EDITOR
    /// Pointer to the `ObjectArena` that owns this Core. Set by
    /// `ObjectArenaImpl::allocate` when the slot is filled. Used by
    /// `Core::editorArena()` so any Core method can resolve a
    /// `CoreHandle` to its target without threading the arena
    /// through every accessor signature.
    ///
    /// Stays null on Cores never allocated through an arena (test
    /// fixtures, or runtime-imported Cores held by `Artboard::
    /// m_Objects`). Resolves on those Cores will fail, but those
    /// Cores also don't hold cross-Core `CoreHandle` references —
    /// the runtime keeps raw pointers via `#ifndef WITH_RIVE_EDITOR`
    /// branches, so the arena is never touched on the runtime hot
    /// path.
    void editorSetArena(ObjectArena* arena) { m_arena = arena; }
    ObjectArena* editorArena() const { return m_arena; }

private:
    ObjectArena* m_arena = nullptr;

public:
    /// Editor-mode validation flag — mirrors Dart's `Core._hasValidated`
    /// from packages/core/lib/core.dart:187. Generated setters in
    /// editor builds guard their `${name}Changed()` callback on this
    /// bool so that property mutations applied during coop-apply
    /// hydration (before `onAddedDirty` / `onAddedClean` have wired
    /// `m_Artboard` / `m_Parent`) don't invoke runtime side effects
    /// that assume those pointers are valid.
    ///
    /// editor_native flips this to `true` at the end of its five-pass
    /// coop-apply flow (post-`onAddedClean`) so subsequent peer
    /// mutations fire callbacks normally. Runtime-only builds compile
    /// these members out entirely — the generator emits the callback
    /// call unconditionally in the non-`WITH_RIVE_EDITOR` branch.
    bool hasValidated() const { return m_hasValidated; }
    void markValidated() { m_hasValidated = true; }

private:
    bool m_hasValidated = false;

public:
#endif

    /// Apply a property change via the public setter path — fires
    /// ${name}Changed() and onPropertyChanging. Used by coop
    /// APPLY_COOP_CHANGES and SET_PROPERTY dispatch.
    ///
    /// Contrast with deserialize(): deserialize writes the private field
    /// directly during file load (no Changed/onPropertyChanging), which is
    /// correct for initial load but wrong for user/coop mutations. Always
    /// use applyChange for mutations arriving after load.
    ///
    /// Returns true if the property was recognized at this type's level
    /// or any super's; false if the key is unknown. Top-level `*Base::
    /// applyChange` overrides chain here, giving this body the chance
    /// to handle Core-level keys (currently none) and otherwise return
    /// false so the caller can route the unknown key elsewhere.
    virtual bool applyChange(uint16_t propertyKey, BinaryReader& reader)
    {
        return false;
    }
#endif
};
} // namespace rive
#endif

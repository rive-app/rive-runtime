#ifndef _RIVE_CORE_HPP_
#define _RIVE_CORE_HPP_

#include "rive/rive_types.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/status_code.hpp"

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
};
} // namespace rive
#endif

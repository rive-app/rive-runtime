#ifndef _RIVE_CORE_HPP_
#define _RIVE_CORE_HPP_

#include "rive/rive_types.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/status_code.hpp"

namespace rive
{
class CoreContext;
class ImportStack;
class Core
{
public:
    Core() = default;
    Core(const Core&) = default;
    const uint32_t emptyId = -1;
    static const int invalidPropertyKey = 0;
    virtual ~Core() {}
    virtual uint16_t coreType() const = 0;
    virtual bool isTypeOf(uint16_t typeKey) const = 0;
    virtual bool deserialize(uint16_t propertyKey, BinaryReader& reader) = 0;

    template <typename T> inline bool is() const { return isTypeOf(T::typeKey); }
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

    /// Called when the object is first added to the context, other objects
    /// may not have resolved their dependencies yet. This is an opportunity
    /// to look up objects referenced by id, but not assume that they in
    /// turn have resolved their references yet. Called during
    /// load/instance.
    virtual StatusCode onAddedDirty(CoreContext* context) { return StatusCode::Ok; }

    /// Called when all the objects in the context have had onAddedDirty
    /// called. This is an opportunity to reference things referenced by
    /// dependencies. (A path should be able to find a Shape somewhere in
    /// its hierarchy, which may be multiple levels up).
    virtual StatusCode onAddedClean(CoreContext* context) { return StatusCode::Ok; }

    virtual StatusCode import(ImportStack& importStack) { return StatusCode::Ok; }
};
} // namespace rive
#endif

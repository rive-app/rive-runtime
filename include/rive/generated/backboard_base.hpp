#ifndef _RIVE_BACKBOARD_BASE_HPP_
#define _RIVE_BACKBOARD_BASE_HPP_
#include "rive/core.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class BackboardBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 23;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BackboardBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

public:
    Core* clone() const override;
    void copy(const BackboardBase& object)
    {
        RIVE_EDITOR_COPY(object);
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/backboard_ext.inl"
#endif
};
} // namespace rive

#endif
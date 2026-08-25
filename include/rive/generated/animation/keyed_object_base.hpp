#ifndef _RIVE_KEYED_OBJECT_BASE_HPP_
#define _RIVE_KEYED_OBJECT_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class KeyedObjectBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 25;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case KeyedObjectBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t objectIdPropertyKey = 51;

protected:
    Id m_ObjectId = 0;

public:
    inline Id objectId() const { return m_ObjectId; }
    void objectId(Id value)
    {
        if (m_ObjectId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(objectIdPropertyKey, &m_ObjectId, &value);
        m_ObjectId = value;
        RIVE_EDITOR_CHANGED(objectIdChanged());
        notifyPropertyChanged(objectIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const KeyedObjectBase& object)
    {
        m_ObjectId = object.m_ObjectId;
        RIVE_EDITOR_COPY(object);
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case objectIdPropertyKey:
                m_ObjectId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }

protected:
    virtual void objectIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/keyed_object_ext.inl"
#endif
};
} // namespace rive

#endif
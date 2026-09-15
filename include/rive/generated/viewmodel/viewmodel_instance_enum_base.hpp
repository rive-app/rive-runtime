#ifndef _RIVE_VIEW_MODEL_INSTANCE_ENUM_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_ENUM_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ViewModelInstanceEnumBase : public ViewModelInstanceValue
{
protected:
    typedef ViewModelInstanceValue Super;

public:
    static const uint16_t typeKey = 432;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceEnumBase::typeKey:
            case ViewModelInstanceValueBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyValuePropertyKey = 560;

protected:
    Id m_PropertyValue = 0;

public:
    inline Id propertyValue() const { return m_PropertyValue; }
    void propertyValue(Id value)
    {
        if (m_PropertyValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(propertyValuePropertyKey,
                             &m_PropertyValue,
                             &value);
        m_PropertyValue = value;
        RIVE_EDITOR_CHANGED(propertyValueChanged());
        notifyPropertyChanged(propertyValuePropertyKey);
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceEnumBase& object)
    {
        m_PropertyValue = object.m_PropertyValue;
        RIVE_EDITOR_COPY(object);
        ViewModelInstanceValue::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyValuePropertyKey:
                m_PropertyValue = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ViewModelInstanceValue::deserialize(propertyKey, reader);
    }

protected:
    virtual void propertyValueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/viewmodel_instance_enum_ext.inl"
#endif
};
} // namespace rive

#endif
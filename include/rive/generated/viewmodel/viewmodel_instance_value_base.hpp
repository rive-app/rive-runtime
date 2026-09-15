#ifndef _RIVE_VIEW_MODEL_INSTANCE_VALUE_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_VALUE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ViewModelInstanceValueBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 428;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceValueBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelPropertyIdPropertyKey = 554;

protected:
    Id m_ViewModelPropertyId = 0;

public:
    inline Id viewModelPropertyId() const { return m_ViewModelPropertyId; }
    void viewModelPropertyId(Id value)
    {
        if (m_ViewModelPropertyId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(viewModelPropertyIdPropertyKey,
                             &m_ViewModelPropertyId,
                             &value);
        m_ViewModelPropertyId = value;
        RIVE_EDITOR_CHANGED(viewModelPropertyIdChanged());
        notifyPropertyChanged(viewModelPropertyIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceValueBase& object)
    {
        m_ViewModelPropertyId = object.m_ViewModelPropertyId;
        RIVE_EDITOR_COPY(object);
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelPropertyIdPropertyKey:
                m_ViewModelPropertyId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void viewModelPropertyIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/viewmodel_instance_value_ext.inl"
#endif
};
} // namespace rive

#endif
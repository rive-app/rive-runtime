#ifndef _RIVE_VIEW_MODEL_INSTANCE_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ViewModelInstanceBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 437;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelIdPropertyKey = 566;

protected:
    Id m_ViewModelId = 0;

public:
    inline Id viewModelId() const { return m_ViewModelId; }
    void viewModelId(Id value)
    {
        if (m_ViewModelId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(viewModelIdPropertyKey, &m_ViewModelId, &value);
        m_ViewModelId = value;
        RIVE_EDITOR_CHANGED(viewModelIdChanged());
        notifyPropertyChanged(viewModelIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceBase& object)
    {
        m_ViewModelId = object.m_ViewModelId;
        RIVE_EDITOR_COPY(object);
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelIdPropertyKey:
                m_ViewModelId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void viewModelIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/viewmodel_instance_ext.inl"
#endif
};
} // namespace rive

#endif
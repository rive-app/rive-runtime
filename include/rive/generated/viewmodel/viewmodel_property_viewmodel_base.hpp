#ifndef _RIVE_VIEW_MODEL_PROPERTY_VIEW_MODEL_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_VIEW_MODEL_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
namespace rive
{
class ViewModelPropertyViewModelBase : public ViewModelProperty
{
protected:
    typedef ViewModelProperty Super;

public:
    static const uint16_t typeKey = 436;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelPropertyViewModelBase::typeKey:
            case ViewModelPropertyBase::typeKey:
            case ViewModelComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelReferenceIdPropertyKey = 565;

protected:
    Id m_ViewModelReferenceId = 0;

public:
    inline Id viewModelReferenceId() const { return m_ViewModelReferenceId; }
    void viewModelReferenceId(Id value)
    {
        if (m_ViewModelReferenceId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(viewModelReferenceIdPropertyKey,
                             &m_ViewModelReferenceId,
                             &value);
        m_ViewModelReferenceId = value;
        RIVE_EDITOR_CHANGED(viewModelReferenceIdChanged());
        notifyPropertyChanged(viewModelReferenceIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const ViewModelPropertyViewModelBase& object)
    {
        m_ViewModelReferenceId = object.m_ViewModelReferenceId;
        ViewModelProperty::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelReferenceIdPropertyKey:
                m_ViewModelReferenceId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return ViewModelProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void viewModelReferenceIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/viewmodel_property_viewmodel_ext.inl"
#endif
};
} // namespace rive

#endif
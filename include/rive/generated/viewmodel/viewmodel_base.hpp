#ifndef _RIVE_VIEW_MODEL_BASE_HPP_
#define _RIVE_VIEW_MODEL_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/viewmodel_component.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ViewModelBase : public ViewModelComponent
{
protected:
    typedef ViewModelComponent Super;

public:
    static const uint16_t typeKey = 435;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelBase::typeKey:
            case ViewModelComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelTypePropertyKey = 981;

protected:
    uint32_t m_ViewModelType = 0;

public:
    inline uint32_t viewModelType() const { return m_ViewModelType; }
    void viewModelType(uint32_t value)
    {
        if (m_ViewModelType == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(viewModelTypePropertyKey,
                             &m_ViewModelType,
                             &value);
        m_ViewModelType = value;
        RIVE_EDITOR_CHANGED(viewModelTypeChanged());
        notifyPropertyChanged(viewModelTypePropertyKey);
    }

    Core* clone() const override;
    void copy(const ViewModelBase& object)
    {
        m_ViewModelType = object.m_ViewModelType;
        RIVE_EDITOR_COPY(object);
        ViewModelComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelTypePropertyKey:
                m_ViewModelType = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ViewModelComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void viewModelTypeChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/viewmodel/viewmodel_ext.inl"
#endif
};
} // namespace rive

#endif
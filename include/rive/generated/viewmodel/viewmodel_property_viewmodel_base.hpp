#ifndef _RIVE_VIEW_MODEL_PROPERTY_VIEW_MODEL_BASE_HPP_
#define _RIVE_VIEW_MODEL_PROPERTY_VIEW_MODEL_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"
namespace rive
{
class ViewModelPropertyViewModelBase : public ViewModelProperty
{
protected:
    typedef ViewModelProperty Super;

public:
    static const uint16_t typeKey = 436;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

private:
    uint32_t m_ViewModelReferenceId = 0;

public:
    inline uint32_t viewModelReferenceId() const { return m_ViewModelReferenceId; }
    void viewModelReferenceId(uint32_t value)
    {
        if (m_ViewModelReferenceId == value)
        {
            return;
        }
        m_ViewModelReferenceId = value;
        viewModelReferenceIdChanged();
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
                m_ViewModelReferenceId = CoreUintType::deserialize(reader);
                return true;
        }
        return ViewModelProperty::deserialize(propertyKey, reader);
    }

protected:
    virtual void viewModelReferenceIdChanged() {}
};
} // namespace rive

#endif
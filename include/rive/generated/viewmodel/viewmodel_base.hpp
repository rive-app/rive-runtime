#ifndef _RIVE_VIEW_MODEL_BASE_HPP_
#define _RIVE_VIEW_MODEL_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/viewmodel_component.hpp"
namespace rive
{
class ViewModelBase : public ViewModelComponent
{
protected:
    typedef ViewModelComponent Super;

public:
    static const uint16_t typeKey = 435;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

    static const uint16_t defaultInstanceIdPropertyKey = 564;

private:
    uint32_t m_DefaultInstanceId = -1;

public:
    inline uint32_t defaultInstanceId() const { return m_DefaultInstanceId; }
    void defaultInstanceId(uint32_t value)
    {
        if (m_DefaultInstanceId == value)
        {
            return;
        }
        m_DefaultInstanceId = value;
        defaultInstanceIdChanged();
    }

    Core* clone() const override;
    void copy(const ViewModelBase& object)
    {
        m_DefaultInstanceId = object.m_DefaultInstanceId;
        ViewModelComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case defaultInstanceIdPropertyKey:
                m_DefaultInstanceId = CoreUintType::deserialize(reader);
                return true;
        }
        return ViewModelComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void defaultInstanceIdChanged() {}
};
} // namespace rive

#endif
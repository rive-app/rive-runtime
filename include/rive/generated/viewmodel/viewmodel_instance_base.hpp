#ifndef _RIVE_VIEW_MODEL_INSTANCE_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ViewModelInstanceBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 437;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelIdPropertyKey = 566;

private:
    uint32_t m_ViewModelId = 0;

public:
    inline uint32_t viewModelId() const { return m_ViewModelId; }
    void viewModelId(uint32_t value)
    {
        if (m_ViewModelId == value)
        {
            return;
        }
        m_ViewModelId = value;
        viewModelIdChanged();
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceBase& object)
    {
        m_ViewModelId = object.m_ViewModelId;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelIdPropertyKey:
                m_ViewModelId = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void viewModelIdChanged() {}
};
} // namespace rive

#endif
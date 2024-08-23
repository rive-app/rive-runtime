#ifndef _RIVE_VIEW_MODEL_INSTANCE_NUMBER_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_NUMBER_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
namespace rive
{
class ViewModelInstanceNumberBase : public ViewModelInstanceValue
{
protected:
    typedef ViewModelInstanceValue Super;

public:
    static const uint16_t typeKey = 442;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceNumberBase::typeKey:
            case ViewModelInstanceValueBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t propertyValuePropertyKey = 575;

private:
    float m_PropertyValue = 0.0f;

public:
    inline float propertyValue() const { return m_PropertyValue; }
    void propertyValue(float value)
    {
        if (m_PropertyValue == value)
        {
            return;
        }
        m_PropertyValue = value;
        propertyValueChanged();
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceNumberBase& object)
    {
        m_PropertyValue = object.m_PropertyValue;
        ViewModelInstanceValue::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case propertyValuePropertyKey:
                m_PropertyValue = CoreDoubleType::deserialize(reader);
                return true;
        }
        return ViewModelInstanceValue::deserialize(propertyKey, reader);
    }

protected:
    virtual void propertyValueChanged() {}
};
} // namespace rive

#endif
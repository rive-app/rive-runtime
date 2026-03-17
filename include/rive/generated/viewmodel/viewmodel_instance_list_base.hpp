#ifndef _RIVE_VIEW_MODEL_INSTANCE_LIST_BASE_HPP_
#define _RIVE_VIEW_MODEL_INSTANCE_LIST_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
namespace rive
{
class ViewModelInstanceListBase : public ViewModelInstanceValue
{
protected:
    typedef ViewModelInstanceValue Super;

public:
    static const uint16_t typeKey = 441;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelInstanceListBase::typeKey:
            case ViewModelInstanceValueBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t listSourcePropertyKey = 966;

protected:
    uint32_t m_ListSource = -1;

public:
    inline uint32_t listSource() const { return m_ListSource; }
    void listSource(uint32_t value)
    {
        if (m_ListSource == value)
        {
            return;
        }
        m_ListSource = value;
        listSourceChanged();
    }

    Core* clone() const override;
    void copy(const ViewModelInstanceListBase& object)
    {
        m_ListSource = object.m_ListSource;
        ViewModelInstanceValue::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case listSourcePropertyKey:
                m_ListSource = CoreUintType::deserialize(reader);
                return true;
        }
        return ViewModelInstanceValue::deserialize(propertyKey, reader);
    }

protected:
    virtual void listSourceChanged() {}
};
} // namespace rive

#endif
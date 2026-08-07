#ifndef _RIVE_COMPONENT_BASE_HPP_
#define _RIVE_COMPONENT_BASE_HPP_
#include <string>
#include "rive/core.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/sidecar.hpp"
namespace rive
{
struct ComponentNameSidecar
{
    std::string name = "";
};
class ComponentBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 10;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t namePropertyKey = 4;
    static const uint16_t parentIdPropertyKey = 5;

protected:
    uint32_t m_ParentId = 0;
    Sidecar<ComponentNameSidecar> m_name;

public:
    inline const std::string& name() const
    {
        static const std::string defaultValue = "";
        auto* sidecar = m_name.get();
        return sidecar != nullptr ? sidecar->name : defaultValue;
    }
    void name(std::string value)
    {
        if (name() == value)
        {
            return;
        }
        m_name.ensure()->name = value;
        nameChanged();
        notifyPropertyChanged(namePropertyKey);
    }

    inline uint32_t parentId() const { return m_ParentId; }
    void parentId(uint32_t value)
    {
        if (m_ParentId == value)
        {
            return;
        }
        m_ParentId = value;
        parentIdChanged();
        notifyPropertyChanged(parentIdPropertyKey);
    }

    void copy(const ComponentBase& object)
    {
        m_ParentId = object.m_ParentId;
        m_name = object.m_name;
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case namePropertyKey:
                m_name.ensure()->name = CoreStringType::deserialize(reader);
                return true;
            case parentIdPropertyKey:
                m_ParentId = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void nameChanged() {}
    virtual void parentIdChanged() {}
};
} // namespace rive

#endif
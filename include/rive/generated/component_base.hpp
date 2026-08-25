#ifndef _RIVE_COMPONENT_BASE_HPP_
#define _RIVE_COMPONENT_BASE_HPP_
#include <string>
#include "rive/core.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/id.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
#ifndef WITH_RIVE_EDITOR
#include "rive/sidecar.hpp"
#endif
namespace rive
{
#ifndef WITH_RIVE_EDITOR
struct ComponentNameSidecar
{
    std::string name = "";
};
#endif
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
#ifdef WITH_RIVE_EDITOR
    std::string m_Name = "";
#endif
    Id m_ParentId = 0;
#ifndef WITH_RIVE_EDITOR
    Sidecar<ComponentNameSidecar> m_name;
#endif
public:
#ifdef WITH_RIVE_EDITOR
    inline const std::string& name() const { return m_Name; }
    void name(std::string value)
    {
        if (m_Name == value)
        {
            return;
        }
        RIVE_EDITOR_STRING_CHANGING(namePropertyKey, m_Name, value);
        m_Name = value;
        RIVE_EDITOR_CHANGED(nameChanged());
        notifyPropertyChanged(namePropertyKey);
    }
#else
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
        m_name.ensureAllocated()->name = value;
        nameChanged();
        notifyPropertyChanged(namePropertyKey);
    }
#endif

    inline Id parentId() const { return m_ParentId; }
    void parentId(Id value)
    {
        if (m_ParentId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(parentIdPropertyKey, &m_ParentId, &value);
        m_ParentId = value;
        RIVE_EDITOR_CHANGED(parentIdChanged());
        notifyPropertyChanged(parentIdPropertyKey);
    }

    void copy(const ComponentBase& object)
    {
#ifdef WITH_RIVE_EDITOR
        m_Name = object.m_Name;
#endif
        m_ParentId = object.m_ParentId;
#ifndef WITH_RIVE_EDITOR
        m_name = object.m_name;
#endif
        RIVE_EDITOR_COPY(object);
        RIVE_EDITOR_COPY_VALIDATED(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case namePropertyKey:
#ifdef WITH_RIVE_EDITOR
                m_Name = CoreStringType::deserialize(reader);
#else
                m_name.ensureAllocated()->name =
                    CoreStringType::deserialize(reader);
#endif
                return true;
            case parentIdPropertyKey:
                m_ParentId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return false;
    }

protected:
    virtual void nameChanged() {}
    virtual void parentIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/component_ext.inl"
#endif
};
} // namespace rive

#endif
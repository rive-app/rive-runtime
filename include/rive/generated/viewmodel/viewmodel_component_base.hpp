#ifndef _RIVE_VIEW_MODEL_COMPONENT_BASE_HPP_
#define _RIVE_VIEW_MODEL_COMPONENT_BASE_HPP_
#include <string>
#include "rive/core.hpp"
#include "rive/core/field_types/core_string_type.hpp"
namespace rive
{
class ViewModelComponentBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 429;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ViewModelComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t namePropertyKey = 557;

private:
    std::string m_Name = "";

public:
    inline const std::string& name() const { return m_Name; }
    void name(std::string value)
    {
        if (m_Name == value)
        {
            return;
        }
        m_Name = value;
        nameChanged();
    }

    Core* clone() const override;
    void copy(const ViewModelComponentBase& object) { m_Name = object.m_Name; }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case namePropertyKey:
                m_Name = CoreStringType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void nameChanged() {}
};
} // namespace rive

#endif
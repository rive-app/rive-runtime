#ifndef _RIVE_ASSET_BASE_HPP_
#define _RIVE_ASSET_BASE_HPP_
#include <string>
#include "rive/core.hpp"
#include "rive/core/field_types/core_string_type.hpp"
namespace rive
{
class AssetBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 99;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case AssetBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t namePropertyKey = 203;

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

    void copy(const AssetBase& object) { m_Name = object.m_Name; }

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
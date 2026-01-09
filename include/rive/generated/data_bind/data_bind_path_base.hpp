#ifndef _RIVE_DATA_BIND_PATH_BASE_HPP_
#define _RIVE_DATA_BIND_PATH_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/span.hpp"
namespace rive
{
class DataBindPathBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 643;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataBindPathBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t pathPropertyKey = 920;
    static const uint16_t isRelativePropertyKey = 921;

protected:
    bool m_IsRelative = false;

public:
    virtual void decodePath(Span<const uint8_t> value) = 0;
    virtual void copyPath(const DataBindPathBase& object) = 0;

    inline bool isRelative() const { return m_IsRelative; }
    void isRelative(bool value)
    {
        if (m_IsRelative == value)
        {
            return;
        }
        m_IsRelative = value;
        isRelativeChanged();
    }

    Core* clone() const override;
    void copy(const DataBindPathBase& object)
    {
        copyPath(object);
        m_IsRelative = object.m_IsRelative;
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case pathPropertyKey:
                decodePath(CoreBytesType::deserialize(reader));
                return true;
            case isRelativePropertyKey:
                m_IsRelative = CoreBoolType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void pathChanged() {}
    virtual void isRelativeChanged() {}
};
} // namespace rive

#endif
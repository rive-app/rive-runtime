#ifndef _RIVE_DATA_BIND_CONTEXT_BASE_HPP_
#define _RIVE_DATA_BIND_CONTEXT_BASE_HPP_
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/span.hpp"
namespace rive
{
class DataBindContextBase : public DataBind
{
protected:
    typedef DataBind Super;

public:
    static const uint16_t typeKey = 447;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataBindContextBase::typeKey:
            case DataBindBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t sourcePathIdsPropertyKey = 588;

public:
    virtual void decodeSourcePathIds(Span<const uint8_t> value) = 0;
    virtual void copySourcePathIds(const DataBindContextBase& object) = 0;

    Core* clone() const override;
    void copy(const DataBindContextBase& object)
    {
        copySourcePathIds(object);
        DataBind::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case sourcePathIdsPropertyKey:
                decodeSourcePathIds(CoreBytesType::deserialize(reader));
                return true;
        }
        return DataBind::deserialize(propertyKey, reader);
    }

protected:
    virtual void sourcePathIdsChanged() {}
};
} // namespace rive

#endif
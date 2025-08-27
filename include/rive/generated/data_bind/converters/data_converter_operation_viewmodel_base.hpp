#ifndef _RIVE_DATA_CONVERTER_OPERATION_VIEW_MODEL_BASE_HPP_
#define _RIVE_DATA_CONVERTER_OPERATION_VIEW_MODEL_BASE_HPP_
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/data_bind/converters/data_converter_operation.hpp"
#include "rive/span.hpp"
namespace rive
{
class DataConverterOperationViewModelBase : public DataConverterOperation
{
protected:
    typedef DataConverterOperation Super;

public:
    static const uint16_t typeKey = 517;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case DataConverterOperationViewModelBase::typeKey:
            case DataConverterOperationBase::typeKey:
            case DataConverterBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t sourcePathIdsPropertyKey = 711;

public:
    virtual void decodeSourcePathIds(Span<const uint8_t> value) = 0;
    virtual void copySourcePathIds(
        const DataConverterOperationViewModelBase& object) = 0;

    Core* clone() const override;
    void copy(const DataConverterOperationViewModelBase& object)
    {
        copySourcePathIds(object);
        DataConverterOperation::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case sourcePathIdsPropertyKey:
                decodeSourcePathIds(CoreBytesType::deserialize(reader));
                return true;
        }
        return DataConverterOperation::deserialize(propertyKey, reader);
    }

protected:
    virtual void sourcePathIdsChanged() {}
};
} // namespace rive

#endif
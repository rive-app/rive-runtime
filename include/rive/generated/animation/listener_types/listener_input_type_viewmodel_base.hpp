#ifndef _RIVE_LISTENER_INPUT_TYPE_VIEW_MODEL_BASE_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_VIEW_MODEL_BASE_HPP_
#include "rive/animation/listener_types/listener_input_type.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/span.hpp"
namespace rive
{
class ListenerInputTypeViewModelBase : public ListenerInputType
{
protected:
    typedef ListenerInputType Super;

public:
    static const uint16_t typeKey = 660;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerInputTypeViewModelBase::typeKey:
            case ListenerInputTypeBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t viewModelPathIdsPropertyKey = 963;

public:
    virtual void decodeViewModelPathIds(Span<const uint8_t> value) = 0;
    virtual void copyViewModelPathIds(
        const ListenerInputTypeViewModelBase& object) = 0;

    Core* clone() const override;
    void copy(const ListenerInputTypeViewModelBase& object)
    {
        copyViewModelPathIds(object);
        ListenerInputType::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case viewModelPathIdsPropertyKey:
                decodeViewModelPathIds(CoreBytesType::deserialize(reader));
                return true;
        }
        return ListenerInputType::deserialize(propertyKey, reader);
    }

protected:
    virtual void viewModelPathIdsChanged() {}
};
} // namespace rive

#endif
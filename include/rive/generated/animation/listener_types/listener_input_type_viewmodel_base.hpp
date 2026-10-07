#ifndef _RIVE_LISTENER_INPUT_TYPE_VIEW_MODEL_BASE_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_VIEW_MODEL_BASE_HPP_
#include "rive/animation/listener_types/listener_input_type.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/span.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ListenerInputTypeViewModelBase : public ListenerInputType
{
protected:
    typedef ListenerInputType Super;

public:
    static const uint16_t typeKey = 660;

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
        RIVE_EDITOR_COPY(object);
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
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ListenerInputType::deserialize(propertyKey, reader);
    }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/listener_types/listener_input_type_viewmodel_ext.inl"
#endif
};
} // namespace rive

#endif
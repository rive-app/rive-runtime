#ifndef _RIVE_LISTENER_VIEW_MODEL_CHANGE_BASE_HPP_
#define _RIVE_LISTENER_VIEW_MODEL_CHANGE_BASE_HPP_
#include "rive/animation/listener_action.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class ListenerViewModelChangeBase : public ListenerAction
{
protected:
    typedef ListenerAction Super;

public:
    static const uint16_t typeKey = 487;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListenerViewModelChangeBase::typeKey:
            case ListenerActionBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

public:
    Core* clone() const override;
    void copy(const ListenerViewModelChangeBase& object)
    {
        RIVE_EDITOR_COPY(object);
        ListenerAction::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return ListenerAction::deserialize(propertyKey, reader);
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/listener_viewmodel_change_ext.inl"
#endif
};
} // namespace rive

#endif
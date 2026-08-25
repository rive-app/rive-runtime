#ifndef _RIVE_SOLO_BASE_HPP_
#define _RIVE_SOLO_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/node.hpp"
namespace rive
{
class SoloBase : public Node
{
protected:
    typedef Node Super;

public:
    static const uint16_t typeKey = 147;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case SoloBase::typeKey:
            case NodeBase::typeKey:
            case TransformComponentBase::typeKey:
            case WorldTransformComponentBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t activeComponentIdPropertyKey = 296;

protected:
    Id m_ActiveComponentId = 0;

public:
    inline Id activeComponentId() const { return m_ActiveComponentId; }
    void activeComponentId(Id value)
    {
        if (m_ActiveComponentId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(activeComponentIdPropertyKey,
                             &m_ActiveComponentId,
                             &value);
        m_ActiveComponentId = value;
        RIVE_EDITOR_CHANGED(activeComponentIdChanged());
        notifyPropertyChanged(activeComponentIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const SoloBase& object)
    {
        m_ActiveComponentId = object.m_ActiveComponentId;
        Node::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case activeComponentIdPropertyKey:
                m_ActiveComponentId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return Node::deserialize(propertyKey, reader);
    }

protected:
    virtual void activeComponentIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/solo_ext.inl"
#endif
};
} // namespace rive

#endif
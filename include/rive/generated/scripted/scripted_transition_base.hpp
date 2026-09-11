#ifndef _RIVE_SCRIPTED_TRANSITION_BASE_HPP_
#define _RIVE_SCRIPTED_TRANSITION_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/scripted/scripted_drawable.hpp"
namespace rive
{
class ScriptedTransitionBase : public ScriptedDrawable
{
protected:
    typedef ScriptedDrawable Super;

public:
    static const uint16_t typeKey = 110;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScriptedTransitionBase::typeKey:
            case ScriptedDrawableBase::typeKey:
            case DrawableBase::typeKey:
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

    static const uint16_t activeComponentIdPropertyKey = 273;
    static const uint16_t listSourcePropertyKey = 414;

protected:
    Id m_ActiveComponentId = 0;
    Id m_ListSource = kEmptyId;

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

    inline Id listSource() const { return m_ListSource; }
    void listSource(Id value)
    {
        if (m_ListSource == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(listSourcePropertyKey, &m_ListSource, &value);
        m_ListSource = value;
        RIVE_EDITOR_CHANGED(listSourceChanged());
        notifyPropertyChanged(listSourcePropertyKey);
    }

    Core* clone() const override;
    void copy(const ScriptedTransitionBase& object)
    {
        m_ActiveComponentId = object.m_ActiveComponentId;
        m_ListSource = object.m_ListSource;
        ScriptedDrawable::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case activeComponentIdPropertyKey:
                m_ActiveComponentId = CoreIdType::runtimeDeserialize(reader);
                return true;
            case listSourcePropertyKey:
                m_ListSource = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return ScriptedDrawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void activeComponentIdChanged() {}
    virtual void listSourceChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/scripted/scripted_transition_ext.inl"
#endif
};
} // namespace rive

#endif
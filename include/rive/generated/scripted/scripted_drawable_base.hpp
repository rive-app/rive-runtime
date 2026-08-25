#ifndef _RIVE_SCRIPTED_DRAWABLE_BASE_HPP_
#define _RIVE_SCRIPTED_DRAWABLE_BASE_HPP_
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
#include "rive/drawable.hpp"
namespace rive
{
class ScriptedDrawableBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 603;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
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

    static const uint16_t scriptAssetIdPropertyKey = 848;

protected:
    Id m_ScriptAssetId = kEmptyId;

public:
    inline Id scriptAssetId() const { return m_ScriptAssetId; }
    void scriptAssetId(Id value)
    {
        if (m_ScriptAssetId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(scriptAssetIdPropertyKey,
                             &m_ScriptAssetId,
                             &value);
        m_ScriptAssetId = value;
        RIVE_EDITOR_CHANGED(scriptAssetIdChanged());
        notifyPropertyChanged(scriptAssetIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const ScriptedDrawableBase& object)
    {
        m_ScriptAssetId = object.m_ScriptAssetId;
        Drawable::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case scriptAssetIdPropertyKey:
                m_ScriptAssetId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return Drawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void scriptAssetIdChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/scripted/scripted_drawable_ext.inl"
#endif
};
} // namespace rive

#endif
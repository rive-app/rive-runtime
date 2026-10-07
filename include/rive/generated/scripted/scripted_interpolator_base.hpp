#ifndef _RIVE_SCRIPTED_INTERPOLATOR_BASE_HPP_
#define _RIVE_SCRIPTED_INTERPOLATOR_BASE_HPP_
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class ScriptedInterpolatorBase : public KeyFrameInterpolator
{
protected:
    typedef KeyFrameInterpolator Super;

public:
    static const uint16_t typeKey = 972;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t scriptAssetIdPropertyKey = 1015;

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
        notifyPropertyChanged(scriptAssetIdPropertyKey);
    }

    Core* clone() const override;
    void copy(const ScriptedInterpolatorBase& object)
    {
        m_ScriptAssetId = object.m_ScriptAssetId;
        KeyFrameInterpolator::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case scriptAssetIdPropertyKey:
                m_ScriptAssetId = CoreIdType::runtimeDeserialize(reader);
                return true;
        }
        return KeyFrameInterpolator::deserialize(propertyKey, reader);
    }

protected:
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/scripted/scripted_interpolator_ext.inl"
#endif
};
} // namespace rive

#endif
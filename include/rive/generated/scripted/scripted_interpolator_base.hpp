#ifndef _RIVE_SCRIPTED_INTERPOLATOR_BASE_HPP_
#define _RIVE_SCRIPTED_INTERPOLATOR_BASE_HPP_
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ScriptedInterpolatorBase : public KeyFrameInterpolator
{
protected:
    typedef KeyFrameInterpolator Super;

public:
    static const uint16_t typeKey = 972;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScriptedInterpolatorBase::typeKey:
            case KeyFrameInterpolatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t scriptAssetIdPropertyKey = 1015;

protected:
    uint32_t m_ScriptAssetId = -1;

public:
    inline uint32_t scriptAssetId() const { return m_ScriptAssetId; }
    void scriptAssetId(uint32_t value)
    {
        if (m_ScriptAssetId == value)
        {
            return;
        }
        m_ScriptAssetId = value;
        scriptAssetIdChanged();
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
                m_ScriptAssetId = CoreUintType::deserialize(reader);
                return true;
        }
        return KeyFrameInterpolator::deserialize(propertyKey, reader);
    }

protected:
    virtual void scriptAssetIdChanged() {}
};
} // namespace rive

#endif
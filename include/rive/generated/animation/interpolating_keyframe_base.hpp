#ifndef _RIVE_INTERPOLATING_KEY_FRAME_BASE_HPP_
#define _RIVE_INTERPOLATING_KEY_FRAME_BASE_HPP_
#include "rive/animation/keyframe.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class InterpolatingKeyFrameBase : public KeyFrame
{
protected:
    typedef KeyFrame Super;

public:
    static const uint16_t typeKey = 170;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case InterpolatingKeyFrameBase::typeKey:
            case KeyFrameBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t interpolationTypePropertyKey = 68;
    static const uint16_t interpolatorIdPropertyKey = 69;

private:
    uint32_t m_InterpolationType = 0;
    uint32_t m_InterpolatorId = -1;

public:
    inline uint32_t interpolationType() const { return m_InterpolationType; }
    void interpolationType(uint32_t value)
    {
        if (m_InterpolationType == value)
        {
            return;
        }
        m_InterpolationType = value;
        interpolationTypeChanged();
    }

    inline uint32_t interpolatorId() const { return m_InterpolatorId; }
    void interpolatorId(uint32_t value)
    {
        if (m_InterpolatorId == value)
        {
            return;
        }
        m_InterpolatorId = value;
        interpolatorIdChanged();
    }

    void copy(const InterpolatingKeyFrameBase& object)
    {
        m_InterpolationType = object.m_InterpolationType;
        m_InterpolatorId = object.m_InterpolatorId;
        KeyFrame::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case interpolationTypePropertyKey:
                m_InterpolationType = CoreUintType::deserialize(reader);
                return true;
            case interpolatorIdPropertyKey:
                m_InterpolatorId = CoreUintType::deserialize(reader);
                return true;
        }
        return KeyFrame::deserialize(propertyKey, reader);
    }

protected:
    virtual void interpolationTypeChanged() {}
    virtual void interpolatorIdChanged() {}
};
} // namespace rive

#endif
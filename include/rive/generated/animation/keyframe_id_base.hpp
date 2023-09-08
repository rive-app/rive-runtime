#ifndef _RIVE_KEY_FRAME_ID_BASE_HPP_
#define _RIVE_KEY_FRAME_ID_BASE_HPP_
#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class KeyFrameIdBase : public InterpolatingKeyFrame
{
protected:
    typedef InterpolatingKeyFrame Super;

public:
    static const uint16_t typeKey = 50;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case KeyFrameIdBase::typeKey:
            case InterpolatingKeyFrameBase::typeKey:
            case KeyFrameBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t valuePropertyKey = 122;

private:
    uint32_t m_Value = -1;

public:
    inline uint32_t value() const { return m_Value; }
    void value(uint32_t value)
    {
        if (m_Value == value)
        {
            return;
        }
        m_Value = value;
        valueChanged();
    }

    Core* clone() const override;
    void copy(const KeyFrameIdBase& object)
    {
        m_Value = object.m_Value;
        InterpolatingKeyFrame::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreUintType::deserialize(reader);
                return true;
        }
        return InterpolatingKeyFrame::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
};
} // namespace rive

#endif
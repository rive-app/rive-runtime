#ifndef _RIVE_KEY_FRAME_COLOR_BASE_HPP_
#define _RIVE_KEY_FRAME_COLOR_BASE_HPP_
#include "rive/animation/interpolating_keyframe.hpp"
#include "rive/core/field_types/core_color_type.hpp"
namespace rive
{
class KeyFrameColorBase : public InterpolatingKeyFrame
{
protected:
    typedef InterpolatingKeyFrame Super;

public:
    static const uint16_t typeKey = 37;

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t valuePropertyKey = 88;

protected:
    int m_Value = 0;

public:
    inline int value() const { return m_Value; }
    void value(int value)
    {
        if (m_Value == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(valuePropertyKey, &m_Value, &value);
        m_Value = value;
        RIVE_EDITOR_CHANGED(valueChanged());
        notifyPropertyChanged(valuePropertyKey);
    }

    Core* clone() const override;
    void copy(const KeyFrameColorBase& object)
    {
        m_Value = object.m_Value;
        InterpolatingKeyFrame::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case valuePropertyKey:
                m_Value = CoreColorType::deserialize(reader);
                return true;
        }
        return InterpolatingKeyFrame::deserialize(propertyKey, reader);
    }

protected:
    virtual void valueChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/animation/keyframe_color_ext.inl"
#endif
};
} // namespace rive

#endif
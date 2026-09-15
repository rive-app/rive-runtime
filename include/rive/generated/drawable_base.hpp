#ifndef _RIVE_DRAWABLE_BASE_HPP_
#define _RIVE_DRAWABLE_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/node.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class DrawableBase : public Node
{
protected:
    typedef Node Super;

public:
    static const uint16_t typeKey = 13;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
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

    static const uint16_t blendModeValuePropertyKey = 23;
    static const uint16_t drawableFlagsPropertyKey = 129;

protected:
    uint8_t m_BlendModeValue = 3;
    uint16_t m_DrawableFlags = 0;

public:
    inline uint8_t blendModeValue() const { return m_BlendModeValue; }
    void blendModeValue(uint8_t value)
    {
        if (m_BlendModeValue == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(blendModeValuePropertyKey,
                             &m_BlendModeValue,
                             &value);
        m_BlendModeValue = value;
        RIVE_EDITOR_CHANGED(blendModeValueChanged());
        notifyPropertyChanged(blendModeValuePropertyKey);
    }

    inline uint16_t drawableFlags() const { return m_DrawableFlags; }
    void drawableFlags(uint16_t value)
    {
        if (m_DrawableFlags == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(drawableFlagsPropertyKey,
                             &m_DrawableFlags,
                             &value);
        m_DrawableFlags = value;
        RIVE_EDITOR_CHANGED(drawableFlagsChanged());
        notifyPropertyChanged(drawableFlagsPropertyKey);
    }

    void copy(const DrawableBase& object)
    {
        m_BlendModeValue = object.m_BlendModeValue;
        m_DrawableFlags = object.m_DrawableFlags;
        RIVE_EDITOR_COPY(object);
        Node::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case blendModeValuePropertyKey:
                m_BlendModeValue = CoreUintType::deserialize(reader);
                return true;
            case drawableFlagsPropertyKey:
                m_DrawableFlags = CoreUintType::deserialize(reader);
                return true;
        }
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return Node::deserialize(propertyKey, reader);
    }

protected:
    virtual void blendModeValueChanged() {}
    virtual void drawableFlagsChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/drawable_ext.inl"
#endif
};
} // namespace rive

#endif
#ifndef _RIVE_DRAWABLE_BASE_HPP_
#define _RIVE_DRAWABLE_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/node.hpp"
namespace rive
{
class DrawableBase : public Node
{
protected:
    typedef Node Super;

public:
    static const uint16_t typeKey = 13;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
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

private:
    uint32_t m_BlendModeValue = 3;
    uint32_t m_DrawableFlags = 0;

public:
    inline uint32_t blendModeValue() const { return m_BlendModeValue; }
    void blendModeValue(uint32_t value)
    {
        if (m_BlendModeValue == value)
        {
            return;
        }
        m_BlendModeValue = value;
        blendModeValueChanged();
    }

    inline uint32_t drawableFlags() const { return m_DrawableFlags; }
    void drawableFlags(uint32_t value)
    {
        if (m_DrawableFlags == value)
        {
            return;
        }
        m_DrawableFlags = value;
        drawableFlagsChanged();
    }

    void copy(const DrawableBase& object)
    {
        m_BlendModeValue = object.m_BlendModeValue;
        m_DrawableFlags = object.m_DrawableFlags;
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
        return Node::deserialize(propertyKey, reader);
    }

protected:
    virtual void blendModeValueChanged() {}
    virtual void drawableFlagsChanged() {}
};
} // namespace rive

#endif
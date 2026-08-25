#ifndef _RIVE_N_SLICED_NODE_BASE_HPP_
#define _RIVE_N_SLICED_NODE_BASE_HPP_
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/node.hpp"
namespace rive
{
class NSlicedNodeBase : public Node
{
protected:
    typedef Node Super;

public:
    static const uint16_t typeKey = 508;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NSlicedNodeBase::typeKey:
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

    static const uint16_t initialWidthPropertyKey = 697;
    static const uint16_t initialHeightPropertyKey = 698;
    static const uint16_t widthPropertyKey = 699;
    static const uint16_t heightPropertyKey = 700;

protected:
    float m_InitialWidth = 0.0f;
    float m_InitialHeight = 0.0f;
    float m_Width = 0.0f;
    float m_Height = 0.0f;

public:
    inline float initialWidth() const { return m_InitialWidth; }
    void initialWidth(float value)
    {
        if (m_InitialWidth == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(initialWidthPropertyKey, &m_InitialWidth, &value);
        m_InitialWidth = value;
        RIVE_EDITOR_CHANGED(initialWidthChanged());
        notifyPropertyChanged(initialWidthPropertyKey);
    }

    inline float initialHeight() const { return m_InitialHeight; }
    void initialHeight(float value)
    {
        if (m_InitialHeight == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(initialHeightPropertyKey,
                             &m_InitialHeight,
                             &value);
        m_InitialHeight = value;
        RIVE_EDITOR_CHANGED(initialHeightChanged());
        notifyPropertyChanged(initialHeightPropertyKey);
    }

    inline float width() const { return m_Width; }
    void width(float value)
    {
        if (m_Width == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(widthPropertyKey, &m_Width, &value);
        m_Width = value;
        RIVE_EDITOR_CHANGED(widthChanged());
        notifyPropertyChanged(widthPropertyKey);
    }

    inline float height() const { return m_Height; }
    void height(float value)
    {
        if (m_Height == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(heightPropertyKey, &m_Height, &value);
        m_Height = value;
        RIVE_EDITOR_CHANGED(heightChanged());
        notifyPropertyChanged(heightPropertyKey);
    }

    Core* clone() const override;
    void copy(const NSlicedNodeBase& object)
    {
        m_InitialWidth = object.m_InitialWidth;
        m_InitialHeight = object.m_InitialHeight;
        m_Width = object.m_Width;
        m_Height = object.m_Height;
        Node::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case initialWidthPropertyKey:
                m_InitialWidth = CoreDoubleType::deserialize(reader);
                return true;
            case initialHeightPropertyKey:
                m_InitialHeight = CoreDoubleType::deserialize(reader);
                return true;
            case widthPropertyKey:
                m_Width = CoreDoubleType::deserialize(reader);
                return true;
            case heightPropertyKey:
                m_Height = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Node::deserialize(propertyKey, reader);
    }

protected:
    virtual void initialWidthChanged() {}
    virtual void initialHeightChanged() {}
    virtual void widthChanged() {}
    virtual void heightChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/layout/n_sliced_node_ext.inl"
#endif
};
} // namespace rive

#endif
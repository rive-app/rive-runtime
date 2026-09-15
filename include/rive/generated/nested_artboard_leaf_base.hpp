#ifndef _RIVE_NESTED_ARTBOARD_LEAF_BASE_HPP_
#define _RIVE_NESTED_ARTBOARD_LEAF_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/nested_artboard.hpp"
namespace rive
{
class NestedArtboardLeafBase : public NestedArtboard
{
protected:
    typedef NestedArtboard Super;

public:
    static const uint16_t typeKey = 451;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedArtboardLeafBase::typeKey:
            case NestedArtboardBase::typeKey:
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

    static const uint16_t fitPropertyKey = 538;
    static const uint16_t fitToLayoutParentPropertyKey = 1098;
    static const uint16_t alignmentXPropertyKey = 644;
    static const uint16_t alignmentYPropertyKey = 645;

protected:
    uint8_t m_Fit = 0;
    bool m_FitToLayoutParent = false;
    float m_AlignmentX = 0.0f;
    float m_AlignmentY = 0.0f;

public:
    inline uint8_t fit() const { return m_Fit; }
    void fit(uint8_t value)
    {
        if (m_Fit == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(fitPropertyKey, &m_Fit, &value);
        m_Fit = value;
        RIVE_EDITOR_CHANGED(fitChanged());
        notifyPropertyChanged(fitPropertyKey);
    }

    inline bool fitToLayoutParent() const { return m_FitToLayoutParent; }
    void fitToLayoutParent(bool value)
    {
        if (m_FitToLayoutParent == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(fitToLayoutParentPropertyKey,
                             &m_FitToLayoutParent,
                             &value);
        m_FitToLayoutParent = value;
        RIVE_EDITOR_CHANGED(fitToLayoutParentChanged());
        notifyPropertyChanged(fitToLayoutParentPropertyKey);
    }

    inline float alignmentX() const { return m_AlignmentX; }
    void alignmentX(float value)
    {
        if (m_AlignmentX == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(alignmentXPropertyKey, &m_AlignmentX, &value);
        m_AlignmentX = value;
        RIVE_EDITOR_CHANGED(alignmentXChanged());
        notifyPropertyChanged(alignmentXPropertyKey);
    }

    inline float alignmentY() const { return m_AlignmentY; }
    void alignmentY(float value)
    {
        if (m_AlignmentY == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(alignmentYPropertyKey, &m_AlignmentY, &value);
        m_AlignmentY = value;
        RIVE_EDITOR_CHANGED(alignmentYChanged());
        notifyPropertyChanged(alignmentYPropertyKey);
    }

    Core* clone() const override;
    void copy(const NestedArtboardLeafBase& object)
    {
        m_Fit = object.m_Fit;
        m_FitToLayoutParent = object.m_FitToLayoutParent;
        m_AlignmentX = object.m_AlignmentX;
        m_AlignmentY = object.m_AlignmentY;
        NestedArtboard::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case fitPropertyKey:
                m_Fit = CoreUintType::deserialize(reader);
                return true;
            case fitToLayoutParentPropertyKey:
                m_FitToLayoutParent = CoreBoolType::deserialize(reader);
                return true;
            case alignmentXPropertyKey:
                m_AlignmentX = CoreDoubleType::deserialize(reader);
                return true;
            case alignmentYPropertyKey:
                m_AlignmentY = CoreDoubleType::deserialize(reader);
                return true;
        }
        return NestedArtboard::deserialize(propertyKey, reader);
    }

protected:
    virtual void fitChanged() {}
    virtual void fitToLayoutParentChanged() {}
    virtual void alignmentXChanged() {}
    virtual void alignmentYChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/nested_artboard_leaf_ext.inl"
#endif
};
} // namespace rive

#endif
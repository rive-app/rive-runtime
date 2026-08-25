#ifndef _RIVE_BONE_BASE_HPP_
#define _RIVE_BONE_BASE_HPP_
#include "rive/bones/skeletal_component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class BoneBase : public SkeletalComponent
{
protected:
    typedef SkeletalComponent Super;

public:
    static const uint16_t typeKey = 40;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case BoneBase::typeKey:
            case SkeletalComponentBase::typeKey:
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

    static const uint16_t lengthPropertyKey = 89;

protected:
    float m_Length = 0.0f;

public:
    inline float length() const { return m_Length; }
    void length(float value)
    {
        if (m_Length == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(lengthPropertyKey, &m_Length, &value);
        m_Length = value;
        RIVE_EDITOR_CHANGED(lengthChanged());
        notifyPropertyChanged(lengthPropertyKey);
    }

    Core* clone() const override;
    void copy(const BoneBase& object)
    {
        m_Length = object.m_Length;
        SkeletalComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case lengthPropertyKey:
                m_Length = CoreDoubleType::deserialize(reader);
                return true;
        }
        return SkeletalComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void lengthChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/bones/bone_ext.inl"
#endif
};
} // namespace rive

#endif
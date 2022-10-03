#ifndef _RIVE_ROOT_BONE_BASE_HPP_
#define _RIVE_ROOT_BONE_BASE_HPP_
#include "rive/bones/bone.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class RootBoneBase : public Bone
{
protected:
    typedef Bone Super;

public:
    static const uint16_t typeKey = 41;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case RootBoneBase::typeKey:
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

    static const uint16_t xPropertyKey = 90;
    static const uint16_t yPropertyKey = 91;

private:
    float m_X = 0.0f;
    float m_Y = 0.0f;

public:
    inline float x() const override { return m_X; }
    void x(float value)
    {
        if (m_X == value)
        {
            return;
        }
        m_X = value;
        xChanged();
    }

    inline float y() const override { return m_Y; }
    void y(float value)
    {
        if (m_Y == value)
        {
            return;
        }
        m_Y = value;
        yChanged();
    }

    Core* clone() const override;
    void copy(const RootBoneBase& object)
    {
        m_X = object.m_X;
        m_Y = object.m_Y;
        Bone::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case xPropertyKey:
                m_X = CoreDoubleType::deserialize(reader);
                return true;
            case yPropertyKey:
                m_Y = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Bone::deserialize(propertyKey, reader);
    }

protected:
    virtual void xChanged() {}
    virtual void yChanged() {}
};
} // namespace rive

#endif
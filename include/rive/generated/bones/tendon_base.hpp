#ifndef _RIVE_TENDON_BASE_HPP_
#define _RIVE_TENDON_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class TendonBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 44;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TendonBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t boneIdPropertyKey = 95;
    static const uint16_t xxPropertyKey = 96;
    static const uint16_t yxPropertyKey = 97;
    static const uint16_t xyPropertyKey = 98;
    static const uint16_t yyPropertyKey = 99;
    static const uint16_t txPropertyKey = 100;
    static const uint16_t tyPropertyKey = 101;

private:
    uint32_t m_BoneId = -1;
    float m_Xx = 1.0f;
    float m_Yx = 0.0f;
    float m_Xy = 0.0f;
    float m_Yy = 1.0f;
    float m_Tx = 0.0f;
    float m_Ty = 0.0f;

public:
    inline uint32_t boneId() const { return m_BoneId; }
    void boneId(uint32_t value)
    {
        if (m_BoneId == value)
        {
            return;
        }
        m_BoneId = value;
        boneIdChanged();
    }

    inline float xx() const { return m_Xx; }
    void xx(float value)
    {
        if (m_Xx == value)
        {
            return;
        }
        m_Xx = value;
        xxChanged();
    }

    inline float yx() const { return m_Yx; }
    void yx(float value)
    {
        if (m_Yx == value)
        {
            return;
        }
        m_Yx = value;
        yxChanged();
    }

    inline float xy() const { return m_Xy; }
    void xy(float value)
    {
        if (m_Xy == value)
        {
            return;
        }
        m_Xy = value;
        xyChanged();
    }

    inline float yy() const { return m_Yy; }
    void yy(float value)
    {
        if (m_Yy == value)
        {
            return;
        }
        m_Yy = value;
        yyChanged();
    }

    inline float tx() const { return m_Tx; }
    void tx(float value)
    {
        if (m_Tx == value)
        {
            return;
        }
        m_Tx = value;
        txChanged();
    }

    inline float ty() const { return m_Ty; }
    void ty(float value)
    {
        if (m_Ty == value)
        {
            return;
        }
        m_Ty = value;
        tyChanged();
    }

    Core* clone() const override;
    void copy(const TendonBase& object)
    {
        m_BoneId = object.m_BoneId;
        m_Xx = object.m_Xx;
        m_Yx = object.m_Yx;
        m_Xy = object.m_Xy;
        m_Yy = object.m_Yy;
        m_Tx = object.m_Tx;
        m_Ty = object.m_Ty;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case boneIdPropertyKey:
                m_BoneId = CoreUintType::deserialize(reader);
                return true;
            case xxPropertyKey:
                m_Xx = CoreDoubleType::deserialize(reader);
                return true;
            case yxPropertyKey:
                m_Yx = CoreDoubleType::deserialize(reader);
                return true;
            case xyPropertyKey:
                m_Xy = CoreDoubleType::deserialize(reader);
                return true;
            case yyPropertyKey:
                m_Yy = CoreDoubleType::deserialize(reader);
                return true;
            case txPropertyKey:
                m_Tx = CoreDoubleType::deserialize(reader);
                return true;
            case tyPropertyKey:
                m_Ty = CoreDoubleType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void boneIdChanged() {}
    virtual void xxChanged() {}
    virtual void yxChanged() {}
    virtual void xyChanged() {}
    virtual void yyChanged() {}
    virtual void txChanged() {}
    virtual void tyChanged() {}
};
} // namespace rive

#endif
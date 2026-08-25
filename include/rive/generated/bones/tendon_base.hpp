#ifndef _RIVE_TENDON_BASE_HPP_
#define _RIVE_TENDON_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_id_type.hpp"
#include "rive/core/id.hpp"
namespace rive
{
class TendonBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 44;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
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

protected:
    Id m_BoneId = kEmptyId;
    float m_Xx = 1.0f;
    float m_Yx = 0.0f;
    float m_Xy = 0.0f;
    float m_Yy = 1.0f;
    float m_Tx = 0.0f;
    float m_Ty = 0.0f;

public:
    inline Id boneId() const { return m_BoneId; }
    void boneId(Id value)
    {
        if (m_BoneId == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(boneIdPropertyKey, &m_BoneId, &value);
        m_BoneId = value;
        RIVE_EDITOR_CHANGED(boneIdChanged());
        notifyPropertyChanged(boneIdPropertyKey);
    }

    inline float xx() const { return m_Xx; }
    void xx(float value)
    {
        if (m_Xx == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(xxPropertyKey, &m_Xx, &value);
        m_Xx = value;
        RIVE_EDITOR_CHANGED(xxChanged());
        notifyPropertyChanged(xxPropertyKey);
    }

    inline float yx() const { return m_Yx; }
    void yx(float value)
    {
        if (m_Yx == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(yxPropertyKey, &m_Yx, &value);
        m_Yx = value;
        RIVE_EDITOR_CHANGED(yxChanged());
        notifyPropertyChanged(yxPropertyKey);
    }

    inline float xy() const { return m_Xy; }
    void xy(float value)
    {
        if (m_Xy == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(xyPropertyKey, &m_Xy, &value);
        m_Xy = value;
        RIVE_EDITOR_CHANGED(xyChanged());
        notifyPropertyChanged(xyPropertyKey);
    }

    inline float yy() const { return m_Yy; }
    void yy(float value)
    {
        if (m_Yy == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(yyPropertyKey, &m_Yy, &value);
        m_Yy = value;
        RIVE_EDITOR_CHANGED(yyChanged());
        notifyPropertyChanged(yyPropertyKey);
    }

    inline float tx() const { return m_Tx; }
    void tx(float value)
    {
        if (m_Tx == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(txPropertyKey, &m_Tx, &value);
        m_Tx = value;
        RIVE_EDITOR_CHANGED(txChanged());
        notifyPropertyChanged(txPropertyKey);
    }

    inline float ty() const { return m_Ty; }
    void ty(float value)
    {
        if (m_Ty == value)
        {
            return;
        }
        RIVE_EDITOR_CHANGING(tyPropertyKey, &m_Ty, &value);
        m_Ty = value;
        RIVE_EDITOR_CHANGED(tyChanged());
        notifyPropertyChanged(tyPropertyKey);
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
                m_BoneId = CoreIdType::runtimeDeserialize(reader);
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
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/bones/tendon_ext.inl"
#endif
};
} // namespace rive

#endif
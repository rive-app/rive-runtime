#ifndef _RIVE_KEY_FRAME_BASE_HPP_
#define _RIVE_KEY_FRAME_BASE_HPP_
#include "rive/core.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class KeyFrameBase : public Core
{
protected:
    typedef Core Super;

public:
    static const uint16_t typeKey = 29;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case KeyFrameBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t framePropertyKey = 67;

private:
    uint32_t m_Frame = 0;

public:
    inline uint32_t frame() const { return m_Frame; }
    void frame(uint32_t value)
    {
        if (m_Frame == value)
        {
            return;
        }
        m_Frame = value;
        frameChanged();
    }

    void copy(const KeyFrameBase& object) { m_Frame = object.m_Frame; }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case framePropertyKey:
                m_Frame = CoreUintType::deserialize(reader);
                return true;
        }
        return false;
    }

protected:
    virtual void frameChanged() {}
};
} // namespace rive

#endif
#ifndef _RIVE_NESTED_ARTBOARD_BASE_HPP_
#define _RIVE_NESTED_ARTBOARD_BASE_HPP_
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/drawable.hpp"
#include "rive/span.hpp"
namespace rive
{
class NestedArtboardBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 92;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
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

    static const uint16_t artboardIdPropertyKey = 197;
    static const uint16_t dataBindPathIdsPropertyKey = 582;

protected:
    uint32_t m_ArtboardId = -1;

public:
    inline uint32_t artboardId() const { return m_ArtboardId; }
    void artboardId(uint32_t value)
    {
        if (m_ArtboardId == value)
        {
            return;
        }
        m_ArtboardId = value;
        artboardIdChanged();
    }

    virtual void decodeDataBindPathIds(Span<const uint8_t> value) = 0;
    virtual void copyDataBindPathIds(const NestedArtboardBase& object) = 0;

    Core* clone() const override;
    void copy(const NestedArtboardBase& object)
    {
        m_ArtboardId = object.m_ArtboardId;
        copyDataBindPathIds(object);
        Drawable::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case artboardIdPropertyKey:
                m_ArtboardId = CoreUintType::deserialize(reader);
                return true;
            case dataBindPathIdsPropertyKey:
                decodeDataBindPathIds(CoreBytesType::deserialize(reader));
                return true;
        }
        return Drawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void artboardIdChanged() {}
    virtual void dataBindPathIdsChanged() {}
};
} // namespace rive

#endif
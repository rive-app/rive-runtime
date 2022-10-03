#ifndef _RIVE_IMAGE_BASE_HPP_
#define _RIVE_IMAGE_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/drawable.hpp"
namespace rive
{
class ImageBase : public Drawable
{
protected:
    typedef Drawable Super;

public:
    static const uint16_t typeKey = 100;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ImageBase::typeKey:
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

    static const uint16_t assetIdPropertyKey = 206;

private:
    uint32_t m_AssetId = -1;

public:
    inline uint32_t assetId() const { return m_AssetId; }
    void assetId(uint32_t value)
    {
        if (m_AssetId == value)
        {
            return;
        }
        m_AssetId = value;
        assetIdChanged();
    }

    Core* clone() const override;
    void copy(const ImageBase& object)
    {
        m_AssetId = object.m_AssetId;
        Drawable::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case assetIdPropertyKey:
                m_AssetId = CoreUintType::deserialize(reader);
                return true;
        }
        return Drawable::deserialize(propertyKey, reader);
    }

protected:
    virtual void assetIdChanged() {}
};
} // namespace rive

#endif
#ifndef _RIVE_MESH_BASE_HPP_
#define _RIVE_MESH_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_bytes_type.hpp"
#include "rive/span.hpp"
namespace rive
{
class MeshBase : public ContainerComponent
{
protected:
    typedef ContainerComponent Super;

public:
    static const uint16_t typeKey = 109;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case MeshBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t triangleIndexBytesPropertyKey = 223;

public:
    virtual void decodeTriangleIndexBytes(Span<const uint8_t> value) = 0;
    virtual void copyTriangleIndexBytes(const MeshBase& object) = 0;

    Core* clone() const override;
    void copy(const MeshBase& object)
    {
        copyTriangleIndexBytes(object);
        ContainerComponent::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case triangleIndexBytesPropertyKey:
                decodeTriangleIndexBytes(CoreBytesType::deserialize(reader));
                return true;
        }
        return ContainerComponent::deserialize(propertyKey, reader);
    }

protected:
    virtual void triangleIndexBytesChanged() {}
};
} // namespace rive

#endif
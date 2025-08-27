#ifndef _RIVE_LIST_PATH_BASE_HPP_
#define _RIVE_LIST_PATH_BASE_HPP_
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/shapes/points_common_path.hpp"
namespace rive
{
class ListPathBase : public PointsCommonPath
{
protected:
    typedef PointsCommonPath Super;

public:
    static const uint16_t typeKey = 619;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ListPathBase::typeKey:
            case PointsCommonPathBase::typeKey:
            case PathBase::typeKey:
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

    static const uint16_t listSourcePropertyKey = 874;

protected:
    uint32_t m_ListSource = -1;

public:
    inline uint32_t listSource() const { return m_ListSource; }
    void listSource(uint32_t value)
    {
        if (m_ListSource == value)
        {
            return;
        }
        m_ListSource = value;
        listSourceChanged();
    }

    Core* clone() const override;
    void copy(const ListPathBase& object)
    {
        m_ListSource = object.m_ListSource;
        PointsCommonPath::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case listSourcePropertyKey:
                m_ListSource = CoreUintType::deserialize(reader);
                return true;
        }
        return PointsCommonPath::deserialize(propertyKey, reader);
    }

protected:
    virtual void listSourceChanged() {}
};
} // namespace rive

#endif
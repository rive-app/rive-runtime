#ifndef _RIVE_POINTS_PATH_BASE_HPP_
#define _RIVE_POINTS_PATH_BASE_HPP_
#include "rive/shapes/points_common_path.hpp"
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/editor_field_types.hpp"
#endif
namespace rive
{
class PointsPathBase : public PointsCommonPath
{
protected:
    typedef PointsCommonPath Super;

public:
    static const uint16_t typeKey = 16;

    uint16_t coreType() const override { return typeKey; }

public:
    Core* clone() const override;
    void copy(const PointsPathBase& object)
    {
        RIVE_EDITOR_COPY(object);
        PointsCommonPath::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        RIVE_EDITOR_DESERIALIZE(propertyKey, reader);
        return PointsCommonPath::deserialize(propertyKey, reader);
    }
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/shapes/points_path_ext.inl"
#endif
};
} // namespace rive

#endif
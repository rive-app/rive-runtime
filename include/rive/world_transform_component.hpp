#ifndef _RIVE_WORLD_TRANSFORM_COMPONENT_HPP_
#define _RIVE_WORLD_TRANSFORM_COMPONENT_HPP_
#include "rive/generated/world_transform_component_base.hpp"
#include "rive/math/mat2d.hpp"

namespace rive
{
class TransformComponent;
class WorldTransformComponent : public WorldTransformComponentBase
{
    friend class TransformComponent;

protected:
    Mat2D m_WorldTransform;

public:
    void markWorldTransformDirty();
    virtual float childOpacity();
    Mat2D& mutableWorldTransform();
    const Mat2D& worldTransform() const;
    Vec2D worldTranslation() const { return m_WorldTransform.translation(); }
    void opacityChanged() override;
};
} // namespace rive

#endif
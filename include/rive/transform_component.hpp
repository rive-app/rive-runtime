#ifndef _RIVE_TRANSFORM_COMPONENT_HPP_
#define _RIVE_TRANSFORM_COMPONENT_HPP_
#include "rive/generated/transform_component_base.hpp"
#include "rive/math/mat2d.hpp"

namespace rive
{
class Constraint;
class WorldTransformComponent;
class AABB;
class TransformComponent : public TransformComponentBase
{
private:
    Mat2D m_Transform;
    float m_RenderOpacity = 0.0f;
    WorldTransformComponent* m_ParentTransformComponent = nullptr;
    std::vector<Constraint*> m_Constraints;

public:
    bool collapse(bool value) override;
    const std::vector<Constraint*>& constraints() const { return m_Constraints; }
    StatusCode onAddedClean(CoreContext* context) override;
    void buildDependencies() override;
    void update(ComponentDirt value) override;
    void updateTransform();
    void updateWorldTransform();
    void markTransformDirty();

    /// Opacity inherited by any child of this transform component. This'll
    /// later get overridden by effect layers.
    float childOpacity() override { return m_RenderOpacity; }
    float renderOpacity() const { return m_RenderOpacity; }

    const Mat2D& transform() const;

    /// Explicitly dangerous. Use transform/worldTransform when you don't
    /// need to transform things outside of their hierarchy.
    Mat2D& mutableTransform();

    virtual float x() const = 0;
    virtual float y() const = 0;

    void rotationChanged() override;
    void scaleXChanged() override;
    void scaleYChanged() override;

    void addConstraint(Constraint* constraint);
    virtual AABB localBounds() const;
    void markDirtyIfConstrained();
};
} // namespace rive

#endif
#ifndef _RIVE_CONSTRAINT_HPP_
#define _RIVE_CONSTRAINT_HPP_
#include "rive/generated/constraints/constraint_base.hpp"
#include <stdio.h>
namespace rive
{
class TransformComponent;
class Mat2D;
class TransformComponents;

class Constraint : public ConstraintBase
{
public:
    void strengthChanged() override;
    StatusCode onAddedDirty(CoreContext* context) override;
    virtual void markConstraintDirty();
    virtual void constrain(TransformComponent* component) = 0;
    void buildDependencies() override;
    void onDirty(ComponentDirt dirt) override;

protected:
    /// Writes composed into the component's world transform, keeping its
    /// anchor where it already was. A layout's box starts at its local zero,
    /// so recomposing about that zero would swing the box about its corner
    /// rather than its origin.
    static void composeKeepingAnchor(TransformComponent* component,
                                     const TransformComponents& composed);
    /// Steps the component's world translation back by its anchor, so a
    /// position already written as "where local zero goes" instead lands the
    /// anchor there. Scaled by strength so zero strength stays a no-op.
    static void landAnchor(TransformComponent* component, float strength);
    /// Composes, then lands the anchor — for constraints that copy a position.
    static void composeLandingAnchor(TransformComponent* component,
                                     const TransformComponents& composed,
                                     float strength);
    /// The component's world transform displaced by [offset] applied in its
    /// parent's frame — for constraints that move a component by a delta
    /// measured in the space the layout was solved in.
    static Mat2D offsetInParentFrame(TransformComponent* component,
                                     const Mat2D& offset);
};

const Mat2D& getParentWorld(const TransformComponent& component);
} // namespace rive

#endif
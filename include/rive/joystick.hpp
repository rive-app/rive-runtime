#ifndef _RIVE_JOYSTICK_HPP_
#define _RIVE_JOYSTICK_HPP_
#include "rive/generated/joystick_base.hpp"
#include "rive/intrinsically_sizeable.hpp"
#include "rive/joystick_flags.hpp"
#include "rive/animation/nested_remap_animation.hpp"
#include "rive/math/mat2d.hpp"
#include <stdio.h>
#include <memory>

namespace rive
{
class Artboard;
class LinearAnimation;
class LinearAnimationInstance;
class TransformComponent;
class Joystick : public JoystickBase, public IntrinsicallySizeable
{
public:
    Joystick();
    ~Joystick() override;
    void update(ComponentDirt value) override;
    void apply(Artboard* artboard) const;
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    void xChanged() override;
    void yChanged() override;
    void posXChanged() override;
    void posYChanged() override;
    void widthChanged() override;
    void heightChanged() override;

    bool isJoystickFlagged(JoystickFlags flag) const
    {
        return enums::any_flag_set(JoystickFlags(joystickFlags()), flag);
    }

    bool canApplyBeforeUpdate() const { return m_handleSource == nullptr; }

    void addDependents(Artboard* artboard);

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size,
                     LayoutScaleType widthScaleType,
                     LayoutScaleType heightScaleType,
                     LayoutDirection direction) override;
    bool shouldPropagateSizeToChildren() override { return false; }

protected:
    void buildDependencies() override;

private:
    Mat2D m_worldTransform;
    Mat2D m_inverseWorldTransform;
    LinearAnimation* m_xAnimation = nullptr;
    LinearAnimation* m_yAnimation = nullptr;
    // Per-instance playback contexts for the x/y animations. Joysticks apply
    // the shared LinearAnimation directly (not via a playing scene), so these
    // carry the LinearAnimationInstance context needed to resolve data-bound
    // keyframe values (see LinearAnimationInstance::keyFrameValueHolder). Only
    // created on artboard instances; null on the source artboard.
    std::unique_ptr<LinearAnimationInstance> m_xAnimationInstance;
    std::unique_ptr<LinearAnimationInstance> m_yAnimationInstance;
    TransformComponent* m_handleSource = nullptr;
    std::vector<NestedRemapAnimation*> m_dependents;

    void addAnimationDependents(Artboard* artboard, LinearAnimation* animation);
};
} // namespace rive

#endif

#ifndef _RIVE_JOYSTICK_HPP_
#define _RIVE_JOYSTICK_HPP_
#include "rive/generated/joystick_base.hpp"
#include "rive/intrinsically_sizeable.hpp"
#include "rive/joystick_flags.hpp"
#include "rive/math/mat2d.hpp"
#include <stdio.h>

namespace rive
{
class Artboard;
class LinearAnimation;
class TransformComponent;
class Joystick : public JoystickBase, public IntrinsicallySizeable
{
public:
    void update(ComponentDirt value) override;
    void apply(Artboard* artboard) const;
    StatusCode onAddedClean(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;

    bool isJoystickFlagged(JoystickFlags flag) const
    {
        return ((JoystickFlags)joystickFlags()) & flag;
    }

    bool canApplyBeforeUpdate() const { return m_handleSource == nullptr; }

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size) override;

protected:
    void buildDependencies() override;

private:
    Mat2D m_worldTransform;
    Mat2D m_inverseWorldTransform;
    LinearAnimation* m_xAnimation = nullptr;
    LinearAnimation* m_yAnimation = nullptr;
    TransformComponent* m_handleSource = nullptr;
};
} // namespace rive

#endif

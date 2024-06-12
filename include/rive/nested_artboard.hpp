#ifndef _RIVE_NESTED_ARTBOARD_HPP_
#define _RIVE_NESTED_ARTBOARD_HPP_

#include "rive/generated/nested_artboard_base.hpp"
#include "rive/hit_info.hpp"
#include "rive/span.hpp"
#include <stdio.h>

namespace rive
{

enum class NestedArtboardFitType : uint8_t
{
    fill, // Default value - scales to fill available view without maintaining aspect ratio
    contain,
    cover,
    fitWidth,
    fitHeight,
    resizeArtboard,
    none,
};

enum class NestedArtboardAlignmentType : uint8_t
{
    center, // Default value
    topLeft,
    topCenter,
    topRight,
    centerLeft,
    centerRight,
    bottomLeft,
    bottomCenter,
    bottomRight,
};

class ArtboardInstance;
class NestedAnimation;
class NestedInput;
class NestedStateMachine;
class StateMachineInstance;
class NestedArtboard : public NestedArtboardBase
{

private:
    Artboard* m_Artboard = nullptr;               // might point to m_Instance, and might not
    std::unique_ptr<ArtboardInstance> m_Instance; // may be null
    std::vector<NestedAnimation*> m_NestedAnimations;
    float m_layoutScaleX = NAN;
    float m_layoutScaleY = NAN;

public:
    NestedArtboard();
    ~NestedArtboard() override;
    StatusCode onAddedClean(CoreContext* context) override;
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    void addNestedAnimation(NestedAnimation* nestedAnimation);

    void nest(Artboard* artboard);

    ArtboardInstance* artboard() { return m_Instance.get(); }

    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    bool advance(float elapsedSeconds);
    void update(ComponentDirt value) override;

    bool hasNestedStateMachines() const;
    Span<NestedAnimation*> nestedAnimations();
    NestedArtboard* nestedArtboard(std::string name) const;
    NestedStateMachine* stateMachine(std::string name) const;
    NestedInput* input(std::string name) const;
    NestedInput* input(std::string name, std::string stateMachineName) const;

    NestedArtboardAlignmentType alignmentType() const
    {
        return (NestedArtboardAlignmentType)alignment();
    }
    NestedArtboardFitType fitType() const { return (NestedArtboardFitType)fit(); }
    float effectiveScaleX() { return std::isnan(m_layoutScaleX) ? scaleX() : m_layoutScaleX; }
    float effectiveScaleY() { return std::isnan(m_layoutScaleY) ? scaleY() : m_layoutScaleY; }

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size) override;

    /// Convert a world space (relative to the artboard that this
    /// NestedArtboard is a child of) to the local space of the Artboard
    /// nested within. Returns true when the conversion succeeds, and false
    /// when one is not possible.
    bool worldToLocal(Vec2D world, Vec2D* local);
};
} // namespace rive

#endif

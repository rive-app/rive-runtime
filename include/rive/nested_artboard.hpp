#ifndef _RIVE_NESTED_ARTBOARD_HPP_
#define _RIVE_NESTED_ARTBOARD_HPP_

#include "rive/generated/nested_artboard_base.hpp"
#include "rive/hit_info.hpp"
#include "rive/span.hpp"
#include <stdio.h>

namespace rive
{
class ArtboardInstance;
class NestedAnimation;
class NestedArtboard : public NestedArtboardBase
{

private:
    Artboard* m_Artboard = nullptr;               // might point to m_Instance, and might not
    std::unique_ptr<ArtboardInstance> m_Instance; // may be null
    std::vector<NestedAnimation*> m_NestedAnimations;

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

    /// Convert a world space (relative to the artboard that this
    /// NestedArtboard is a child of) to the local space of the Artboard
    /// nested within. Returns true when the conversion succeeds, and false
    /// when one is not possible.
    bool worldToLocal(Vec2D world, Vec2D* local);
};
} // namespace rive

#endif

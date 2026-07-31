#ifndef _RIVE_LAYOUT_PARTICIPANT_HPP_
#define _RIVE_LAYOUT_PARTICIPANT_HPP_
#include "rive/generated/layout/layout_participant_base.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/layout/layout_data.hpp"
#include "rive/layout_component.hpp"
#include "rive/advancing_component.hpp"

namespace rive
{
class TransformComponent;
class LayoutComponent;
class LayoutStyleApplier;
class KeyFrameInterpolator;
// Lazily-allocated animation state (defined in the .cpp); only exists while a
// participant is under an animated layout.
struct ParticipantAnimation;

/// A layout participant: a LayoutNodeStyle (sizing) that also owns the layout
/// node and provides it to the parent layout on behalf of its host node. Its
/// presence on a node signals "this node participates in layout."
///
/// Ported from LayoutNodeParticipant; unlike that mixin this lives as a child
/// object, so geometry/transform resolve to the host (its parent). The host
/// composes the resolved slot into its own world transform.
class LayoutParticipant : public LayoutParticipantBase,
                          public LayoutNodeProvider,
                          public AdvancingComponent
{
public:
    ~LayoutParticipant() override;

    // The host node this participant belongs to (our parent). The
    // LayoutNodeProvider machinery uses this as the "node" whose world
    // transform the slot composes into, and as the IntrinsicallySizeable to
    // measure/size.
    TransformComponent* transformComponent() override;

    StatusCode onAddedClean(CoreContext* context) override;

#ifdef WITH_RIVE_LAYOUT
    void* layoutNode(int index) override;
    // Re-establish participation: (de)allocate the node or, on a forwarding
    // host (Solo), re-collect the forwarded child node. Called on load and when
    // a Solo swaps its active child.
    void resync();
#endif
    size_t numLayoutNodes() override;
    AABB layoutBounds() override;
    AABB layoutBoundsForNode(int index) override;
    bool syncStyleChanges() override;
    void updateLayoutBounds(bool animate = true) override;
#ifdef WITH_RIVE_LAYOUT
    void addLayoutStyleApplier(LayoutStyleApplier* applier) override;
    void applyBaseStyle(YGStyle& style,
                        const LayoutSyncContext& context) override;
#endif
    void markLayoutNodeDirty(
        bool shouldForceUpdateLayoutBounds = false) override;

    // Layout animation: advanced each frame (as an AdvancingComponent) to
    // interpolate the resolved slot toward the newly solved layout, using the
    // interpolation inherited from the parent layout via cascadeLayoutStyle.
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
#ifdef WITH_RIVE_LAYOUT
    bool cascadeLayoutStyle(LayoutStyleInterpolation inheritedInterpolation,
                            KeyFrameInterpolator* inheritedInterpolator,
                            float inheritedInterpolationTime,
                            LayoutDirection direction) override;
#endif

    bool isParticipatingInLayout() const;

    // The resolved slot (in the parent layout's space), read live from the
    // YGNode. The host reads these to place its scaled geometry at the slot.
    float resolvedLeft() const;
    float resolvedTop() const;
    float resolvedWidth() const;
    float resolvedHeight() const;

protected:
    // Any sizing change funnels here to re-sync the node.
    void onSizingChanged();
    void layoutWidthScaleTypeChanged() override { onSizingChanged(); }
    void layoutHeightScaleTypeChanged() override { onSizingChanged(); }
    void widthChanged() override { onSizingChanged(); }
    void heightChanged() override { onSizingChanged(); }
    void fractionalWidthChanged() override { onSizingChanged(); }
    void fractionalHeightChanged() override { onSizingChanged(); }
    void minWidthChanged() override { onSizingChanged(); }
    void maxWidthChanged() override { onSizingChanged(); }
    void minHeightChanged() override { onSizingChanged(); }
    void maxHeightChanged() override { onSizingChanged(); }
    void minWidthUnitsValueChanged() override { onSizingChanged(); }
    void maxWidthUnitsValueChanged() override { onSizingChanged(); }
    void minHeightUnitsValueChanged() override { onSizingChanged(); }
    void maxHeightUnitsValueChanged() override { onSizingChanged(); }
    void widthUnitsValueChanged() override { onSizingChanged(); }
    void heightUnitsValueChanged() override { onSizingChanged(); }
    void justifySelfValueChanged() override { onSizingChanged(); }
    void displayValueChanged() override { onSizingChanged(); }

private:
    // The nearest ancestor LayoutComponent, walking up through the host.
    LayoutComponent* owningLayout();

public:
    // Host fit state, kept here rather than on the host so a non-participating
    // Shape carries none of it — only participants read or write these, and the
    // participant is already allocated for exactly that case.
    //
    // Scale that fits the host's geometry to its slot, composed innermost in
    // the host's world transform.
    float hostScaleX() const { return m_hostScaleX; }
    float hostScaleY() const { return m_hostScaleY; }
    void hostScale(float x, float y)
    {
        m_hostScaleX = x;
        m_hostScaleY = y;
    }
    // Memoized host intrinsic bounds. Recomputing walks and re-tessellates
    // every path, and the host reads it on each world transform update.
    bool hostBoundsValid() const { return m_hostBoundsValid; }
    const AABB& hostBounds() const { return m_hostBounds; }
    void hostBounds(const AABB& bounds, bool cache)
    {
        m_hostBounds = bounds;
        m_hostBoundsValid = cache;
    }
    void invalidateHostBounds() { m_hostBoundsValid = false; }

private:
    // Data members are grouped here, widest first, so the two flags share one
    // tail hole instead of each opening its own.
    //
    // ── Layout animation. A participant has no animation style of its own; it
    // inherits the parent layout's via cascadeLayoutStyle. All animation state
    // (buffers, interpolation, the animated slot) is lazily heap-allocated only
    // while under an animated layout — so a non-animating participant pays just
    // the pointer.
    ParticipantAnimation* m_animation = nullptr;
    AABB m_hostBounds;
    float m_hostScaleX = 1.0f;
    float m_hostScaleY = 1.0f;
    // Gates the first-solve snap (animate only once we've solved before).
    bool m_hasSolvedLayout = false;
    bool m_hostBoundsValid = false;

    // The resolved slot straight from the yoga node (no animation).
    // resolvedLeft etc. use this when not animating; the animated slot lives in
    // m_animation.
    Layout solvedLayout() const;
    LayoutAnimationData* currentAnimationData();
    bool animates() const;
    LayoutStyleInterpolation interpolation() const;
    float interpolationTime() const;
    KeyFrameInterpolator* interpolator() const;

#ifdef WITH_RIVE_LAYOUT
    bool applyInterpolation(float elapsedSeconds, bool animate);

    LayoutData* m_layoutData = nullptr;

    static float definedOrZero(float v)
    {
        return YGFloatIsUndefined(v) ? 0.0f : v;
    }
    void applyResolvedLayoutSize();
    void releaseLayoutData();
    // Whether our host forwards a child's node (Solo) rather than owning one.
    // On a forwarding host we own no node and delegate the provider methods to
#endif
};
} // namespace rive

#endif

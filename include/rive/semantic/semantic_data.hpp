#ifndef _RIVE_SEMANTIC_DATA_HPP_
#define _RIVE_SEMANTIC_DATA_HPP_

#include "rive/component_dirt.hpp"
#include "rive/generated/semantic/semantic_data_base.hpp"
#include "rive/semantic/semantic_node.hpp"
#include "rive/refcnt.hpp"

#include <vector>

namespace rive
{
class SemanticListener;
class SemanticManager;

class SemanticData : public SemanticDataBase
{
public:
    ~SemanticData() override;

    rcp<SemanticNode> semanticNode();
    bool hasSemanticNode() const { return m_semanticNode != nullptr; }

    /// Set or clear the Focused state flag on the underlying SemanticNode.
    /// This is runtime-driven (not authored), so it modifies the node directly.
    void setFocusedState(bool focused);

    /// Request focus on a sibling FocusData (if any) via the FocusManager.
    /// Returns true if focus was successfully set.
    bool requestFocus();

    /// Register/unregister a SemanticListener for semantic action callbacks.
    void addSemanticListener(SemanticListener* listener);
    void removeSemanticListener(SemanticListener* listener);

    /// Fire semantic actions, notifying all registered listeners.
    void fireSemanticTap();
    void fireSemanticIncrease();
    void fireSemanticDecrease();

    /// Find the parent SemanticData by walking up the component hierarchy.
    SemanticData* findParentSemanticData() const;

    /// Find the closest SemanticNode for a component by walking up the
    /// hierarchy. Crosses artboard boundaries via externalParentSemanticNode.
    static rcp<SemanticNode> findClosestSemanticNode(Component* component);

    // Component overrides for update cycle
    bool collapse(bool value) override;
    void buildDependencies() override;
    void update(ComponentDirt value) override;
    void syncSemanticTreeVisibility();

protected:
    void roleChanged() override;
    void labelChanged() override;
    void stateFlagsChanged() override;
    void traitFlagsChanged() override;
    void valueChanged() override;
    void hintChanged() override;
    void headingLevelChanged() override;

    uint32_t semanticId() const;

private:
    bool shouldExcludeFromSemanticTree() const;
    rcp<SemanticNode> resolveParentNode() const;
    void updateWorldBounds();
    void markContentDirty();
    void applyInferredSemanticsIfNeeded();
    rcp<SemanticNode> m_semanticNode;
    SemanticManager* m_semanticManager = nullptr;
    std::vector<SemanticListener*> m_semanticListeners;
    bool m_boundsRetryPending = false;
    bool m_excludedFromTree = false;
};
} // namespace rive

#endif

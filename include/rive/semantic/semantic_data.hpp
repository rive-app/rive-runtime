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

  /// The node a hosted artboard's boundary hangs under: the closest
  /// SemanticData at or above the component in its own artboard (not one on
  /// the artboard itself), else that artboard's boundary node (null for a
  /// root artboard). It stops at the artboard so that hiding a host hides
  /// every artboard nested below it.
  static rcp<SemanticNode> findClosestSemanticNode(Component* component);

  /// True when this node is left out of the tree the platform sees: its
  /// component is collapsed, it has the Hidden state, or its owner was a
  /// hidden drawable when last checked. Read on every flatten; the tree
  /// never changes shape.
  bool isExcludedFromSemanticTree() const;

  // Component overrides for update cycle
  bool collapse(bool value) override;
  void buildDependencies() override;
  void update(ComponentDirt value) override;

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
  void syncSemanticTreeVisibility();
  void updateWorldBounds();
  void markContentDirty();
  void applyInferredSemanticsIfNeeded();
  rcp<SemanticNode> m_semanticNode;
  std::vector<SemanticListener*> m_semanticListeners;
  bool m_boundsRetryPending = false;
  // Whether the owning drawable was flagged Hidden when last read; see
  // isDrawableHidden in semantic_data.cpp.
  bool m_ownerHidden = false;
};
} // namespace rive

#endif

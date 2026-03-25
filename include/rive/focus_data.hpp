#ifndef _RIVE_FOCUS_DATA_HPP_
#define _RIVE_FOCUS_DATA_HPP_
#include "rive/component_dirt.hpp"
#include "rive/generated/focus_data_base.hpp"
#include "rive/input/focus_node.hpp"
#include "rive/input/keyboard_listener.hpp"
#include "rive/input/focusable.hpp"
#include "rive/math/aabb.hpp"
#include "rive/refcnt.hpp"
#include <vector>

namespace rive
{

class FocusListener;

class FocusData : public FocusDataBase, public Focusable
{
public:
    ~FocusData() override;

    rcp<FocusNode> focusNode();

    /// Register a listener to be notified of focus/blur events.
    void addFocusListener(FocusListener* listener);

    /// Unregister a listener.
    void removeFocusListener(FocusListener* listener);

    /// Register a listener to be notified of key input events.
    void addKeyboardListener(KeyboardListener* listener);

    /// Unregister a keyboard listener.
    void removeKeyboardListener(KeyboardListener* listener);

    /// Register a listener to be notified of text input events.
    void addTextInputListener(KeyboardListener* listener);

    /// Unregister a text input listener.
    void removeTextInputListener(KeyboardListener* listener);

    /// Programmatically focus this node.
    void focus();

    /// Find the parent FocusData by walking up the component hierarchy.
    /// Returns nullptr if no parent FocusData exists.
    FocusData* findParentFocusData() const;

    /// Find the closest FocusNode for a component by walking up the hierarchy.
    /// Checks parent nodes for FocusData children, and crosses artboard
    /// boundaries using externalParentFocusNode.
    /// @param component The component to start searching from
    /// @return The closest FocusNode, or nullptr if none found
    static rcp<FocusNode> findClosestFocusNode(Component* component);

    /// True if this focus target should participate in tab/spatial navigation
    /// and pointer focus (not under a collapsed/hidden/opacity-zero branch,
    /// including nested artboard hosts). Non-FocusData focusables are treated
    /// as visible by FocusManager.
    bool isEligibleForFocusTraversal() const override;

    // Focusable interface implementation
    bool keyInput(Key value,
                  KeyModifiers modifiers,
                  bool isPressed,
                  bool isRepeat) override;
    bool textInput(const std::string& text) override;
    void focused() override;
    void blurred() override;
    bool worldPosition(Vec2D& outPosition) override;
    Artboard* focusableArtboard() const override { return artboard(); }

    // Component overrides for update cycle
    void buildDependencies() override;
    void update(ComponentDirt value) override;

protected:
    void canFocusChanged() override;
    void canTouchChanged() override;
    void canTraverseChanged() override;
    void edgeBehaviorValueChanged() override;
    void nameChanged() override;

private:
    void updateWorldBounds();
    void scrollIntoView();
    void scrollConstraintToShowBounds(class ScrollConstraint* constraint,
                                      const AABB& elementBounds);
    rcp<FocusNode> m_focusNode;
    std::vector<FocusListener*> m_focusListeners;
    std::vector<KeyboardListener*> m_keyboardListeners;
    std::vector<KeyboardListener*> m_textInputListeners;
};
} // namespace rive

#endif

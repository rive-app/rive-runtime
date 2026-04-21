#ifndef _RIVE_SEMANTIC_LISTENER_GROUP_HPP_
#define _RIVE_SEMANTIC_LISTENER_GROUP_HPP_

#include "rive/semantic/semantic_listener.hpp"
#include "rive/listener_type.hpp"

#include <cstdint>

namespace rive
{
class SemanticData;
class StateMachineListener;
class StateMachineInstance;

enum class SemanticActionType : uint8_t
{
    tap = 0,
    increase = 1,
    decrease = 2,
};

/// A SemanticListenerGroup manages semantic action listeners attached
/// to a SemanticData. When semantic actions (tap, increase, decrease) are
/// fired, the listener's actions are queued for deferred execution.
class SemanticListenerGroup : public SemanticListener
{
public:
    SemanticListenerGroup(SemanticData* semanticData,
                          const StateMachineListener* listener,
                          StateMachineInstance* stateMachineInstance);
    ~SemanticListenerGroup() override;

    /// Get the listener this group is managing.
    const StateMachineListener* listener() const { return m_listener; }

    /// Get the SemanticData this group is attached to.
    SemanticData* semanticData() const { return m_semanticData; }

    /// Check which listener types are active.
    bool isTapListener() const { return m_isTapListener; }
    bool isIncreaseListener() const { return m_isIncreaseListener; }
    bool isDecreaseListener() const { return m_isDecreaseListener; }

    // SemanticListener interface
    void onSemanticTap() override;
    void onSemanticIncrease() override;
    void onSemanticDecrease() override;

private:
    SemanticData* m_semanticData;
    const StateMachineListener* m_listener;
    StateMachineInstance* m_stateMachineInstance;
    bool m_isTapListener;
    bool m_isIncreaseListener;
    bool m_isDecreaseListener;
};

} // namespace rive

#endif

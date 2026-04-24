#ifndef _RIVE_LISTENER_INPUT_TYPE_SEMANTIC_HPP_
#define _RIVE_LISTENER_INPUT_TYPE_SEMANTIC_HPP_

#include "rive/generated/animation/listener_types/listener_input_type_semantic_base.hpp"

#include <vector>

namespace rive
{
enum class SemanticActionType : uint8_t;

class SemanticInput;
class StateMachineListener;

class ListenerInputTypeSemantic : public ListenerInputTypeSemanticBase
{
public:
    size_t semanticInputCount() const { return m_semanticInputs.size(); }
    const SemanticInput* semanticInput(size_t index) const;
    void addSemanticInput(SemanticInput* input);

    /// True if [listener] should receive a semantic invocation for [action].
    /// Mirrors [ListenerInputTypeKeyboard::keyboardListenerConstraintsMet]:
    /// a [ListenerInputTypeSemantic] with no [semanticInput] rows matches any
    /// [action]. A base [ListenerInputType] whose value is
    /// [ListenerType::semanticAction] (no typed subclass / no rows) also
    /// matches any [action]. Otherwise at least one [SemanticInput] row must
    /// match [action].
    static bool semanticListenerConstraintsMet(
        const StateMachineListener* listener,
        SemanticActionType action);

private:
    std::vector<SemanticInput*> m_semanticInputs;
};
} // namespace rive

#endif

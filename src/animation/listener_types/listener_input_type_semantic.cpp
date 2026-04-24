#include "rive/animation/listener_types/listener_input_type_semantic.hpp"
#include "rive/animation/semantic_listener_group.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/inputs/semantic_input.hpp"

using namespace rive;

const SemanticInput* ListenerInputTypeSemantic::semanticInput(
    size_t index) const
{
    if (index < m_semanticInputs.size())
    {
        return m_semanticInputs[index];
    }
    return nullptr;
}

void ListenerInputTypeSemantic::addSemanticInput(SemanticInput* input)
{
    if (input == nullptr)
    {
        return;
    }
    for (SemanticInput* existing : m_semanticInputs)
    {
        if (existing == input)
        {
            return;
        }
    }
    m_semanticInputs.push_back(input);
}

bool ListenerInputTypeSemantic::semanticListenerConstraintsMet(
    const StateMachineListener* listener,
    SemanticActionType action)
{
    if (listener == nullptr)
    {
        return false;
    }
    const uint32_t actionValue = static_cast<uint32_t>(action);
    for (size_t i = 0; i < listener->listenerInputTypeCount(); ++i)
    {
        const ListenerInputType* lit = listener->listenerInputType(i);
        if (lit == nullptr)
        {
            continue;
        }
        if (lit->is<ListenerInputTypeSemantic>())
        {
            const ListenerInputTypeSemantic* sits =
                lit->as<ListenerInputTypeSemantic>();
            // No rows (or not yet deserialized): accept tap, increase, and
            // decrease.
            if (sits->semanticInputCount() == 0)
            {
                return true;
            }
            for (size_t j = 0; j < sits->semanticInputCount(); ++j)
            {
                const SemanticInput* si = sits->semanticInput(j);
                if (si != nullptr && si->actionType() == actionValue)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

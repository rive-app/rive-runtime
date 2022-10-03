#ifndef _RIVE_TRANSITION_IMPORTER_HPP_
#define _RIVE_TRANSITION_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class StateTransition;
class TransitionCondition;

class StateTransitionImporter : public ImportStackObject
{
private:
    StateTransition* m_Transition;

public:
    StateTransitionImporter(StateTransition* transition);
    void addCondition(TransitionCondition* condition);
    StatusCode resolve() override;
};
} // namespace rive
#endif

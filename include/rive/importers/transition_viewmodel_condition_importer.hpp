#ifndef _RIVE_TRANSITION_VIEWMODEL_CONDITION_IMPORTER_HPP_
#define _RIVE_TRANSITION_VIEWMODEL_CONDITION_IMPORTER_HPP_

#include "rive/importers/import_stack.hpp"

namespace rive
{
class TransitionViewModelCondition;
class TransitionComparator;
class DataBind;

class TransitionViewModelConditionImporter : public ImportStackObject
{
private:
    TransitionViewModelCondition* m_TransitionViewModelCondition;

public:
    TransitionViewModelConditionImporter(
        TransitionViewModelCondition* transitionViewModelCondition);
    void setComparator(TransitionComparator* comparator);
};
} // namespace rive
#endif
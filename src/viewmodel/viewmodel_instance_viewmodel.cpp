#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/refcnt.hpp"

using namespace rive;

ViewModelInstanceViewModel::~ViewModelInstanceViewModel() {}

void ViewModelInstanceViewModel::propertyValueChanged()
{
#ifdef WITH_RIVE_TOOLS
    if (m_changedCallback != nullptr)
    {
        m_changedCallback(this);
    }
#endif
    onValueChanged();
}

void ViewModelInstanceViewModel::setRoot(rcp<ViewModelInstance> value)
{
    Super::setRoot(value);
    m_referenceViewModelInstance->setRoot(value);
}

void ViewModelInstanceViewModel::advanced()
{
    if (m_referenceViewModelInstance != nullptr)
    {
        m_referenceViewModelInstance->advanced();
    }
}

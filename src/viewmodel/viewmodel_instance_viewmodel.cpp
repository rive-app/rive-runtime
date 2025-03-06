#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

using namespace rive;

ViewModelInstanceViewModel::~ViewModelInstanceViewModel() {}

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
#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

using namespace rive;

void ViewModelInstanceViewModel::setRoot(ViewModelInstance* value)
{
    Super::setRoot(value);
    referenceViewModelInstance()->setRoot(value);
}
#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

void ViewModelInstanceNumber::propertyValueChanged() { addDirt(ComponentDirt::Bindings); }
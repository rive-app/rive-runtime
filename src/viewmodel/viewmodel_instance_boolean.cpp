#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

void ViewModelInstanceBoolean::propertyValueChanged() { addDirt(ComponentDirt::Bindings); }
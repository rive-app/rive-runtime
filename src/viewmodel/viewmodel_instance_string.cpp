#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

void ViewModelInstanceString::propertyValueChanged() { addDirt(ComponentDirt::Bindings); }
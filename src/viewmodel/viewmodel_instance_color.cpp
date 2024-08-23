#include <sstream>
#include <iomanip>
#include <array>

#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/component_dirt.hpp"

using namespace rive;

void ViewModelInstanceColor::propertyValueChanged() { addDirt(ComponentDirt::Bindings); }
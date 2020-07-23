#include "shapes/parametric_path.hpp"

using namespace rive;

void ParametricPath::widthChanged() { markPathDirty(); }
void ParametricPath::heightChanged() { markPathDirty(); }
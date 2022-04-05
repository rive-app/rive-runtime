#include "rive/animation/system_state_instance.hpp"
using namespace rive;

SystemStateInstance::SystemStateInstance(const LayerState* layerState, Artboard* instance) :
    StateInstance(layerState) {}

void SystemStateInstance::advance(float seconds, SMIInput** inputs) {}
void SystemStateInstance::apply(float mix) {}

bool SystemStateInstance::keepGoing() const { return false; }
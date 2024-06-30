#include "rive/animation/system_state_instance.hpp"
using namespace rive;

SystemStateInstance::SystemStateInstance(const LayerState* layerState, ArtboardInstance* instance) :
    StateInstance(layerState)
{}

void SystemStateInstance::advance(float seconds, StateMachineInstance* stateMachineInstance) {}
void SystemStateInstance::apply(ArtboardInstance* artboard, float mix) {}

bool SystemStateInstance::keepGoing() const { return false; }
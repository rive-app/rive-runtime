#include "rive/animation/state_instance.hpp"
using namespace rive;

StateInstance::StateInstance(const LayerState* layerState) : m_LayerState(layerState) {}

StateInstance::~StateInstance() {}

const LayerState* StateInstance::state() const { return m_LayerState; }
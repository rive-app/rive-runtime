#include "rive/animation/blend_state_direct.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/blend_state_direct_instance.hpp"
#include "rive/importers/state_machine_importer.hpp"

using namespace rive;

std::unique_ptr<StateInstance> BlendStateDirect::makeInstance(ArtboardInstance* instance) const
{
    return rivestd::make_unique<BlendStateDirectInstance>(this, instance);
}
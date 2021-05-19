#include "animation/blend_state_direct.hpp"
#include "animation/state_machine.hpp"
#include "animation/state_machine_number.hpp"
#include "animation/blend_state_direct_instance.hpp"
#include "importers/state_machine_importer.hpp"

using namespace rive;

StateInstance* BlendStateDirect::makeInstance() const
{
	return new BlendStateDirectInstance(this);
}
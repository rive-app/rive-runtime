#include "generated/bones/cubic_weight_base.hpp"
#include "bones/cubic_weight.hpp"

using namespace rive;

Core* CubicWeightBase::clone() const
{
	auto cloned = new CubicWeight();
	cloned->copy(*this);
	return cloned;
}

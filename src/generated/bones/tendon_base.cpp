#include "generated/bones/tendon_base.hpp"
#include "bones/tendon.hpp"

using namespace rive;

Core* TendonBase::clone() const
{
	auto cloned = new Tendon();
	cloned->copy(*this);
	return cloned;
}

#include "generated/bones/bone_base.hpp"
#include "bones/bone.hpp"

using namespace rive;

Core* BoneBase::clone() const
{
	auto cloned = new Bone();
	cloned->copy(*this);
	return cloned;
}

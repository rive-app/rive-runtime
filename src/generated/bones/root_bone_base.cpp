#include "generated/bones/root_bone_base.hpp"
#include "bones/root_bone.hpp"

using namespace rive;

Core* RootBoneBase::clone() const
{
	auto cloned = new RootBone();
	cloned->copy(*this);
	return cloned;
}

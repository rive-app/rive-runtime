#include "bones/bone.hpp"

using namespace rive;

void Bone::addChildBone(Bone* bone) { m_ChildBones.push_back(bone); }

StatusCode Bone::onAddedClean(CoreContext* context)
{
	Super::onAddedClean(context);
	if (!parent()->is<Bone>())
	{
		return StatusCode::MissingObject;
	}
	parent()->as<Bone>()->addChildBone(this);
	return StatusCode::Ok;
}

void Bone::lengthChanged()
{
	for (auto bone : m_ChildBones)
	{
		bone->markTransformDirty();
	}
}

float Bone::x() const { return parent()->as<Bone>()->length(); }

float Bone::y() const { return 0.0f; }
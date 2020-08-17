#include "bones/bone.hpp"

using namespace rive;

void Bone::addChildBone(Bone* bone) { m_ChildBones.push_back(bone); }
void Bone::onAddedClean(CoreContext* context)
{
	parent()->as<Bone>()->addChildBone(this);
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

//   /// Sets the position of the Node
//   set translation(Vec2D pos) {
//     x = pos[0];
//     y = pos[1];
//   }

//   @override
//   void xChanged(double from, double to) {
//     markTransformDirty();
//   }

//   @override
//   void yChanged(double from, double to) {
//     markTransformDirty();
//   }

//   AABB get localBounds => AABB.fromValues(x, y, x, y);
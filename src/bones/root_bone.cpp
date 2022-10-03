#include "rive/bones/root_bone.hpp"

using namespace rive;

StatusCode RootBone::onAddedClean(CoreContext* context)
{
    // Intentionally doesn't call Super(Bone)::onAddedClean and goes straight to
    // the super.super TransformComponent as that assumes the parent must be a
    // Bone while a root bone is a special case Bone that can be parented to
    // other TransformComponents.
    return TransformComponent::onAddedClean(context);
}

void RootBone::xChanged() { markTransformDirty(); }
void RootBone::yChanged() { markTransformDirty(); }
#include "rive/bones/bone.hpp"
#include "rive/math/vec2d.hpp"
#include <algorithm>

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

Vec2D Bone::tipWorldTranslation() const { return worldTransform() * Vec2D(length(), 0); }

void Bone::addPeerConstraint(Constraint* peer)
{
    assert(std::find(m_PeerConstraints.begin(), m_PeerConstraints.end(), peer) ==
           m_PeerConstraints.end());
    m_PeerConstraints.push_back(peer);
}

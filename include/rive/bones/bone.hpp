#ifndef _RIVE_BONE_HPP_
#define _RIVE_BONE_HPP_
#include "rive/generated/bones/bone_base.hpp"
#include <stdio.h>
#include <vector>

namespace rive
{
class Constraint;
class Bone : public BoneBase
{

private:
    std::vector<Bone*> m_ChildBones;
    std::vector<Constraint*> m_PeerConstraints;

public:
    StatusCode onAddedClean(CoreContext* context) override;
    float x() const override;
    float y() const override;

    inline const std::vector<Bone*> childBones() { return m_ChildBones; }

    void addChildBone(Bone* bone);
    Vec2D tipWorldTranslation() const;
    void addPeerConstraint(Constraint* peer);
    const std::vector<Constraint*>& peerConstraints() const { return m_PeerConstraints; }

private:
    void lengthChanged() override;
};
} // namespace rive

#endif
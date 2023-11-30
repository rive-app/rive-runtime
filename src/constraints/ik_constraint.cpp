#include "rive/constraints/ik_constraint.hpp"
#include "rive/bones/bone.hpp"
#include "rive/artboard.hpp"
#include "rive/math/math_types.hpp"
#include <algorithm>

using namespace rive;

static float atan2(Vec2D v) { return std::atan2(v.y, v.x); }

void IKConstraint::buildDependencies()
{
    Super::buildDependencies();

    // IK Constraint needs to depend on the target so that world transform
    // changes can propagate to the bones (and they can be reset before IK
    // runs).
    if (m_Target != nullptr)
    {
        m_Target->addDependent(this);
    }
}

StatusCode IKConstraint::onAddedClean(CoreContext* context)
{
    if (!parent()->is<Bone>())
    {
        return StatusCode::InvalidObject;
    }

    auto boneCount = parentBoneCount();
    auto bone = parent()->as<Bone>();
    std::vector<Bone*> bones;
    bones.push_back(bone);
    // Get the reverse FK chain of bones (from tip up).
    while (bone->parent()->is<Bone>() && boneCount > 0)
    {
        boneCount--;
        bone = bone->parent()->as<Bone>();
        bone->addPeerConstraint(this);
        bones.push_back(bone);
    }
    int numBones = (int)bones.size();
    m_FkChain.resize(numBones);
    // Now put them in FK order (top to bottom).
    int idx = 0;
    for (auto boneItr = bones.rbegin(); boneItr != bones.rend(); ++boneItr)
    {
        BoneChainLink& link = m_FkChain[idx];
        link.index = idx++;
        link.bone = *boneItr;
        link.angle = 0.0f;
    }

    // Make sure all of the first level children of each bone depend on the
    // tip (constrainedComponent).
    auto tip = parent()->as<Bone>();

    auto artboard = static_cast<Artboard*>(context);

    // Find all children of this bone (we don't directly build up
    // hierarchy at runtime, so we have to traverse everything and check
    // parents).
    for (auto core : artboard->objects())
    {
        if (core == nullptr || !core->is<TransformComponent>())
        {
            continue;
        }
        auto transformComponent = core->as<TransformComponent>();

        for (int i = 1; i < numBones; i++)
        {
            auto childBone = bones[i];
            if (transformComponent->parent() == childBone &&
                std::find(bones.begin(), bones.end(), transformComponent) == bones.end())
            {
                tip->addDependent(transformComponent);
            }
        }
    }
    return Super::onAddedClean(context);
}

void IKConstraint::markConstraintDirty()
{
    Super::markConstraintDirty();
    // We automatically propagate dirt to the parent constrained bone, but we
    // also need to make sure the other bones we influence above it rebuild
    // their transforms.
    for (int i = 0, length = (int)m_FkChain.size() - 1; i < length; i++)
    {
        m_FkChain[i].bone->markTransformDirty();
    }
}

void IKConstraint::solve1(BoneChainLink* fk1, const Vec2D& worldTargetTranslation)
{
    Mat2D iworld = fk1->parentWorldInverse;
    Vec2D pA = fk1->bone->worldTranslation();
    Vec2D pBT(worldTargetTranslation);

    // To target in worldspace
    const Vec2D toTarget = pBT - pA;

    // Note this is directional, hence not transformMat2d
    Vec2D toTargetLocal = Vec2D::transformDir(toTarget, iworld);
    float r = atan2(toTargetLocal);

    constrainRotation(*fk1, r);
    fk1->angle = r;
}

void IKConstraint::solve2(BoneChainLink* fk1,
                          BoneChainLink* fk2,
                          const Vec2D& worldTargetTranslation)
{
    Bone* b1 = fk1->bone;
    Bone* b2 = fk2->bone;
    BoneChainLink* firstChild = &(m_FkChain[fk1->index + 1]);

    const Mat2D& iworld = fk1->parentWorldInverse;

    Vec2D pA = b1->worldTranslation();
    Vec2D pC = firstChild->bone->worldTranslation();
    Vec2D pB = b2->tipWorldTranslation();
    Vec2D pBT(worldTargetTranslation);

    pA = iworld * pA;
    pC = iworld * pC;
    pB = iworld * pB;
    pBT = iworld * pBT;

    // http://mathworld.wolfram.com/LawofCosines.html
    Vec2D av = pB - pC, bv = pC - pA, cv = pBT - pA;
    float a = av.length(), b = bv.length(), c = cv.length();

    float A = std::acos(std::max(-1.0f, std::min(1.0f, (-a * a + b * b + c * c) / (2.0f * b * c))));
    float C = std::acos(std::max(-1.0f, std::min(1.0f, (a * a + b * b - c * c) / (2.0f * a * b))));

    float r1, r2;
    if (b2->parent() != b1)
    {
        BoneChainLink& secondChild = m_FkChain[fk1->index + 2];

        const Mat2D& secondChildWorldInverse = secondChild.parentWorldInverse;

        pC = firstChild->bone->worldTranslation();
        pB = b2->tipWorldTranslation();

        Vec2D avLocal = Vec2D::transformDir(pB - pC, secondChildWorldInverse);
        float angleCorrection = -atan2(avLocal);

        if (invertDirection())
        {
            r1 = atan2(cv) - A;
            r2 = -C + math::PI + angleCorrection;
        }
        else
        {
            r1 = A + atan2(cv);
            r2 = C - math::PI + angleCorrection;
        }
    }
    else if (invertDirection())
    {
        r1 = atan2(cv) - A;
        r2 = -C + math::PI;
    }
    else
    {
        r1 = A + atan2(cv);
        r2 = C - math::PI;
    }

    constrainRotation(*fk1, r1);
    constrainRotation(*firstChild, r2);
    if (firstChild != fk2)
    {
        Bone* bone = fk2->bone;
        bone->mutableWorldTransform() = getParentWorld(*bone) * bone->transform();
    }

    // Simple storage, need this for interpolation.
    fk1->angle = r1;
    firstChild->angle = r2;
}

void IKConstraint::constrainRotation(BoneChainLink& fk, float rotation)
{
    Bone* bone = fk.bone;
    const Mat2D& parentWorld = getParentWorld(*bone);
    Mat2D& transform = bone->mutableTransform();
    TransformComponents& c = fk.transformComponents;

    transform = Mat2D::fromRotation(rotation);

    // Translate
    transform[4] = c.x();
    transform[5] = c.y();
    // Scale
    float scaleX = c.scaleX();
    float scaleY = c.scaleY();
    transform[0] *= scaleX;
    transform[1] *= scaleX;
    transform[2] *= scaleY;
    transform[3] *= scaleY;

    // Skew
    const float skew = c.skew();
    if (skew != 0.0f)
    {
        transform[2] = transform[0] * skew + transform[2];
        transform[3] = transform[1] * skew + transform[3];
    }

    bone->mutableWorldTransform() = parentWorld * transform;
}

void IKConstraint::constrain(TransformComponent* component)
{
    if (m_Target == nullptr || m_Target->isCollapsed())
    {
        return;
    }

    Vec2D worldTargetTranslation = m_Target->worldTranslation();

    // Decompose the chain.
    for (BoneChainLink& item : m_FkChain)
    {
        auto bone = item.bone;
        const Mat2D& parentWorld = getParentWorld(*bone);
        item.parentWorldInverse = parentWorld.invertOrIdentity();

        Mat2D& boneTransform = bone->mutableTransform();
        boneTransform = item.parentWorldInverse * bone->worldTransform();
        item.transformComponents = boneTransform.decompose();
    }

    int count = (int)m_FkChain.size();
    switch (count)
    {
        case 1:
            solve1(&m_FkChain[0], worldTargetTranslation);
            break;
        case 2:
            solve2(&m_FkChain[0], &m_FkChain[1], worldTargetTranslation);
            break;
        default:
        {
            auto last = count - 1;
            BoneChainLink* tip = &m_FkChain[last];
            for (int i = 0; i < last; i++)
            {
                BoneChainLink* item = &m_FkChain[i];
                solve2(item, tip, worldTargetTranslation);
                for (int j = item->index + 1, end = (int)m_FkChain.size() - 1; j < end; j++)
                {
                    BoneChainLink& fk = m_FkChain[j];
                    fk.parentWorldInverse = getParentWorld(*fk.bone).invertOrIdentity();
                }
            }
            break;
        }
    }
    // At the end, mix the FK angle with the IK angle by strength
    if (strength() != 1.0f)
    {
        for (BoneChainLink& fk : m_FkChain)
        {
            float fromAngle = std::fmod(fk.transformComponents.rotation(), math::PI * 2);
            float toAngle = std::fmod(fk.angle, math::PI * 2);
            float diff = toAngle - fromAngle;
            if (diff > math::PI)
            {
                diff -= math::PI * 2;
            }
            else if (diff < -math::PI)
            {
                diff += math::PI * 2;
            }
            float angle = fromAngle + diff * strength();
            constrainRotation(fk, angle);
        }
    }
}

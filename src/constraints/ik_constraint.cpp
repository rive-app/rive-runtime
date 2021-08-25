#include "rive/constraints/ik_constraint.hpp"
#include "rive/bones/bone.hpp"
#include "rive/artboard.hpp"
#include <algorithm>

using namespace rive;

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
			auto bone = bones[i];
			if (transformComponent->parent() == bone &&
			    std::find(bones.begin(), bones.end(), transformComponent) ==
			        bones.end())
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

void IKConstraint::solve1(BoneChainLink* fk1,
                          const Vec2D& worldTargetTranslation)
{
	Mat2D iworld = fk1->parentWorldInverse;
	Vec2D pA;
	fk1->bone->worldTranslation(pA);
	Vec2D pBT(worldTargetTranslation);

	// To target in worldspace
	Vec2D toTarget;
	Vec2D::subtract(toTarget, pBT, pA);

	// Note this is directional, hence not transformMat2d
	Vec2D toTargetLocal;
	Vec2D::transformDir(toTargetLocal, toTarget, iworld);
	float r = std::atan2(toTargetLocal[1], toTargetLocal[0]);

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

	Vec2D pA, pC, pB;
	b1->worldTranslation(pA);
	firstChild->bone->worldTranslation(pC);
	b2->tipWorldTranslation(pB);
	Vec2D pBT(worldTargetTranslation);

	Vec2D::transform(pA, pA, iworld);
	Vec2D::transform(pC, pC, iworld);
	Vec2D::transform(pB, pB, iworld);
	Vec2D::transform(pBT, pBT, iworld);

	// http://mathworld.wolfram.com/LawofCosines.html
	Vec2D av, bv, cv;
	Vec2D::subtract(av, pB, pC);
	float a = Vec2D::length(av);

	Vec2D::subtract(bv, pC, pA);
	float b = Vec2D::length(bv);

	Vec2D::subtract(cv, pBT, pA);
	float c = Vec2D::length(cv);

	float A = std::acos(std::max(
	    -1.0f, std::min(1.0f, (-a * a + b * b + c * c) / (2.0f * b * c))));
	float C = std::acos(std::max(
	    -1.0f, std::min(1.0f, (a * a + b * b - c * c) / (2.0f * a * b))));

	float r1, r2;
	if (b2->parent() != b1)
	{
		BoneChainLink& secondChild = m_FkChain[fk1->index + 2];

		const Mat2D& secondChildWorldInverse = secondChild.parentWorldInverse;

		firstChild->bone->worldTranslation(pC);
		b2->tipWorldTranslation(pB);

		Vec2D avec;
		Vec2D::subtract(avec, pB, pC);
		Vec2D avLocal;
		Vec2D::transformDir(avLocal, avec, secondChildWorldInverse);
		float angleCorrection = -std::atan2(avLocal[1], avLocal[0]);

		if (invertDirection())
		{
			r1 = std::atan2(cv[1], cv[0]) - A;
			r2 = -C + M_PI + angleCorrection;
		}
		else
		{
			r1 = A + std::atan2(cv[1], cv[0]);
			r2 = C - M_PI + angleCorrection;
		}
	}
	else if (invertDirection())
	{
		r1 = std::atan2(cv[1], cv[0]) - A;
		r2 = -C + M_PI;
	}
	else
	{
		r1 = A + std::atan2(cv[1], cv[0]);
		r2 = C - M_PI;
	}

	constrainRotation(*fk1, r1);
	constrainRotation(*firstChild, r2);
	if (firstChild != fk2)
	{
		Bone* bone = fk2->bone;
		Mat2D::multiply(bone->mutableWorldTransform(),
		                getParentWorld(*bone),
		                bone->transform());
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

	if (rotation == 0.0f)
	{
		Mat2D::identity(transform);
	}
	else
	{
		Mat2D::fromRotation(transform, rotation);
	}

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

	Mat2D::multiply(bone->mutableWorldTransform(), parentWorld, transform);
}

void IKConstraint::constrain(TransformComponent* component)
{
	if (m_Target == nullptr)
	{
		return;
	}

	Vec2D worldTargetTranslation;
	m_Target->worldTranslation(worldTargetTranslation);

	// Decompose the chain.
	for (BoneChainLink& item : m_FkChain)
	{
		auto bone = item.bone;
		const Mat2D& parentWorld = getParentWorld(*bone);
		Mat2D::invert(item.parentWorldInverse, parentWorld);

		Mat2D& boneTransform = bone->mutableTransform();
		Mat2D::multiply(
		    boneTransform, item.parentWorldInverse, bone->worldTransform());
		Mat2D::decompose(item.transformComponents, boneTransform);
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
				for (int j = item->index + 1, end = m_FkChain.size() - 1;
				     j < end;
				     j++)
				{
					BoneChainLink& fk = m_FkChain[j];
					Mat2D::invert(fk.parentWorldInverse,
					              getParentWorld(*fk.bone));
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
			float fromAngle =
			    std::fmod(fk.transformComponents.rotation(), (float)M_PI * 2);
			float toAngle = std::fmod(fk.angle, (float)M_PI * 2);
			float diff = toAngle - fromAngle;
			if (diff > M_PI)
			{
				diff -= M_PI * 2;
			}
			else if (diff < -M_PI)
			{
				diff += M_PI * 2;
			}
			float angle = fromAngle + diff * strength();
			constrainRotation(fk, angle);
		}
	}
}

#ifndef _RIVE_I_KCONSTRAINT_HPP_
#define _RIVE_I_KCONSTRAINT_HPP_
#include "rive/generated/constraints/ik_constraint_base.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/transform_components.hpp"
#include <vector>

namespace rive
{
class Bone;
class IKConstraint : public IKConstraintBase
{
private:
    struct BoneChainLink
    {
        int index;
        Bone* bone;
        float angle;
        TransformComponents transformComponents;
        Mat2D parentWorldInverse;
        // The angle the bone held before we wrote to it, and the local
        // transform we left on it.
        float baseRotation = 0.0f;
        Mat2D solvedLocal;
        bool solved = false;
        // Whether the pose found on the bone this solve was our own.
        bool ours = false;

        bool holdsOurSolve() const;
        void recordSolve();
    };
    std::vector<BoneChainLink> m_FkChain;
    void solve1(BoneChainLink* fk1, const Vec2D& worldTargetTranslation);
    void solve2(BoneChainLink* fk1,
                BoneChainLink* fk2,
                const Vec2D& worldTargetTranslation);
    void constrainRotation(BoneChainLink& fk, float rotation);

public:
    void markConstraintDirty() override;
    StatusCode onAddedClean(CoreContext* context) override;
    void constrain(TransformComponent* component) override;
    void buildDependencies() override;
    void invertDirectionChanged() override;
};
} // namespace rive

#endif
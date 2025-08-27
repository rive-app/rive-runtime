#ifndef _RIVE_POINTS_PATH_HPP_
#define _RIVE_POINTS_PATH_HPP_
#include "rive/bones/skinnable.hpp"
#include "rive/generated/shapes/points_path_base.hpp"
namespace rive
{
class PointsPath : public PointsPathBase, public Skinnable
{
public:
    void buildDependencies() override;
    void update(ComponentDirt value) override;
    void markPathDirty(bool sendToLayout = true) override;
    void markSkinDirty() override;
    const Mat2D& pathTransform() const override;
};
} // namespace rive

#endif
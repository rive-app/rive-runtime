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
    // 1 when the path as drawn winds clockwise, -1 otherwise.
    int winding();
#ifdef WITH_RIVE_EDITOR
    void bindingChangedForEditor() override { m_windingReference = 0; }
#endif

private:
    // Measured winding with the bones' mirroring divided out, 0 when unknown.
    int m_windingReference = 0;
};
} // namespace rive

#endif
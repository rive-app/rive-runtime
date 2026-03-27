#ifndef _RIVE_FEATHER_HPP_
#define _RIVE_FEATHER_HPP_
#include "rive/generated/shapes/paint/feather_base.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/transform_space.hpp"

namespace rive
{

class Feather : public FeatherBase
{
public:
    bool validate(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
    void update(ComponentDirt value) override;
    TransformSpace space() const { return (TransformSpace)spaceValue(); }
    void buildDependencies() override;

    ShapePaintPath* innerPath() { return &m_innerPath; }
    void rebuildInnerPath(const ShapePaintPath* path,
                          const Mat2D& shapeTransform,
                          bool offsetInArtboard);

    bool effectPathDirty() const { return m_effectPathDirty; }
    void markEffectPathDirty() { m_effectPathDirty = true; }

protected:
    void strengthChanged() override;
    void offsetXChanged() override;
    void offsetYChanged() override;

private:
    ShapePaintPath m_innerPath;
    bool m_effectPathDirty = true;
#ifdef TESTING
public:
    int renderCount = 0;
#endif
};
} // namespace rive

#endif
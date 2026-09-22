#ifndef _RIVE_PATH_COMPOSER_HPP_
#define _RIVE_PATH_COMPOSER_HPP_
#include "rive/component.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/refcnt.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/shapes/path_flags.hpp"
#include <cmath>
#include <vector>

namespace rive
{
class Shape;
class CommandPath;

class PathComposer : public Component
{

public:
    PathComposer(Shape* shape);
    Shape* shape() const { return m_shape; }
    void buildDependencies() override;
    void onDirty(ComponentDirt dirt) override;
    void update(ComponentDirt value) override;

    ShapePaintPath* localPath() { return &m_localPath; }
    ShapePaintPath* worldPath() { return &m_worldPath; }
    ShapePaintPath* localClockwisePath() { return &m_localClockwisePath; }

    void pathCollapseChanged();

    // The inputs a local path is built from, per path, as of the last build.
    struct LocalPathInput
    {
        Mat2D transform;
        uint32_t geometryVersion;
        bool skipped;
        float linearTolerance;
        float translationTolerance;

        // The composed transform is inverseWorld * pathTransform, so it
        // carries the rounding of an A^-1 * A round trip: bit equality never
        // holds for a shape that has moved, even though the exact result is
        // unchanged. The two halves drift at different scales, so they get
        // separate tolerances, and both are compared against the transform the
        // buffer was actually built from -- which is what keeps the error
        // bounded by the tolerance instead of accumulating frame over frame.
        bool matches(const LocalPathInput& o) const
        {
            if (geometryVersion != o.geometryVersion || skipped != o.skipped)
            {
                return false;
            }
            for (int i = 0; i < 4; i++)
            {
                if (std::abs(transform[i] - o.transform[i]) > o.linearTolerance)
                {
                    return false;
                }
            }
            return std::abs(transform[4] - o.transform[4]) <=
                       o.translationTolerance &&
                   std::abs(transform[5] - o.transform[5]) <=
                       o.translationTolerance;
        }
    };
    bool localInputsChanged();

private:
    std::vector<LocalPathInput> m_localInputs;
    std::vector<LocalPathInput> m_scratchInputs;
    PathFlags m_builtLocalFlags = PathFlags::none;
    bool m_hasLocalInputs = false;
    Shape* m_shape;
    ShapePaintPath m_localPath;
    ShapePaintPath m_worldPath;
    ShapePaintPath m_localClockwisePath;
    bool m_deferredPathDirt;
    bool m_shapeNotified = false;
};
} // namespace rive
#endif

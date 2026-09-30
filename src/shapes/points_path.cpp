#include "rive/artboard.hpp"
#include "rive/shapes/points_path.hpp"
#include "rive/shapes/cubic_vertex.hpp"
#include "rive/shapes/vertex.hpp"
#include "rive/shapes/path_vertex.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/bones/skin.hpp"
#include "rive/span.hpp"
#include "rive/shapes/shape_path_flags.hpp"

#include <algorithm>
#include <cmath>

using namespace rive;

void PointsPath::buildDependencies()
{
    Super::buildDependencies();
    if (skin() != nullptr)
    {
        skin()->addDependent(this);
    }
}

const Mat2D& PointsPath::pathTransform() const
{
    if (skin() != nullptr)
    {
        static Mat2D identity;
        return identity;
    }
    return worldTransform();
}

void PointsPath::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::Path) && skin() != nullptr)
    {
        // Path tracks re-adding ComponentDirt::Path if we deferred due to to
        // shape being invisible.
        skin()->deform(
            Span<Vertex*>((Vertex**)m_Vertices.data(), m_Vertices.size()));
    }
    Super::update(value);
}

void PointsPath::markPathDirty(bool sendToLayout)
{
    // The vertices changed, so the measured winding no longer holds.
    m_windingReference = 0;
    if (skin() != nullptr)
    {
        skin()->addDirt(ComponentDirt::Skin);
    }
    Super::markPathDirty();
}

void PointsPath::markSkinDirty() { Super::markPathDirty(); }

int PointsPath::measureWinding(bool deformed)
{
    const auto& points = m_Vertices;
    size_t count = points.size();
    if (count < 2)
    {
        return 0;
    }
    auto at = [deformed](PathVertex* vertex) {
        return deformed ? vertex->renderTranslation()
                        : Vec2D(vertex->x(), vertex->y());
    };
    // Measured from the first point, so an open path closes on it for free.
    Vec2D origin = at(points[0]);
    Vec2D p0(0, 0);
    float area = 0, minX = 0, minY = 0, maxX = 0, maxY = 0;
    size_t segments = isPathClosed() ? count : count - 1;
    for (size_t i = 0; i < segments; i++)
    {
        auto from = points[i];
        auto to = points[(i + 1) % count];
        Vec2D p3 = at(to) - origin;
        Vec2D p1 = p0, p2 = p3;
        if (from->is<CubicVertex>())
        {
            auto cubic = from->as<CubicVertex>();
            p1 = (deformed ? cubic->renderOut() : cubic->outPoint()) - origin;
        }
        if (to->is<CubicVertex>())
        {
            auto cubic = to->as<CubicVertex>();
            p2 = (deformed ? cubic->renderIn() : cubic->inPoint()) - origin;
        }
        // Exact area of a cubic, a line being one with its handles on its ends.
        area += 6 * Vec2D::cross(p0, p1) + 3 * Vec2D::cross(p0, p2) +
                Vec2D::cross(p0, p3) + 3 * Vec2D::cross(p1, p2) +
                3 * Vec2D::cross(p1, p3) + 6 * Vec2D::cross(p2, p3);
        for (Vec2D p : {p1, p2, p3})
        {
            minX = std::min(minX, p.x);
            minY = std::min(minY, p.y);
            maxX = std::max(maxX, p.x);
            maxY = std::max(maxY, p.y);
        }
        p0 = p3;
    }
    area /= 20;
    float extent = std::max(maxX - minX, maxY - minY);
    // The area sums products of the path's extent, so that is the scale of
    // its rounding. A collapsed path has no winding worth caching.
    if (std::abs(area) <= 1e-5f * extent * extent)
    {
        return 0;
    }
    return area < 0 ? -1 : 1;
}

// Taken from the path as bound, then follows the bones' mirroring. Bones that
// fold the path inside out without mirroring keep that answer, as the flag did,
// so whatever pose a rig loads in can't change what its unfolded path draws.
int PointsPath::winding()
{
    int authored = isClockwise() ? 1 : -1;
    if (skin() == nullptr)
    {
        return authored;
    }
    if (m_windingReference == 0)
    {
        // With every bone at its bind transform the inverse binds cancel,
        // leaving the local points under an affine map that only flips their
        // winding when it mirrors.
        const Mat2D& bind = skin()->bindTransform();
        m_windingReference =
            measureWinding(false) *
            Skin::orientation(bind.xx(), bind.xy(), bind.yx(), bind.yy());
    }
    int sign = skin()->windingSign();
    if (sign != 0 && m_windingReference != 0)
    {
        return m_windingReference * sign;
    }
    // Mixed or collapsed bones leave no mirroring to follow, and a path bound
    // with no area has no reference: measure the pose itself.
    int measured = measureWinding(true);
    if (measured == 0)
    {
        return authored;
    }
    if (sign != 0)
    {
        m_windingReference = measured * sign;
    }
    return measured;
}

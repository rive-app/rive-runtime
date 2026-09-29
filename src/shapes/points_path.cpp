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

namespace
{
// 1 when [path] winds clockwise, -1 when it winds the other way, 0 when it has
// too little area to tell.
int measureWinding(const RawPath& path)
{
    AABB bounds = path.bounds();
    float area = path.computeCoarseArea(bounds.center());
    float extent = std::max(bounds.width(), bounds.height());
    // The area sums products of the path's extent, so that is the scale of
    // its rounding. A collapsed path has no winding worth caching.
    if (std::abs(area) <= 1e-5f * extent * extent)
    {
        return 0;
    }
    return area < 0 ? -1 : 1;
}
} // namespace

// With every bone at its bind transform the tendons' inverse binds cancel them,
// so deform leaves each point at the skin's world transform applied to it.
// Corner radii are left out, they round a corner without turning it around.
int PointsPath::bindWinding()
{
    const auto& points = m_Vertices;
    size_t count = points.size();
    if (count < 2)
    {
        return 0;
    }
    const Mat2D& world = skin()->worldTransform();
    RawPath bound;
    bound.move(world * Vec2D(points[0]->x(), points[0]->y()));
    size_t segments = isPathClosed() ? count : count - 1;
    for (size_t i = 0; i < segments; i++)
    {
        auto from = points[i];
        auto to = points[(i + 1) % count];
        Vec2D end = world * Vec2D(to->x(), to->y());
        if (from->is<CubicVertex>() || to->is<CubicVertex>())
        {
            Vec2D out = from->is<CubicVertex>()
                            ? from->as<CubicVertex>()->outPoint()
                            : Vec2D(from->x(), from->y());
            Vec2D in = to->is<CubicVertex>() ? to->as<CubicVertex>()->inPoint()
                                             : Vec2D(to->x(), to->y());
            bound.cubic(world * out, world * in, end);
        }
        else
        {
            bound.line(end);
        }
    }
    bound.close();
    return measureWinding(bound);
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
        m_windingReference = bindWinding();
    }
    int sign = skin()->windingSign();
    if (sign != 0 && m_windingReference != 0)
    {
        return m_windingReference * sign;
    }
    // Mixed or collapsed bones leave no mirroring to follow, and a path bound
    // with no area has no reference: measure the pose itself.
    int measured = measureWinding(rawPath());
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

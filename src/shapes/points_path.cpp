#include "rive/artboard.hpp"
#include "rive/shapes/points_path.hpp"
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

// Measured once, then follows the bones' mirroring. Bones that fold the path
// inside out without mirroring keep the measured answer, as the flag did.
int PointsPath::winding()
{
    int authored = isClockwise() ? 1 : -1;
    if (skin() == nullptr)
    {
        return authored;
    }
    int sign = skin()->windingSign();
    if (sign != 0 && m_windingReference != 0)
    {
        return m_windingReference * sign;
    }
    AABB bounds = rawPath().bounds();
    float area = rawPath().computeCoarseArea(bounds.center());
    float extent = std::max(bounds.width(), bounds.height());
    // The area sums products of the path's extent, so that is the scale of
    // its rounding. A collapsed path has no winding worth caching.
    if (std::abs(area) <= 1e-5f * extent * extent)
    {
        return authored;
    }
    int measured = area < 0 ? -1 : 1;
    if (sign != 0)
    {
        m_windingReference = measured * sign;
    }
    return measured;
}

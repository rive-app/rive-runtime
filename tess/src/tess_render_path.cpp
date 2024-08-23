#include "rive/tess/tess_render_path.hpp"
#include "rive/tess/contour_stroke.hpp"
#include "tesselator.h"

static const float contourThreshold = 1.0f;

using namespace rive;
TessRenderPath::TessRenderPath() : m_segmentedContour(contourThreshold) {}
TessRenderPath::TessRenderPath(RawPath& rawPath, FillRule fillRule) :
    m_fillRule(fillRule), m_segmentedContour(contourThreshold)
{
    m_rawPath.swap(rawPath);
}

TessRenderPath::~TessRenderPath() {}

void TessRenderPath::rewind()
{
    m_rawPath.rewind();
    m_subPaths.clear();
    m_isContourDirty = m_isTriangulationDirty = true;
    m_isClosed = false;
}

void TessRenderPath::fillRule(FillRule value) { m_fillRule = value; }

void TessRenderPath::moveTo(float x, float y) { m_rawPath.moveTo(x, y); }
void TessRenderPath::lineTo(float x, float y) { m_rawPath.lineTo(x, y); }
void TessRenderPath::cubicTo(float ox, float oy, float ix, float iy, float x, float y)
{
    m_rawPath.cubicTo(ox, oy, ix, iy, x, y);
}
void TessRenderPath::close()
{
    m_rawPath.close();
    m_isClosed = true;
}

void TessRenderPath::addRenderPath(RenderPath* path, const Mat2D& transform)
{
    m_subPaths.emplace_back(SubPath(path, transform));
}

const SegmentedContour& TessRenderPath::segmentedContour() const { return m_segmentedContour; }

// Helper for earcut to understand Vec2D
namespace mapbox
{
namespace util
{

template <> struct nth<0, Vec2D>
{
    inline static float get(const Vec2D& t) { return t.x; };
};
template <> struct nth<1, Vec2D>
{
    inline static float get(const Vec2D& t) { return t.y; };
};

} // namespace util
} // namespace mapbox

const RawPath& TessRenderPath::rawPath() const { return m_rawPath; }

void* stdAlloc(void* userData, unsigned int size)
{
    int* allocated = (int*)userData;
    TESS_NOTUSED(userData);
    *allocated += (int)size;
    return malloc(size);
}

void stdFree(void* userData, void* ptr)
{
    TESS_NOTUSED(userData);
    free(ptr);
}

bool TessRenderPath::triangulate()
{
    if (!m_isTriangulationDirty)
    {
        return false;
    }
    m_isTriangulationDirty = false;
    triangulate(this);
    return true;
}

void TessRenderPath::triangulate(TessRenderPath* containerPath)
{
    AABB bounds = AABB::forExpansion();
    // If there's a single path, we're going to try to assume the user isn't
    // doing any funky self overlapping winding and we'll try to triangulate it
    // quickly as a single polygon.
    if (m_subPaths.size() == 0)
    {
        if (!empty())
        {
            Mat2D identity;
            contour(identity);

            bounds = m_segmentedContour.bounds();

            auto contour = m_segmentedContour.contourPoints();
            auto contours = rive::make_span(&contour, 1);
            m_earcut(contours);

            containerPath->addTriangles(contour, m_earcut.indices);
        }
    }
    else if (m_subPaths.size() == 1)
    {
        // We're a container but we only have 1 path, let's see if we can use
        // our fast triangulator.
        SubPath& subPath = m_subPaths.front();
        auto subRenderPath = static_cast<TessRenderPath*>(subPath.path());
        if (subRenderPath->isContainer())
        {
            // Nope, subpath is also a container, keep going.
            subRenderPath->triangulate(containerPath);
        }
        else if (!subRenderPath->empty())
        {
            // Yes, it's a single path with commands, triangulate it.
            subRenderPath->contour(subPath.transform());
            const SegmentedContour& segmentedContour = subRenderPath->segmentedContour();
            auto contour = segmentedContour.contourPoints();
            auto contours = rive::make_span(&contour, 1);
            m_earcut(contours);

            containerPath->addTriangles(contour, m_earcut.indices);
        }
    }
    else
    {
        // We're a container with multiple sub-paths.
        TESStesselator* tess = nullptr;
        for (SubPath& subPath : m_subPaths)
        {
            auto subRenderPath = static_cast<TessRenderPath*>(subPath.path());
            if (subRenderPath->isContainer())
            {
                subRenderPath->triangulate(containerPath);
            }
            else if (!subRenderPath->empty())
            {
                if (tess == nullptr)
                {
                    tess = tessNewTess(nullptr);
                }
                subRenderPath->contour(subPath.transform());
                const SegmentedContour& segmentedContour = subRenderPath->segmentedContour();
                auto contour = segmentedContour.contourPoints();
                tessAddContour(tess, 2, contour.data(), sizeof(float) * 2, contour.size());
                bounds.expand(segmentedContour.bounds());
            }
        }
        if (tess != nullptr)
        {
            if (tessTesselate(tess, TESS_WINDING_POSITIVE, TESS_POLYGONS, 3, 2, 0))
            {
                auto verts = tessGetVertices(tess);
                // const int* vinds = tessGetVertexIndices(tess);
                auto nverts = tessGetVertexCount(tess);
                auto elems = tessGetElements(tess);
                auto nelems = tessGetElementCount(tess);

                std::vector<uint16_t> indices;
                for (int i = 0; i < nelems * 3; i++)
                {
                    indices.push_back(elems[i]);
                }

                containerPath->addTriangles(
                    Span<const rive::Vec2D>(reinterpret_cast<const Vec2D*>(verts), nverts),
                    indices);
            }
            tessDeleteTess(tess);
        }
    }

    containerPath->setTriangulatedBounds(bounds);
}

void TessRenderPath::contour(const Mat2D& transform)
{
    if (!m_isContourDirty && transform == m_contourTransform)
    {
        return;
    }

    m_isContourDirty = false;
    m_contourTransform = transform;
    m_segmentedContour.contour(m_rawPath, transform);
}

void TessRenderPath::extrudeStroke(ContourStroke* stroke,
                                   StrokeJoin join,
                                   StrokeCap cap,
                                   float strokeWidth,
                                   const Mat2D& transform)
{
    if (isContainer())
    {
        for (auto& subPath : m_subPaths)
        {
            static_cast<TessRenderPath*>(subPath.path())
                ->extrudeStroke(stroke, join, cap, strokeWidth, subPath.transform());
        }
        return;
    }

    contour(transform);
    stroke->extrude(&m_segmentedContour, m_isClosed, join, cap, strokeWidth);
}

bool TessRenderPath::empty() const { return m_rawPath.empty(); }

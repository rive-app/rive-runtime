#include "rive/shapes/path.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/cubic_vertex.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/shapes/deformer.hpp"
#include "rive/shapes/path_vertex.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include "rive/math/math_types.hpp"
#include <cassert>

using namespace rive;

/// Compute an ideal control point distance to create a curve of the given
/// radius. Based on "natural rounding"
/// https://observablehq.com/@daformat/rounding-polygon-corners
float Path::computeIdealControlPointDistance(const Vec2D& toPrev,
                                             const Vec2D& toNext,
                                             float radius)
{
    // Get the angle between next and prev
    float angle = fabs(
        std::atan2(Vec2D::cross(toPrev, toNext), Vec2D::dot(toPrev, toNext)));

    return fmin(radius,
                (4.0f / 3.0f) *
                    std::tan(math::PI / (2.0f * ((2.0f * math::PI) / angle))) *
                    radius *
                    (angle < math::PI / 2 ? 1 + std::cos(angle)
                                          : 2.0f - std::sin(angle)));
}

static void rotatePoints(const Vec2D& nextPoint,
                         const Vec2D& prevPoint,
                         const Vec2D& point,
                         Vec2D& outPoint,
                         Vec2D& inPoint)
{
    // Calculate angle between original pos and new positions
    auto v1 = prevPoint - nextPoint;
    auto v2 = point - nextPoint;
    float angle = std::atan2(Vec2D::cross(v1, v2), Vec2D::dot(v1, v2));
    {
        // Rotate outPoint around prevPoint twice the angle
        float s = std::sin(angle * 2);
        float c = std::cos(angle * 2);
        outPoint.x -= prevPoint.x;
        outPoint.y -= prevPoint.y;
        float xNew = outPoint.x * c - outPoint.y * s;
        float yNew = outPoint.x * s + outPoint.y * c;
        outPoint.x = xNew + prevPoint.x;
        outPoint.y = yNew + prevPoint.y;
    }
    {
        // Rotate inPoint around nextPoint twice the angle
        float s = std::sin(-angle * 2);
        float c = std::cos(-angle * 2);
        inPoint.x -= nextPoint.x;
        inPoint.y -= nextPoint.y;
        float xNew = inPoint.x * c - inPoint.y * s;
        float yNew = inPoint.x * s + inPoint.y * c;
        inPoint.x = xNew + nextPoint.x;
        inPoint.y = yNew + nextPoint.y;
    }
}

RenderPathDeformer* Path::deformer() const
{
    if (m_Shape != nullptr)
    {
        return m_Shape->deformer();
    }
    return nullptr;
}

StatusCode Path::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    // Find the shape.
    for (auto currentParent = parent(); currentParent != nullptr;
         currentParent = currentParent->parent())
    {
        if (currentParent->is<Shape>())
        {
            m_Shape = currentParent->as<Shape>();
            m_Shape->addPath(this);
            return StatusCode::Ok;
        }
    }

    return StatusCode::MissingObject;
}

void Path::buildDependencies() { Super::buildDependencies(); }

void Path::addVertex(PathVertex* vertex) { m_Vertices.push_back(vertex); }

void Path::popVertex() { m_Vertices.pop_back(); }

void Path::addFlags(PathFlags flags) { m_pathFlags |= flags; }
bool Path::isFlagged(PathFlags flags) const
{
    return (int)(m_pathFlags & flags) != 0x00;
}

bool Path::canDeferPathUpdate()
{
    if (m_Shape == nullptr)
    {
        return false;
    }
    // A path cannot defer its update if the shapes requires an update. Note the
    // nuance here where we track that the shape may be marked for follow path
    // (meaning all child paths need to follow path). This doesn't mean the
    // Shape is necessarily forced to update put the paths are, which is why we
    // explicitly also check the shape's path space.

    return m_Shape->canDeferPathUpdate() &&
           !m_Shape->isFlagged(PathFlags::followPath) &&
           !isFlagged(PathFlags::followPath | PathFlags::clipping);
}

const Mat2D& Path::pathTransform() const { return worldTransform(); }

void Path::buildPath(RawPath& rawPath) const
{
    const bool isClosed = isPathClosed();
    const std::vector<PathVertex*>& vertices = m_Vertices;

    auto length = vertices.size();
    if (length < 2)
    {
        return;
    }
    auto firstPoint = vertices[0];

    // Init out to translation
    Vec2D out;
    bool prevIsCubic;

    Vec2D start, startIn;
    bool startIsCubic;

    if (firstPoint->is<CubicVertex>())
    {
        auto cubic = firstPoint->as<CubicVertex>();
        startIsCubic = prevIsCubic = true;
        startIn = cubic->renderIn();
        out = cubic->renderOut();
        start = cubic->renderTranslation();
        rawPath.move(start);
    }
    else
    {
        startIsCubic = prevIsCubic = false;
        auto point = *firstPoint->as<StraightVertex>();
        auto radius = point.radius();
        if (radius != 0.0f)
        {
            auto prev = vertices[length - 1];

            Vec2D pos = point.renderTranslation();

            Vec2D toPrev =
                (prev->is<CubicVertex>() ? prev->as<CubicVertex>()->renderOut()
                                         : prev->renderTranslation()) -
                pos;

            auto toPrevLength = toPrev.normalizeLength();

            auto next = vertices[1];

            Vec2D toNext =
                (next->is<CubicVertex>() ? next->as<CubicVertex>()->renderIn()
                                         : next->renderTranslation()) -
                pos;
            auto toNextLength = toNext.normalizeLength();

            float renderRadius = std::min(
                toPrevLength / 2.0f,
                std::min(toNextLength / 2.0f, radius > 0 ? radius : -radius));
            float idealDistance =
                Path::computeIdealControlPointDistance(toPrev,
                                                       toNext,
                                                       renderRadius);

            startIn = start = Vec2D::scaleAndAdd(pos, toPrev, renderRadius);
            rawPath.move(startIn);

            Vec2D outPoint =
                Vec2D::scaleAndAdd(pos, toPrev, renderRadius - idealDistance);
            Vec2D inPoint =
                Vec2D::scaleAndAdd(pos, toNext, renderRadius - idealDistance);
            out = Vec2D::scaleAndAdd(pos, toNext, renderRadius);

            if (radius < 0)
            {
                rotatePoints(out, startIn, pos, outPoint, inPoint);
            }
            rawPath.cubic(outPoint, inPoint, out);
            prevIsCubic = false;
        }
        else
        {
            startIn = start = out = point.renderTranslation();
            rawPath.move(out);
        }
    }

    for (size_t i = 1; i < length; i++)
    {
        auto vertex = vertices[i];

        if (vertex->is<CubicVertex>())
        {
            auto cubic = vertex->as<CubicVertex>();
            auto inPoint = cubic->renderIn();
            auto translation = cubic->renderTranslation();

            rawPath.cubic(out, inPoint, translation);

            prevIsCubic = true;
            out = cubic->renderOut();
        }
        else
        {
            auto point = *vertex->as<StraightVertex>();
            Vec2D pos = point.renderTranslation();
            auto radius = point.radius();
            if (radius != 0.0f)
            {
                auto prev = vertices[i - 1];
                Vec2D toPrev = (prev->is<CubicVertex>()
                                    ? prev->as<CubicVertex>()->renderOut()
                                    : prev->renderTranslation()) -
                               pos;
                auto toPrevLength = toPrev.normalizeLength();

                auto next = vertices[(i + 1) % length];

                Vec2D toNext = (next->is<CubicVertex>()
                                    ? next->as<CubicVertex>()->renderIn()
                                    : next->renderTranslation()) -
                               pos;
                auto toNextLength = toNext.normalizeLength();

                float renderRadius =
                    std::min(toPrevLength / 2.0f,
                             std::min(toNextLength / 2.0f,
                                      radius > 0 ? radius : -radius));
                float idealDistance =
                    computeIdealControlPointDistance(toPrev,
                                                     toNext,
                                                     renderRadius);

                Vec2D translation =
                    Vec2D::scaleAndAdd(pos, toPrev, renderRadius);
                if (prevIsCubic)
                {
                    rawPath.cubic(out, translation, translation);
                }
                else
                {
                    rawPath.line(translation);
                }

                Vec2D outPoint =
                    Vec2D::scaleAndAdd(pos,
                                       toPrev,
                                       renderRadius - idealDistance);
                Vec2D inPoint =
                    Vec2D::scaleAndAdd(pos,
                                       toNext,
                                       renderRadius - idealDistance);
                out = Vec2D::scaleAndAdd(pos, toNext, renderRadius);

                if (radius < 0)
                {
                    rotatePoints(out, translation, pos, outPoint, inPoint);
                }
                rawPath.cubic(outPoint, inPoint, out);
                prevIsCubic = false;
            }
            else if (prevIsCubic)
            {
                rawPath.cubic(out, pos, pos);

                prevIsCubic = false;
                out = pos;
            }
            else
            {
                out = pos;
                rawPath.line(out);
            }
        }
    }
    if (isClosed)
    {
        if (prevIsCubic || startIsCubic)
        {
            rawPath.cubic(out, startIn, start);
        }
        else
        {
            rawPath.line(start);
        }
        rawPath.close();
    }

    RenderPathDeformer* pathDeformer = deformer();
    if (pathDeformer != nullptr)
    {
        Mat2D transform = pathTransform();
        Mat2D inverse = transform.invertOrIdentity();
        pathDeformer->deformLocalRenderPath(rawPath, transform, inverse);
    }
}

void Path::markPathDirty(bool sendToLayout)
{
    addDirt(ComponentDirt::Path);
    if (m_Shape != nullptr)
    {
        m_Shape->pathChanged();
    }
}

void Path::onDirty(ComponentDirt value)
{
    if (hasDirt(value,
                ComponentDirt::WorldTransform | ComponentDirt::NSlicer) &&
        m_Shape != nullptr)
    {
        m_Shape->pathChanged();
    }
    if (m_deferredPathDirt)
    {
        addDirt(ComponentDirt::Path);
    }
}

void Path::update(ComponentDirt value)
{
    Super::update(value);

    bool pathChanged = hasDirt(value, ComponentDirt::Path);
    bool worldTransformChanged = hasDirt(value, ComponentDirt::WorldTransform);
    bool deformerChanged = hasDirt(value, ComponentDirt::NSlicer);

    if (pathChanged || deformerChanged ||
        (deformer() != nullptr && worldTransformChanged))
    {
        if (canDeferPathUpdate())
        {
            m_deferredPathDirt = true;
            return;
        }
        m_deferredPathDirt = false;
        // Build path doesn't explicitly rewind because we use it to concatenate
        // multiple built paths into a single command path (like the hit
        // tester).
        m_rawPath.rewind();
        buildPath(m_rawPath);
    }
    // if (hasDirt(value, ComponentDirt::WorldTransform) && m_Shape != nullptr)
    // {
    // 	// Make sure the path composer has an opportunity to rebuild the path
    // 	// (this is why the composer depends on the shape and all its paths,
    // 	// ascertaning it updates after both)
    // 	m_Shape->pathChanged();
    // }
}

void Path::isHoleChanged() { markPathDirty(); }

bool Path::collapse(bool value)
{
    bool changed = Super::collapse(value);
    if (changed && m_Shape != nullptr)
    {
        m_Shape->pathCollapseChanged();
    }

    return changed;
}

#ifdef ENABLE_QUERY_FLAT_VERTICES

class DisplayCubicVertex : public CubicVertex
{
public:
    DisplayCubicVertex(const Vec2D& in,
                       const Vec2D& out,
                       const Vec2D& translation)

    {
        m_InPoint = in;
        m_OutPoint = out;
        m_InValid = true;
        m_OutValid = true;
        x(translation.x);
        y(translation.y);
    }

    void computeIn() override {}
    void computeOut() override {}
};

FlattenedPath* Path::makeFlat(bool transformToParent)
{
    if (m_Vertices.empty())
    {
        return nullptr;
    }

    // Path transform always puts the path into world space.
    auto transform = pathTransform();

    if (transformToParent && parent()->is<TransformComponent>())
    {
        // Put the transform in parent space.
        auto world = parent()->as<TransformComponent>()->worldTransform();
        transform = world.invertOrIdentity() * transform;
    }

    FlattenedPath* flat = new FlattenedPath();
    auto length = m_Vertices.size();
    PathVertex* previous = isPathClosed() ? m_Vertices[length - 1] : nullptr;
    bool deletePrevious = false;
    for (size_t i = 0; i < length; i++)
    {
        auto vertex = m_Vertices[i];

        switch (vertex->coreType())
        {
            case StraightVertex::typeKey:
            {
                auto point = *vertex->as<StraightVertex>();
                if (point.radius() > 0.0f &&
                    (isPathClosed() || (i != 0 && i != length - 1)))
                {
                    auto next = m_Vertices[(i + 1) % length];

                    Vec2D prevPoint =
                        previous->is<CubicVertex>()
                            ? previous->as<CubicVertex>()->renderOut()
                            : previous->renderTranslation();
                    Vec2D nextPoint = next->is<CubicVertex>()
                                          ? next->as<CubicVertex>()->renderIn()
                                          : next->renderTranslation();

                    Vec2D pos = point.renderTranslation();

                    Vec2D toPrev = prevPoint - pos;
                    auto toPrevLength = toPrev.normalizeLength();

                    Vec2D toNext = nextPoint - pos;
                    auto toNextLength = toNext.normalizeLength();

                    auto renderRadius =
                        std::min(toPrevLength / 2.0f,
                                 std::min(toNextLength / 2.0f, point.radius()));
                    float idealDistance =
                        computeIdealControlPointDistance(toPrev,
                                                         toNext,
                                                         renderRadius);
                    Vec2D translation =
                        Vec2D::scaleAndAdd(pos, toPrev, renderRadius);

                    Vec2D out =
                        Vec2D::scaleAndAdd(pos,
                                           toPrev,
                                           renderRadius - idealDistance);
                    {
                        auto v1 = new DisplayCubicVertex(translation,
                                                         out,
                                                         translation);
                        flat->addVertex(v1, transform);
                        delete v1;
                    }

                    translation = Vec2D::scaleAndAdd(pos, toNext, renderRadius);

                    Vec2D in = Vec2D::scaleAndAdd(pos,
                                                  toNext,
                                                  renderRadius - idealDistance);
                    auto v2 =
                        new DisplayCubicVertex(in, translation, translation);

                    flat->addVertex(v2, transform);
                    if (deletePrevious)
                    {
                        delete previous;
                    }
                    previous = v2;
                    deletePrevious = true;
                    break;
                }
                RIVE_FALLTHROUGH;
            }
            default:
                if (deletePrevious)
                {
                    delete previous;
                }
                previous = vertex;
                deletePrevious = false;
                flat->addVertex(previous, transform);
                break;
        }
    }
    if (deletePrevious)
    {
        delete previous;
    }
    return flat;
}

void FlattenedPath::addVertex(PathVertex* vertex, const Mat2D& transform)
{
    // To make this easy and relatively clean we just transform the vertices.
    // Requires the vertex to be passed in as a clone.
    if (vertex->is<CubicVertex>())
    {
        auto cubic = vertex->as<CubicVertex>();

        // Cubics need to be transformed so we create a Display version which
        // has set in/out values.
        const auto in = transform * cubic->renderIn();
        const auto out = transform * cubic->renderOut();
        const auto translation = transform * cubic->renderTranslation();

        auto displayCubic = new DisplayCubicVertex(in, out, translation);
        m_Vertices.push_back(displayCubic);
    }
    else
    {
        auto point = new PathVertex();
        Vec2D translation = transform * vertex->renderTranslation();
        point->x(translation.x);
        point->y(translation.y);
        m_Vertices.push_back(point);
    }
}

FlattenedPath::~FlattenedPath()
{
    for (auto vertex : m_Vertices)
    {
        delete vertex;
    }
}

#endif

#include "rive/shapes/path.hpp"
#include "rive/math/circle_constant.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/cubic_vertex.hpp"
#include "rive/shapes/cubic_detached_vertex.hpp"
#include "rive/shapes/path_vertex.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/straight_vertex.hpp"
#include <cassert>

using namespace rive;

StatusCode Path::onAddedClean(CoreContext* context) {
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok) {
        return code;
    }

    // Find the shape.
    for (auto currentParent = parent(); currentParent != nullptr;
         currentParent = currentParent->parent())
    {
        if (currentParent->is<Shape>()) {
            m_Shape = currentParent->as<Shape>();
            m_Shape->addPath(this);
            return StatusCode::Ok;
        }
    }

    return StatusCode::MissingObject;
}

void Path::buildDependencies() {
    Super::buildDependencies();
    // Make sure this is called once the shape has all of the paints added
    // (paints get added during the added cycle so buildDependencies is a good
    // time to do this.)
    m_CommandPath = m_Shape->makeCommandPath(PathSpace::Neither);
}

void Path::addVertex(PathVertex* vertex) { m_Vertices.push_back(vertex); }

const Mat2D& Path::pathTransform() const { return worldTransform(); }

void Path::buildPath(CommandPath& commandPath) const {
    commandPath.reset();

    const bool isClosed = isPathClosed();
    const std::vector<PathVertex*>& vertices = m_Vertices;

    auto length = vertices.size();
    if (length < 2) {
        return;
    }
    auto firstPoint = vertices[0];

    // Init out to translation
    float outX, outY;
    bool prevIsCubic;

    float startX, startY;
    float startInX, startInY;
    bool startIsCubic;

    if (firstPoint->is<CubicVertex>()) {
        auto cubic = firstPoint->as<CubicVertex>();
        startIsCubic = prevIsCubic = true;
        auto inPoint = cubic->renderIn();
        startInX = inPoint[0];
        startInY = inPoint[1];
        auto outPoint = cubic->renderOut();
        outX = outPoint[0];
        outY = outPoint[1];
        auto translation = cubic->renderTranslation();
        commandPath.moveTo(startX = translation[0], startY = translation[1]);
    } else {
        startIsCubic = prevIsCubic = false;
        auto point = *firstPoint->as<StraightVertex>();

        if (auto radius = point.radius(); radius > 0.0f) {
            auto prev = vertices[length - 1];

            Vec2D pos = point.renderTranslation();

            Vec2D toPrev = (prev->is<CubicVertex>() ? prev->as<CubicVertex>()->renderOut()
                                                    : prev->renderTranslation()) -
                           pos;

            auto toPrevLength = toPrev.length();
            toPrev[0] /= toPrevLength;
            toPrev[1] /= toPrevLength;

            auto next = vertices[1];

            Vec2D toNext = (next->is<CubicVertex>() ? next->as<CubicVertex>()->renderIn()
                                                    : next->renderTranslation()) -
                           pos;
            auto toNextLength = toNext.length();
            toNext[0] /= toNextLength;
            toNext[1] /= toNextLength;

            float renderRadius = std::min(toPrevLength, std::min(toNextLength, radius));

            Vec2D translation = Vec2D::scaleAndAdd(pos, toPrev, renderRadius);
            commandPath.moveTo(startInX = startX = translation[0],
                               startInY = startY = translation[1]);

            Vec2D outPoint = Vec2D::scaleAndAdd(pos, toPrev, icircleConstant * renderRadius);
            Vec2D inPoint = Vec2D::scaleAndAdd(pos, toNext, icircleConstant * renderRadius);
            Vec2D posNext = Vec2D::scaleAndAdd(pos, toNext, renderRadius);
            commandPath.cubicTo(outPoint[0],
                                outPoint[1],
                                inPoint[0],
                                inPoint[1],
                                outX = posNext[0],
                                outY = posNext[1]);
            prevIsCubic = false;
        } else {
            auto translation = point.renderTranslation();
            commandPath.moveTo(startInX = startX = outX = translation[0],
                               startInY = startY = outY = translation[1]);
        }
    }

    for (size_t i = 1; i < length; i++) {
        auto vertex = vertices[i];

        if (vertex->is<CubicVertex>()) {
            auto cubic = vertex->as<CubicVertex>();
            auto inPoint = cubic->renderIn();
            auto translation = cubic->renderTranslation();

            commandPath.cubicTo(outX, outY, inPoint[0], inPoint[1], translation[0], translation[1]);

            prevIsCubic = true;
            auto outPoint = cubic->renderOut();
            outX = outPoint[0];
            outY = outPoint[1];
        } else {
            auto point = *vertex->as<StraightVertex>();
            Vec2D pos = point.renderTranslation();

            if (auto radius = point.radius(); radius > 0.0f) {
                Vec2D toPrev = Vec2D(outX, outY) - pos;
                auto toPrevLength = toPrev.length();
                toPrev[0] /= toPrevLength;
                toPrev[1] /= toPrevLength;

                auto next = vertices[(i + 1) % length];

                Vec2D toNext = (next->is<CubicVertex>() ? next->as<CubicVertex>()->renderIn()
                                                        : next->renderTranslation()) -
                               pos;
                auto toNextLength = toNext.length();
                toNext[0] /= toNextLength;
                toNext[1] /= toNextLength;

                float renderRadius = std::min(toPrevLength, std::min(toNextLength, radius));

                Vec2D translation = Vec2D::scaleAndAdd(pos, toPrev, renderRadius);
                if (prevIsCubic) {
                    commandPath.cubicTo(
                        outX, outY, translation[0], translation[1], translation[0], translation[1]);
                } else {
                    commandPath.lineTo(translation[0], translation[1]);
                }

                Vec2D outPoint = Vec2D::scaleAndAdd(pos, toPrev, icircleConstant * renderRadius);
                Vec2D inPoint = Vec2D::scaleAndAdd(pos, toNext, icircleConstant * renderRadius);
                Vec2D posNext = Vec2D::scaleAndAdd(pos, toNext, renderRadius);
                commandPath.cubicTo(outPoint[0],
                                    outPoint[1],
                                    inPoint[0],
                                    inPoint[1],
                                    outX = posNext[0],
                                    outY = posNext[1]);
                prevIsCubic = false;
            } else if (prevIsCubic) {
                float x = pos[0];
                float y = pos[1];
                commandPath.cubicTo(outX, outY, x, y, x, y);

                prevIsCubic = false;
                outX = x;
                outY = y;
            } else {
                commandPath.lineTo(outX = pos[0], outY = pos[1]);
            }
        }
    }
    if (isClosed) {
        if (prevIsCubic || startIsCubic) {
            commandPath.cubicTo(outX, outY, startInX, startInY, startX, startY);
        } else {
            commandPath.lineTo(startX, startY);
        }
        commandPath.close();
    }
}

void Path::markPathDirty() {
    addDirt(ComponentDirt::Path);
    if (m_Shape != nullptr) {
        m_Shape->pathChanged();
    }
}

void Path::onDirty(ComponentDirt value) {
    if (hasDirt(value, ComponentDirt::WorldTransform) && m_Shape != nullptr) {
        m_Shape->pathChanged();
    }
}

void Path::update(ComponentDirt value) {
    Super::update(value);

    assert(m_CommandPath != nullptr);
    if (hasDirt(value, ComponentDirt::Path)) {
        buildPath(*m_CommandPath);
    }
    // if (hasDirt(value, ComponentDirt::WorldTransform) && m_Shape != nullptr)
    // {
    // 	// Make sure the path composer has an opportunity to rebuild the path
    // 	// (this is why the composer depends on the shape and all its paths,
    // 	// ascertaning it updates after both)
    // 	m_Shape->pathChanged();
    // }
}

#ifdef ENABLE_QUERY_FLAT_VERTICES

class DisplayCubicVertex : public CubicVertex {
public:
    DisplayCubicVertex(const Vec2D& in, const Vec2D& out, const Vec2D& translation)

    {
        m_InPoint = in;
        m_OutPoint = out;
        m_InValid = true;
        m_OutValid = true;
        x(translation[0]);
        y(translation[1]);
    }

    void computeIn() override {}
    void computeOut() override {}
};

FlattenedPath* Path::makeFlat(bool transformToParent) {
    if (m_Vertices.empty()) {
        return nullptr;
    }

    // Path transform always puts the path into world space.
    auto transform = pathTransform();

    if (transformToParent && parent()->is<TransformComponent>()) {
        // Put the transform in parent space.
        auto world = parent()->as<TransformComponent>()->worldTransform();
        transform = world.invertOrIdentity() * transform;
    }

    FlattenedPath* flat = new FlattenedPath();
    auto length = m_Vertices.size();
    PathVertex* previous = isPathClosed() ? m_Vertices[length - 1] : nullptr;
    bool deletePrevious = false;
    for (size_t i = 0; i < length; i++) {
        auto vertex = m_Vertices[i];

        switch (vertex->coreType()) {
            case StraightVertex::typeKey: {
                auto point = *vertex->as<StraightVertex>();
                if (point.radius() > 0.0f && (isPathClosed() || (i != 0 && i != length - 1))) {
                    auto next = m_Vertices[(i + 1) % length];

                    Vec2D prevPoint = previous->is<CubicVertex>()
                                          ? previous->as<CubicVertex>()->renderOut()
                                          : previous->renderTranslation();
                    Vec2D nextPoint = next->is<CubicVertex>() ? next->as<CubicVertex>()->renderIn()
                                                              : next->renderTranslation();

                    Vec2D pos = point.renderTranslation();

                    Vec2D toPrev = prevPoint - pos;
                    auto toPrevLength = toPrev.length();
                    toPrev[0] /= toPrevLength;
                    toPrev[1] /= toPrevLength;

                    Vec2D toNext = nextPoint - pos;
                    auto toNextLength = toNext.length();
                    toNext[0] /= toNextLength;
                    toNext[1] /= toNextLength;

                    auto renderRadius =
                        std::min(toPrevLength, std::min(toNextLength, point.radius()));
                    Vec2D translation = Vec2D::scaleAndAdd(pos, toPrev, renderRadius);

                    Vec2D out = Vec2D::scaleAndAdd(pos, toPrev, icircleConstant * renderRadius);
                    {
                        auto v1 = new DisplayCubicVertex(translation, out, translation);
                        flat->addVertex(v1, transform);
                        delete v1;
                    }

                    translation = Vec2D::scaleAndAdd(pos, toNext, renderRadius);

                    Vec2D in = Vec2D::scaleAndAdd(pos, toNext, icircleConstant * renderRadius);
                    auto v2 = new DisplayCubicVertex(in, translation, translation);

                    flat->addVertex(v2, transform);
                    if (deletePrevious) {
                        delete previous;
                    }
                    previous = v2;
                    deletePrevious = true;
                    break;
                }
            }
            default:
                if (deletePrevious) {
                    delete previous;
                }
                previous = vertex;
                deletePrevious = false;
                flat->addVertex(previous, transform);
                break;
        }
    }
    if (deletePrevious) {
        delete previous;
    }
    return flat;
}

void FlattenedPath::addVertex(PathVertex* vertex, const Mat2D& transform) {
    // To make this easy and relatively clean we just transform the vertices.
    // Requires the vertex to be passed in as a clone.
    if (vertex->is<CubicVertex>()) {
        auto cubic = vertex->as<CubicVertex>();

        // Cubics need to be transformed so we create a Display version which
        // has set in/out values.
        const auto in = transform * cubic->renderIn();
        const auto out = transform * cubic->renderOut();
        const auto translation = transform * cubic->renderTranslation();

        auto displayCubic = new DisplayCubicVertex(in, out, translation);
        m_Vertices.push_back(displayCubic);
    } else {
        auto point = new PathVertex();
        Vec2D translation = transform * vertex->renderTranslation();
        point->x(translation[0]);
        point->y(translation[1]);
        m_Vertices.push_back(point);
    }
}

FlattenedPath::~FlattenedPath() {
    for (auto vertex : m_Vertices) {
        delete vertex;
    }
}

#endif

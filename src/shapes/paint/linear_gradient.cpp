#include "rive/shapes/paint/linear_gradient.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/node.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/shapes/paint/gradient_stop.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include <algorithm>

using namespace rive;

StatusCode LinearGradient::onAddedDirty(CoreContext* context) {
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok) {
        return code;
    }

    if (!initPaintMutator(this)) {
        return StatusCode::MissingObject;
    }
    return StatusCode::Ok;
}

void LinearGradient::buildDependencies() {
    auto p = parent();
    if (p != nullptr && p->parent() != nullptr) {
        auto parentsParent = p->parent();
        // Parent's parent must be a shape paint container.
        assert(ShapePaintContainer::from(parentsParent) != nullptr);

        // TODO: see if artboard should inherit from some TransformComponent
        // that can return a world transform. We store the container just for
        // doing the transform to world in update. If it's the artboard, then
        // we're already in world so no need to transform.
        m_ShapePaintContainer =
            parentsParent->is<Node>() ? parentsParent->as<Node>() : nullptr;
        parentsParent->addDependent(this);
    }
}

void LinearGradient::addStop(GradientStop* stop) { m_Stops.push_back(stop); }

static bool stopsComparer(GradientStop* a, GradientStop* b) {
    return a->position() < b->position();
}

void LinearGradient::update(ComponentDirt value) {
    // Do the stops need to be re-ordered?
    bool stopsChanged = hasDirt(value, ComponentDirt::Stops);
    if (stopsChanged) {
        std::sort(m_Stops.begin(), m_Stops.end(), stopsComparer);
    }

    bool worldTransformed = hasDirt(value, ComponentDirt::WorldTransform);

    bool paintsInWorldSpace =
        parent()->as<ShapePaint>()->pathSpace() == PathSpace::World;
    // We rebuild the gradient if the gradient is dirty or we paint in world
    // space and the world space transform has changed, or the local transform
    // has changed. Local transform changes when a stop moves in local space.
    bool rebuildGradient =
        hasDirt(value,
                ComponentDirt::Paint | ComponentDirt::RenderOpacity |
                    ComponentDirt::Transform) ||
        (paintsInWorldSpace && worldTransformed);
    if (rebuildGradient) {
        Vec2D start(startX(), startY());
        Vec2D end(endX(), endY());
        // Check if we need to update the world space gradient (if there's no
        // shape container, presumably it's the artboard and we're already in
        // world).
        if (paintsInWorldSpace && m_ShapePaintContainer != nullptr) {
            // Get the start and end of the gradient in world coordinates (world
            // transform of the shape).
            const Mat2D& world = m_ShapePaintContainer->worldTransform();
            start = world * start;
            end = world * end;
        }

        // build up the color and positions lists
        const double ro = opacity() * renderOpacity();
        const auto count = m_Stops.size();
        ColorInt colors[count];
        float stops[count];
        for (size_t i = 0; i < count; ++i) {
            colors[i] = colorModulateOpacity(m_Stops[i]->colorValue(), ro);
            stops[i] = m_Stops[i]->position();
        }
        
        makeGradient(start, end, colors, stops, count);
    }
}

void LinearGradient::makeGradient(Vec2D start, Vec2D end,
                                  const ColorInt colors[], const float stops[], size_t count) {
    auto paint = renderPaint();
    paint->shader(makeLinearGradient(start[0], start[1], end[0], end[1],
                                     colors, stops, count, RenderTileMode::clamp));
}

void LinearGradient::markGradientDirty() { addDirt(ComponentDirt::Paint); }
void LinearGradient::markStopsDirty() {
    addDirt(ComponentDirt::Paint | ComponentDirt::Stops);
}

void LinearGradient::renderOpacityChanged() { markGradientDirty(); }

void LinearGradient::startXChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::startYChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::endXChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::endYChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::opacityChanged() { markGradientDirty(); }

bool LinearGradient::internalIsTranslucent() const {
    if (opacity() < 1) {
        return true;
    }
    for (const auto stop : m_Stops) {
        if (colorAlpha(stop->colorValue()) != 0xFF) {
            return true;
        }
    }
    return false; // all of our stops are opaque
}

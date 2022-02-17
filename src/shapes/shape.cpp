#include "rive/shapes/shape.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/path_composer.hpp"
#include <algorithm>

using namespace rive;

Shape::Shape() : m_PathComposer(this) {}

void Shape::addPath(Path* path) {
    // Make sure the path is not already in the shape.
    assert(std::find(m_Paths.begin(), m_Paths.end(), path) == m_Paths.end());
    m_Paths.push_back(path);
}

void Shape::update(ComponentDirt value) {
    Super::update(value);

    if (hasDirt(value, ComponentDirt::RenderOpacity)) {
        for (auto shapePaint : m_ShapePaints) {
            shapePaint->renderOpacity(renderOpacity());
        }
    }
}

void Shape::pathChanged() {
    m_PathComposer.addDirt(ComponentDirt::Path, true);
    invalidateStrokeEffects();
}

void Shape::draw(Renderer* renderer) {
    if (renderOpacity() == 0.0f) {
        return;
    }
    auto shouldRestore = clip(renderer);

    for (auto shapePaint : m_ShapePaints) {
        if (!shapePaint->isVisible()) {
            continue;
        }
        renderer->save();
        bool paintsInLocal =
            (shapePaint->pathSpace() & PathSpace::Local) == PathSpace::Local;
        if (paintsInLocal) {
            const Mat2D& transform = worldTransform();
            renderer->transform(transform);
        }
        shapePaint->draw(renderer,
                         paintsInLocal ? m_PathComposer.localPath()
                                       : m_PathComposer.worldPath());
        renderer->restore();
    }

    if (shouldRestore) {
        renderer->restore();
    }
}

void Shape::buildDependencies() {
    // Make sure to propagate the call to PathComposer as it's no longer part of
    // Core and owned only by the Shape.
    m_PathComposer.buildDependencies();

    Super::buildDependencies();

    // Set the blend mode on all the shape paints. If we ever animate this
    // property, we'll need to update it in the update cycle/mark dirty when the
    // blend mode changes.
    for (auto paint : m_ShapePaints) {
        paint->blendMode(blendMode());
    }
}

void Shape::addDefaultPathSpace(PathSpace space) {
    m_DefaultPathSpace |= space;
}

StatusCode Shape::onAddedDirty(CoreContext* context) {
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok) {
        return code;
    }
    // This ensures context propagates to path composer too.
    return m_PathComposer.onAddedDirty(context);
}
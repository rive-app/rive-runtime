#include "rive/shapes/paint/linear_gradient.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/node.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/deformer.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/shapes/paint/gradient_stop.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include <algorithm>

using namespace rive;

StatusCode LinearGradient::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    return initPaintMutator(this);
}

void LinearGradient::buildDependencies()
{
    auto p = parent();
    if (p != nullptr && p->parent() != nullptr)
    {
        ContainerComponent* grandParent = p->parent();
        // Parent's parent must be a shape paint container.
        assert(ShapePaintContainer::from(grandParent) != nullptr);

        // We need to find the container that owns our world space transform.
        // This is the first node up the chain (or none, meaning we are in world
        // space).
        m_shapePaintContainer = nullptr;
        for (ContainerComponent* container = grandParent; container != nullptr;
             container = container->parent())
        {
            if (container->is<Node>())
            {
                m_shapePaintContainer = container->as<Node>();
                break;
            }
        }
        if (m_shapePaintContainer != nullptr)
        {
            m_shapePaintContainer->addDependent(this);
        }
        else
        {
            grandParent->addDependent(this);
        }
    }
    updateDeformer();
}

void LinearGradient::updateDeformer()
{
    if (!m_shapePaintContainer)
    {
        return;
    }
    if (m_shapePaintContainer->is<Shape>())
    {
        if (RenderPathDeformer* deformer =
                m_shapePaintContainer->as<Shape>()->deformer())
        {
            Component* component = deformer->asComponent();
            m_deformer = PointDeformer::from(component);
        }
    }
}

void LinearGradient::addStop(GradientStop* stop) { m_stops.push_back(stop); }

static bool stopsComparer(GradientStop* a, GradientStop* b)
{
    return a->position() < b->position();
}

void LinearGradient::update(ComponentDirt value)
{
    // Do the stops need to be re-ordered?
    bool stopsChanged = hasDirt(value, ComponentDirt::Stops);
    if (stopsChanged)
    {
        std::sort(m_stops.begin(), m_stops.end(), stopsComparer);
    }

    // We rebuild the gradient if the gradient is dirty or we paint in world
    // space and the world space transform has changed, or the local transform
    // has changed. Local transform changes when a stop moves in local space.
    bool rebuildGradient =
        hasDirt(value,
                ComponentDirt::Paint | ComponentDirt::RenderOpacity |
                    ComponentDirt::Transform | ComponentDirt::NSlicer) ||
        (
            // paints in world space
            parent()->as<ShapePaint>()->isFlagged(PathFlags::world) &&
            // and had a world transform change
            hasDirt(value, ComponentDirt::WorldTransform));
    if (rebuildGradient)
    {
        applyTo(renderPaint(), 1.0f);
        m_flags = ShapePaintMutator::Flags::none;
        const auto count = m_stops.size();
        ColorInt* colors = m_colorStorage.data();
        for (size_t i = 0; i < count; ++i)
        {
            auto colorValue = colors[i];
            auto opacity = colorOpacity(colorValue);
            if (opacity > 0.0f)
            {
                m_flags |= ShapePaintMutator::Flags::visible;
            }
            if (opacity < 1.0f)
            {
                m_flags |= ShapePaintMutator::Flags::translucent;
            }
        }
    }
}

void LinearGradient::applyTo(RenderPaint* renderPaint, float opacityModifier)
{
    bool paintsInWorldSpace =
        parent()->as<ShapePaint>()->isFlagged(PathFlags::world);
    Vec2D start(startX(), startY());
    Vec2D end(endX(), endY());

    // Check if we need to update the world space gradient (if there's no
    // shape container, presumably it's the artboard and we're already in
    // world).
    if (paintsInWorldSpace && m_shapePaintContainer != nullptr)
    {
        // Get the start and end of the gradient in world coordinates (world
        // transform of the shape).
        const Mat2D& world = m_shapePaintContainer->worldTransform();
        start = world * start;
        end = world * end;
        if (m_deformer)
        {
            start = m_deformer->deformWorldPoint(start);
            end = m_deformer->deformWorldPoint(end);
        }
    }
    else
    {
        if (m_deformer && m_shapePaintContainer != nullptr)
        {
            const Mat2D& world = m_shapePaintContainer->worldTransform();
            Mat2D inverseWorld;
            if (world.invert(&inverseWorld))
            {
                start =
                    m_deformer->deformLocalPoint(start, world, inverseWorld);
                end = m_deformer->deformLocalPoint(end, world, inverseWorld);
            }
        }
    }

    // build up the color and positions lists
    const auto ro = opacity() * renderOpacity() * opacityModifier;
    const auto count = m_stops.size();

    // need some temporary storage. Allocate enough for both arrays
    assert(sizeof(ColorInt) == sizeof(float));
    m_colorStorage.resize(count * 2);
    ColorInt* colors = m_colorStorage.data();
    float* stops = (float*)colors + count;

    for (size_t i = 0; i < count; ++i)
    {
        colors[i] = colorModulateOpacity(m_stops[i]->colorValue(), ro);
        stops[i] = std::max(0.0f, std::min(m_stops[i]->position(), 1.0f));
    }

    makeGradient(renderPaint, start, end, colors, stops, count);
}

void LinearGradient::makeGradient(RenderPaint* renderPaint,
                                  Vec2D start,
                                  Vec2D end,
                                  const ColorInt colors[],
                                  const float stops[],
                                  size_t count) const
{
    auto factory = artboard()->factory();
    renderPaint->shader(factory->makeLinearGradient(start.x,
                                                    start.y,
                                                    end.x,
                                                    end.y,
                                                    colors,
                                                    stops,
                                                    count));
}

void LinearGradient::markGradientDirty() { addDirt(ComponentDirt::Paint); }
void LinearGradient::markStopsDirty()
{
    addDirt(ComponentDirt::Paint | ComponentDirt::Stops);
}

void LinearGradient::renderOpacityChanged() { markGradientDirty(); }

void LinearGradient::startXChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::startYChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::endXChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::endYChanged() { addDirt(ComponentDirt::Transform); }
void LinearGradient::opacityChanged() { markGradientDirty(); }

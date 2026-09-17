#include "rive/shapes/paint/shape_paint_mutator.hpp"
#include "rive/artboard.hpp"
#include "rive/component.hpp"
#include "rive/shapes/paint/shape_paint.hpp"

using namespace rive;

StatusCode ShapePaintMutator::initPaintMutator(Component* component)
{
    m_flags = Flags::translucent | Flags::visible;
    auto parent = component->parent();
    m_component = component;
    // `ShapePaint::initRenderPaint` (below, via the parent) dereferences
    // `component->artboard()->factory()` and then makes a virtual call on it.
    // A file imported with a null Factory, or a component whose artboard never
    // resolved, would fault there instead of here -- with the mutator, not the
    // bad input, on top of the stack. Bail so the dispatcher culls this mutator
    // (and, at edit time, the paint above it) the same way an incompatible one
    // is culled.
    auto* artboard = component->artboard();
    if (artboard == nullptr || artboard->factory() == nullptr)
    {
        return StatusCode::MissingObject;
    }
#ifdef WITH_RIVE_EDITOR
    // Edit-time: the mutator can hydrate in a later batch than its
    // parent ShapePaint, and if the parent was Pass 4-cull'd before
    // we got here (e.g. parent was hydrated alone in batch N with
    // onAddedClean returning InvalidObject for missing mutator),
    // `parent()` returns null. Bail with MissingObject so the
    // dispatcher culls THIS mutator too instead of segfaulting on
    // `null->is<ShapePaint>()`. Same hazard pattern as the crash log
    // from File::import paths — parent unavailable, dereference
    // explodes.
    if (parent == nullptr)
    {
        return StatusCode::MissingObject;
    }
#endif
    if (parent->is<ShapePaint>())
    {
        if (parent->as<ShapePaint>()->renderPaint() != nullptr)
        {
            DEBUG_PRINT(
                "ShapePaintMutator::initPaintMutator - ShapePaint has already "
                "been asigned a mutator. Does this file have multiple solid "
                "color/gradients in one single fill/stroke?");
            return StatusCode::InvalidObject;
        }
        // Set this object as the mutator for the shape paint and get a
        // reference to the paint we'll be mutating.
        m_renderPaint = parent->as<ShapePaint>()->initRenderPaint(this);
        return StatusCode::Ok;
    }
    return StatusCode::MissingObject;
}
void ShapePaintMutator::renderOpacity(float value)
{
    if (m_renderOpacity == value)
    {
        return;
    }
    m_renderOpacity = value;
    renderOpacityChanged();
}

bool ShapePaintMutator::isTranslucent() const
{
    return (m_flags & Flags::translucent) == Flags::translucent;
}

bool ShapePaintMutator::isVisible() const
{
    return (m_flags & Flags::visible) == Flags::visible;
}
#include "rive/animation/cubic_interpolator.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include <cmath>

using namespace rive;

StatusCode CubicInterpolator::onAddedDirty(CoreContext* context)
{
    m_solver.build(x1(), x2());
    return StatusCode::Ok;
}

StatusCode CubicInterpolator::import(ImportStack& importStack)
{
    auto artboardImporter = importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addComponent(this);
    return Super::import(importStack);
}
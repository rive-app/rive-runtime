#include "rive/layout/n_slicer.hpp"
#include "rive/layout/n_slicer_tile_mode.hpp"
#include "rive/shapes/image.hpp"
#include "rive/shapes/slice_mesh.hpp"
#include "rive/core_context.hpp"

using namespace rive;

NSlicer::NSlicer() { m_sliceMesh = rivestd::make_unique<SliceMesh>(this); }

Image* NSlicer::image()
{
    if (parent())
    {
        return parent()->as<Image>();
    }
    return nullptr;
}

int NSlicer::patchIndex(int patchX, int patchY)
{
    return patchY * (static_cast<int>(m_xs.size()) + 1) + patchX;
}

StatusCode NSlicer::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    if (!parent()->is<Image>())
    {
        return StatusCode::MissingObject;
    }

    parent()->as<Image>()->setMesh(m_sliceMesh.get());
    return StatusCode::Ok;
}

void NSlicer::buildDependencies()
{
    Super::buildDependencies();
    parent()->addDependent(this);
}

void NSlicer::addAxisX(Axis* axis) { m_xs.push_back(axis); }

void NSlicer::addAxisY(Axis* axis) { m_ys.push_back(axis); }

void NSlicer::axisChanged() { addDirt(ComponentDirt::Path); }

void NSlicer::addTileMode(int patchIndex, NSlicerTileModeType style)
{
    m_tileModes[patchIndex] = style;
}

void NSlicer::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::Path) || hasDirt(value, ComponentDirt::WorldTransform))
    {
        if (m_sliceMesh != nullptr)
        {
            m_sliceMesh->update();
        }
    }
    Super::update(value);
}

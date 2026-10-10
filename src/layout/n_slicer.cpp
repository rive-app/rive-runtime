#include "rive/layout/n_slicer.hpp"
#include "rive/layout/n_slicer_tile_mode.hpp"
#include "rive/shapes/mesh_host.hpp"
#include "rive/shapes/slice_mesh.hpp"
#include "rive/core_context.hpp"

using namespace rive;

NSlicer::NSlicer() { m_sliceMesh = std::make_unique<SliceMesh>(this); }

MeshHost* NSlicer::host() { return MeshHost::from(parent()); }

StatusCode NSlicer::onAddedDirty(CoreContext* context)
{
    StatusCode code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    MeshHost* meshHost = host();
    if (meshHost == nullptr)
    {
        return StatusCode::MissingObject;
    }

#ifndef WITH_RIVE_EDITOR
    // Runtime-only; editor build registers via editorParentChanged.
    meshHost->setMesh(m_sliceMesh.get());
#endif
    return StatusCode::Ok;
}

void NSlicer::buildDependencies()
{
    Super::buildDependencies();
    parent()->addDependent(this);
}

void NSlicer::axisChanged() { addDirt(ComponentDirt::NSlicer); }

void NSlicer::update(ComponentDirt value)
{
    if (hasDirt(value, ComponentDirt::NSlicer) ||
        hasDirt(value, ComponentDirt::WorldTransform))
    {
        if (m_sliceMesh != nullptr)
        {
            m_sliceMesh->update();
        }
    }
    Super::update(value);
}

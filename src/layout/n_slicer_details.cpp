#include "rive/component.hpp"
#include "rive/layout/n_sliced_node.hpp"
#include "rive/layout/n_slicer.hpp"
#include "rive/layout/n_slicer_details.hpp"
#include "rive/layout/n_slicer_tile_mode.hpp"

using namespace rive;

NSlicerDetails* NSlicerDetails::from(Component* component)
{
    switch (component->coreType())
    {
        case NSlicer::typeKey:
            return component->as<NSlicer>();
        case NSlicedNode::typeKey:
            return component->as<NSlicedNode>();
    }
    return nullptr;
}

int NSlicerDetails::patchIndex(int patchX, int patchY)
{
    return patchY * (static_cast<int>(m_xs.size()) + 1) + patchX;
}

void NSlicerDetails::addAxisX(Axis* axis) { m_xs.push_back(axis); }

void NSlicerDetails::addAxisY(Axis* axis) { m_ys.push_back(axis); }

void NSlicerDetails::addTileMode(int patchIndex, NSlicerTileModeType style)
{
    m_tileModes[patchIndex] = style;
}

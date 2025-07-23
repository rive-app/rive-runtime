#include "rive/container_component.hpp"
#include "rive/layout/artboard_component_list_override.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/artboard.hpp"

using namespace rive;

void ArtboardComponentListOverride::addArtboard(ArtboardInstance* artboard)
{
    m_artboards.push_back(artboard);
    m_styleOverrider.updateWidthOverride(artboard);
    m_styleOverrider.updateHeightOverride(artboard);
}

void ArtboardComponentListOverride::removeArtboard(ArtboardInstance* artboard)
{
    m_artboards.erase(
        std::remove(m_artboards.begin(), m_artboards.end(), artboard),
        m_artboards.end());
}

bool ArtboardComponentListOverride::isRow()
{
    return parent()->is<ArtboardComponentList>()
               ? parent()->as<ArtboardComponentList>()->mainAxisIsRow()
               : true;
}

void ArtboardComponentListOverride::updateWidthOverride()
{
    for (auto& artboardInstance : m_artboards)
    {
        m_styleOverrider.updateWidthOverride(artboardInstance);
    }
}

void ArtboardComponentListOverride::updateHeightOverride()
{
    for (auto& artboardInstance : m_artboards)
    {
        m_styleOverrider.updateHeightOverride(artboardInstance);
    }
}

void ArtboardComponentListOverride::instanceWidthChanged()
{
    updateWidthOverride();
}
void ArtboardComponentListOverride::instanceHeightChanged()
{
    updateHeightOverride();
}
void ArtboardComponentListOverride::instanceWidthUnitsValueChanged()
{
    updateWidthOverride();
}
void ArtboardComponentListOverride::instanceHeightUnitsValueChanged()
{
    updateHeightOverride();
}
void ArtboardComponentListOverride::instanceWidthScaleTypeChanged()
{
    updateWidthOverride();
}
void ArtboardComponentListOverride::instanceHeightScaleTypeChanged()
{
    updateHeightOverride();
}

void ArtboardComponentListOverride::markHostingLayoutDirty(
    ArtboardInstance* artboardInstance)
{
    if (artboard() != nullptr)
    {
        artboard()->markLayoutDirty(artboardInstance);
        artboard()->markLayoutStyleDirty();
    }
}
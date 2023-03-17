#include "rive/text/text_style.hpp"
#include "rive/text/text_style_axis.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/text/text.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

// satisfy unique_ptr
TextStyle::TextStyle() {}

void TextStyle::addVariation(TextStyleAxis* axis) { m_variations.push_back(axis); }

void TextStyle::onDirty(ComponentDirt dirt)
{
    if ((dirt & ComponentDirt::TextShape) == ComponentDirt::TextShape)
    {
        parent()->as<Text>()->markShapeDirty();
    }
}

void TextStyle::update(ComponentDirt value)
{
    if ((value & ComponentDirt::TextShape) == ComponentDirt::TextShape)
    {
        rcp<Font> baseFont = m_fontAsset == nullptr ? nullptr : m_fontAsset->font();
        if (m_variations.empty())
        {
            m_font = baseFont;
        }
        else
        {
            m_coords.clear();
            for (TextStyleAxis* axis : m_variations)
            {
                m_coords.push_back({axis->tag(), axis->axisValue()});
            }
            m_font = baseFont->makeAtCoords(m_coords);
        }
    }
}

void TextStyle::buildDependencies()
{
    addDependent(parent());
    artboard()->addDependent(this);
    Super::buildDependencies();
    auto factory = getArtboard()->factory();
    m_path = factory->makeEmptyRenderPath();
}

void TextStyle::rewindPath()
{
    m_path->rewind();
    m_hasContents = false;
}

bool TextStyle::addPath(const RawPath& rawPath)
{
    bool hadContents = m_hasContents;
    rawPath.addTo(m_path.get());
    m_hasContents = true;
    return !hadContents;
}

void TextStyle::draw(Renderer* renderer)
{
    auto path = m_path.get();
    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->isVisible())
        {
            continue;
        }
        shapePaint->draw(renderer, path);
    }
}

void TextStyle::assets(const std::vector<FileAsset*>& assets)
{
    if ((size_t)fontAssetId() >= assets.size())
    {
        return;
    }
    auto asset = assets[fontAssetId()];
    if (asset->is<FontAsset>())
    {
        m_fontAsset = asset->as<FontAsset>();
    }
}

StatusCode TextStyle::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    return Super::import(importStack);
}

void TextStyle::fontSizeChanged() { parent()->as<Text>()->markShapeDirty(); }

Core* TextStyle::clone() const
{
    TextStyle* twin = TextStyleBase::clone()->as<TextStyle>();
    twin->m_fontAsset = m_fontAsset;
    return twin;
}
#include "rive/text/text_style.hpp"
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

void TextStyle::buildDependencies()
{
    Super::buildDependencies();
    auto factory = getArtboard()->factory();
    m_path = factory->makeEmptyRenderPath();
}

void TextStyle::resetPath()
{
    m_path->reset();
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

const rcp<Font> TextStyle::font() const
{
    return m_fontAsset == nullptr ? nullptr : m_fontAsset->font();
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
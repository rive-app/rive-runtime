#include "rive/text/text_style.hpp"
#include "rive/text/text_style_axis.hpp"
#include "rive/text/text_style_feature.hpp"
#include "rive/text/text_variation_helper.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/text/text.hpp"
#include "rive/artboard.hpp"

using namespace rive;

// satisfy unique_ptr
TextStyle::TextStyle() = default;

void TextStyle::addVariation(TextStyleAxis* axis)
{
    m_variations.push_back(axis);
}

void TextStyle::addFeature(TextStyleFeature* feature)
{
    m_styleFeatures.push_back(feature);
}

void TextStyle::onDirty(ComponentDirt dirt)
{
    if (m_text != nullptr)
    {
        if ((dirt & ComponentDirt::TextShape) == ComponentDirt::TextShape)
        {

            m_text->markShapeDirty();
            if (m_variationHelper != nullptr)
            {
                m_variationHelper->addDirt(ComponentDirt::TextShape);
            }
        }
    }
}

StatusCode TextStyle::onAddedClean(CoreContext* context)
{
    // We know this is good because we validated it during validate().
    m_text = TextInterface::from(parent());

    auto code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    // This ensures context propagates to variation helper too.
    if (!m_variations.empty() || !m_styleFeatures.empty())
    {
        m_variationHelper = rivestd::make_unique<TextVariationHelper>(this);
    }
    if (m_variationHelper != nullptr)
    {
        if ((code = m_variationHelper->onAddedDirty(context)) != StatusCode::Ok)
        {
            return code;
        }
        if ((code = m_variationHelper->onAddedClean(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
    return StatusCode::Ok;
}

const rcp<Font> TextStyle::font() const
{
    if (m_variableFont != nullptr)
    {
        return m_variableFont;
    }

    auto asset = fontAsset();
    return asset == nullptr ? nullptr : asset->font();
}

void TextStyle::updateVariableFont()
{
    auto asset = fontAsset();
    rcp<Font> baseFont = asset == nullptr ? nullptr : asset->font();
    if (baseFont == nullptr)
    {
        // Not ready yet.
        return;
    }
    if (!m_variations.empty() || !m_styleFeatures.empty())
    {
        m_coords.clear();
        for (TextStyleAxis* axis : m_variations)
        {
            m_coords.push_back({axis->tag(), axis->axisValue()});
        }
        m_features.clear();
        for (TextStyleFeature* styleFeature : m_styleFeatures)
        {
            m_features.push_back(
                {styleFeature->tag(), styleFeature->featureValue()});
        }
        m_variableFont = baseFont->withOptions(m_coords, m_features);
    }
    else
    {
        m_variableFont = nullptr;
    }
}

void TextStyle::buildDependencies()
{
    if (m_variationHelper != nullptr)
    {
        m_variationHelper->buildDependencies();
    }
    parent()->addDependent(this);
    Super::buildDependencies();
}

uint32_t TextStyle::assetId() { return this->fontAssetId(); }

void TextStyle::setAsset(FileAsset* asset)
{
    if (asset->is<FontAsset>())
    {
        FileAssetReferencer::setAsset(asset);
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

void TextStyle::fontSizeChanged() { m_text->markShapeDirty(); }

void TextStyle::lineHeightChanged() { m_text->markShapeDirty(); }

void TextStyle::letterSpacingChanged() { m_text->markShapeDirty(); }

Core* TextStyle::clone() const
{
    TextStyle* twin = TextStyleBase::clone()->as<TextStyle>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }

    return twin;
}

bool TextStyle::validate(CoreContext* context)
{
    return TextInterface::from(context->resolve(parentId())) != nullptr;
}
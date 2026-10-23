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
  // This can happen if the Text is in a Solo which is propagating its
  // collapsed state during its onAddedClean (which could be called before
  // ours, so m_text might still be null).
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
  else if (
      m_owner != nullptr &&
      (dirt & ComponentDirt::TextShape) == ComponentDirt::TextShape)
  {
    // A global style's paint: rebuild its variable font, and let the Texts
    // drawing it re-shape.
    if (m_variationHelper != nullptr)
    {
      m_variationHelper->addDirt(ComponentDirt::TextShape);
    }
    m_owner->styleShapeChanged();
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
    m_variationHelper = std::make_unique<TextVariationHelper>(this);
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

  // Lazily build the variable font if variations/features are present but
  // the variable font hasn't been created yet. This ensures the very first
  // shape/measure uses the intended axes (e.g. wght) rather than the font's
  // default axis values.
  if ((!m_variations.empty() || !m_styleFeatures.empty()))
  {
    updateVariableFont();
    if (m_variableFont != nullptr)
    {
      return m_variableFont;
    }
  }

  auto asset = fontAsset();
  return asset == nullptr ? nullptr : asset->font();
}

void TextStyle::updateVariableFont() const
{
  auto asset = fontAsset();
  rcp<Font> baseFont = asset == nullptr ? nullptr : asset->font();
  if (baseFont == nullptr)
  {
    // Not ready yet, or the font was cleared. Drop any cached variable
    // font so font() can't keep returning one built from the old base.
    m_variableFont = nullptr;
    m_variableFontBase = nullptr;
    return;
  }
  if (!m_variations.empty() || !m_styleFeatures.empty())
  {
    // The variation helper calls this whenever it updates, which is far
    // more often than the axes change. Building a new font each time cost
    // a font object per call, and it gave the text a different font every
    // frame, so the text could never tell that its layout had already
    // measured the shape (Text::measureKey) and re-solved it every time.
    if (m_variableFont != nullptr && m_variableFontBase == baseFont &&
        sameOptions())
    {
      return;
    }
    m_coords.clear();
    for (TextStyleAxis* axis : m_variations)
    {
      m_coords.push_back({axis->tag(), axis->axisValue()});
    }
    m_features.clear();
    for (TextStyleFeature* styleFeature : m_styleFeatures)
    {
      m_features.push_back({styleFeature->tag(), styleFeature->featureValue()});
    }
    m_variableFont = baseFont->withOptions(m_coords, m_features);
    m_variableFontBase = baseFont;
  }
  else
  {
    m_variableFont = nullptr;
    m_variableFontBase = nullptr;
  }
}

// Whether the axes and features still hold the values m_variableFont was
// built with, in the same order.
bool TextStyle::sameOptions() const
{
  if (m_coords.size() != m_variations.size() ||
      m_features.size() != m_styleFeatures.size())
  {
    return false;
  }
  for (size_t i = 0; i < m_variations.size(); i++)
  {
    if (m_coords[i].axis != m_variations[i]->tag() ||
        m_coords[i].value != m_variations[i]->axisValue())
    {
      return false;
    }
  }
  for (size_t i = 0; i < m_styleFeatures.size(); i++)
  {
    if (m_features[i].tag != m_styleFeatures[i]->tag() ||
        m_features[i].value != m_styleFeatures[i]->featureValue())
    {
      return false;
    }
  }
  return true;
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

void TextStyle::setAsset(rcp<FileAsset> asset)
{
  if (asset != nullptr && asset->is<FontAsset>())
  {
    FileAssetReferencer::setAsset(asset);
    // Changing the referenced font at runtime (e.g. via data binding) must
    // reshape the text. Mirror FontAsset::font()'s dirtying so onDirty
    // marks the text shape dirty and rebuilds any variable font. Guard for
    // the clone/import case where the style isn't wired to its Text yet.
    if (m_text != nullptr)
    {
      addDirt(ComponentDirt::TextShape);
    }
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

// A metric change re-shapes the Text the style belongs to. A global text
// style's paint has none (it's parented to its style Artboard): its owner
// re-shapes the Texts drawing it. Nothing else dirties the style artboard when
// a keyframe or bind sets one of these, so this is their only notice.
void TextStyle::metricsChanged()
{
  if (m_text != nullptr)
  {
    m_text->markShapeDirty();
  }
  else if (m_owner != nullptr)
  {
    m_owner->styleShapeChanged();
  }
}

void TextStyle::fontSizeChanged() { metricsChanged(); }

void TextStyle::lineHeightChanged() { metricsChanged(); }

void TextStyle::letterSpacingChanged() { metricsChanged(); }

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
  auto parent = context->resolve(parentId());
  // A style normally lives under a Text (TextInterface). A global text
  // style's TextStylePaint instead lives directly under the style Artboard,
  // so accept an Artboard parent too.
  return TextInterface::from(parent) != nullptr ||
         (parent != nullptr && parent->is<Artboard>());
}
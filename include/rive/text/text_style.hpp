#ifndef _RIVE_TEXT_STYLE_HPP_
#define _RIVE_TEXT_STYLE_HPP_
#include "rive/generated/text/text_style_base.hpp"
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/text/text_interface.hpp"
#include "rive/text/text_variation_helper.hpp"

namespace rive
{
class FontAsset;
class Font;
class FileAsset;
class Renderer;
class RenderPath;
class RenderPaint;

class TextVariationHelper;
class TextStyleAxis;
class TextStyleFeature;
class TextInterface;

/// Whatever owns a global text style's instanced paint (one Text's instance of
/// the style, or the instance shared by every Text using it). That paint lives
/// under the style's artboard, not a Text, so the Texts drawing it learn of
/// shape changes (e.g. a font arriving after load) through this.
class TextStyleOwner
{
public:
  virtual ~TextStyleOwner() {}
  virtual void styleShapeChanged() = 0;
};

class TextStyle : public TextStyleBase, public FileAssetReferencer
{
public:
  TextStyle();
  void buildDependencies() override;
  const rcp<Font> font() const;
  void setAsset(rcp<FileAsset>) override;
  uint32_t assetId() override;
  StatusCode import(ImportStack& importStack) override;

  Core* clone() const override;
  void addVariation(TextStyleAxis* axis);
#ifdef WITH_RIVE_EDITOR
  /// Idempotent — TextStyleAxis::editorParentChanged calls this.
  void addVariationForEditor(TextStyleAxis* axis);
  void removeVariationForEditor(TextStyleAxis* axis);
#endif
  void addFeature(TextStyleFeature* feature);
  void updateVariableFont() const;
  StatusCode onAddedClean(CoreContext* context) override;
  void onDirty(ComponentDirt dirt) override;
  bool validate(CoreContext* context) override;

  /// See [TextStyleOwner]; only set on a global text style's paint.
  void owner(TextStyleOwner* value) { m_owner = value; }
  TextStyleOwner* owner() const { return m_owner; }

protected:
  void fontSizeChanged() override;
  void lineHeightChanged() override;
  void letterSpacingChanged() override;
  FontAsset* fontAsset() const { return (FontAsset*)m_fileAsset.get(); }

private:
  void metricsChanged();
  std::unique_ptr<TextVariationHelper> m_variationHelper;

public:
  /// The lazily-created variation helper, or null if this style never needed
  /// one. Exposed so Artboard can map it back to its owning style when
  /// replaying a source artboard's dependency order onto an instance.
  TextVariationHelper* variationHelper() const
  {
    return m_variationHelper.get();
  }

private:
  mutable rcp<Font> m_variableFont;
  // The font m_variableFont was built from, so a rebuild with the same
  // base, axes and features can keep it (see updateVariableFont).
  mutable rcp<Font> m_variableFontBase;
  bool sameOptions() const;

  mutable std::vector<Font::Coord> m_coords;
  std::vector<TextStyleAxis*> m_variations;
  std::vector<TextStyleFeature*> m_styleFeatures;
  mutable std::vector<Font::Feature> m_features;
  TextInterface* m_text = nullptr;
  TextStyleOwner* m_owner = nullptr;
};
} // namespace rive

#endif

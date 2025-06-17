#ifndef _RIVE_TEXT_STYLE_HPP_
#define _RIVE_TEXT_STYLE_HPP_
#include "rive/generated/text/text_style_base.hpp"
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/text/text_interface.hpp"

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
    void addFeature(TextStyleFeature* feature);
    void updateVariableFont();
    StatusCode onAddedClean(CoreContext* context) override;
    void onDirty(ComponentDirt dirt) override;
    bool validate(CoreContext* context) override;

protected:
    void fontSizeChanged() override;
    void lineHeightChanged() override;
    void letterSpacingChanged() override;
    FontAsset* fontAsset() const { return (FontAsset*)m_fileAsset.get(); }

private:
    std::unique_ptr<TextVariationHelper> m_variationHelper;
    rcp<Font> m_variableFont;

    std::vector<Font::Coord> m_coords;
    std::vector<TextStyleAxis*> m_variations;
    std::vector<TextStyleFeature*> m_styleFeatures;
    std::vector<Font::Feature> m_features;
    TextInterface* m_text = nullptr;
};
} // namespace rive

#endif

#ifndef _RIVE_TEXT_STYLE_HPP_
#define _RIVE_TEXT_STYLE_HPP_
#include "rive/generated/text/text_style_base.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/font_asset.hpp"
#include <unordered_map>

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
class TextStyle : public TextStyleBase, public ShapePaintContainer, public FileAssetReferencer
{
private:
    Artboard* getArtboard() override { return artboard(); }

public:
    TextStyle();
    void buildDependencies() override;
    const rcp<Font> font() const;
    void setAsset(FileAsset*) override;
    uint32_t assetId() override;
    StatusCode import(ImportStack& importStack) override;

    FontAsset* fontAsset() const { return (FontAsset*)m_fileAsset; }

    bool addPath(const RawPath& rawPath, float opacity);
    void rewindPath();
    void draw(Renderer* renderer);
    Core* clone() const override;
    void addVariation(TextStyleAxis* axis);
    void addFeature(TextStyleFeature* feature);
    void updateVariableFont();
    StatusCode onAddedClean(CoreContext* context) override;
    void onDirty(ComponentDirt dirt) override;

protected:
    void fontSizeChanged() override;
    void lineHeightChanged() override;
    void letterSpacingChanged() override;

private:
    std::unique_ptr<TextVariationHelper> m_variationHelper;
    std::unordered_map<float, rcp<RenderPath>> m_opacityPaths;
    rcp<Font> m_variableFont;
    rcp<RenderPath> m_path;
    bool m_hasContents = false;
    std::vector<Font::Coord> m_coords;
    std::vector<TextStyleAxis*> m_variations;
    std::vector<rcp<RenderPaint>> m_paintPool;
    std::vector<TextStyleFeature*> m_styleFeatures;
    std::vector<Font::Feature> m_features;
};
} // namespace rive

#endif

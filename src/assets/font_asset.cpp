#include "rive/text/text_style.hpp"
#include "rive/assets/file_asset_referencer.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

bool FontAsset::decode(SimpleArray<uint8_t>& data, Factory* factory)
{
    font(factory->decodeFont(data));
    return m_font != nullptr;
}
std::string FontAsset::fileExtension() const { return "ttf"; }

void FontAsset::font(rcp<Font> font)
{
    m_font = std::move(font);

    // We could try to tie this to some generic FileAssetReferencer callback
    // but for that we'd need this to be behind a more generic setter like
    // ::asset(rcp<Asset> asset)
    for (FileAssetReferencer* fileAssetReferencer : fileAssetReferencers())
    {
        static_cast<TextStyle*>(fileAssetReferencer)->addDirt(ComponentDirt::TextShape);
    }
}
#include "rive/assets/font_asset.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

bool FontAsset::decode(Span<const uint8_t> data, Factory* factory)
{
    m_font = factory->decodeFont(data);
    return m_font != nullptr;
}
std::string FontAsset::fileExtension() { return "ttf"; }

void FontAsset::font(rcp<Font> font) { m_font = std::move(font); }
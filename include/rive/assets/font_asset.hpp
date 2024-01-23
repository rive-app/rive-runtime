#ifndef _RIVE_FONT_ASSET_HPP_
#define _RIVE_FONT_ASSET_HPP_
#include "rive/generated/assets/font_asset_base.hpp"
#include "rive/text_engine.hpp"
#include "rive/refcnt.hpp"

namespace rive
{
class FontAsset : public FontAssetBase
{
public:
    bool decode(SimpleArray<uint8_t>&, Factory*) override;
    std::string fileExtension() const override;
    const rcp<Font> font() const { return m_font; }
    void font(rcp<Font> font);

private:
    rcp<Font> m_font;
};
} // namespace rive

#endif
#ifndef _RIVE_FONT_HB_HPP_
#define _RIVE_FONT_HB_HPP_

#include "rive/factory.hpp"
#include "rive/text_engine.hpp"

#include <unordered_map>

struct hb_font_t;
struct hb_draw_funcs_t;
struct hb_feature_t;

class HBFont : public rive::Font
{
public:
    // We assume ownership of font!
    HBFont(hb_font_t* font);
    ~HBFont() override;

    Axis getAxis(uint16_t index) const override;
    uint16_t getAxisCount() const override;
    float getAxisValue(uint32_t axisTag) const override;
    uint32_t getFeatureValue(uint32_t featureTag) const override;

    rive::RawPath getPath(rive::GlyphID) const override;
    rive::SimpleArray<rive::Paragraph> onShapeText(rive::Span<const rive::Unichar>,
                                                   rive::Span<const rive::TextRun>) const override;
    rive::SimpleArray<uint32_t> features() const override;
    rive::rcp<Font> withOptions(rive::Span<const Coord> variableAxes,
                                rive::Span<const Feature> features) const override;

    bool hasGlyph(rive::Span<const rive::Unichar>) const override;

    static rive::rcp<rive::Font> Decode(rive::Span<const uint8_t>);
    hb_font_t* font() const { return m_font; }

private:
    HBFont(hb_font_t* font,
           std::unordered_map<uint32_t, float> axisValues,
           std::unordered_map<uint32_t, uint32_t> featureValues,
           std::vector<hb_feature_t> features);

public:
    hb_font_t* m_font;

    // The features list to pass directly to Harfbuzz.
    std::vector<hb_feature_t> m_features;

private:
    hb_draw_funcs_t* m_drawFuncs;

    // Feature value lookup based on tag.
    std::unordered_map<uint32_t, uint32_t> m_featureValues;

    // Axis value lookup based on for the feature.
    std::unordered_map<uint32_t, float> m_axisValues;
};

#endif

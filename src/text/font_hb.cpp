/*
 * Copyright 2022 Rive
 */

#include "rive/text_engine.hpp"

#ifdef WITH_RIVE_TEXT
#include "rive/text/font_hb.hpp"

#include "rive/factory.hpp"
#include "rive/renderer_utils.hpp"

#include "hb.h"
#include "hb-ot.h"
#include <unordered_set>

extern "C"
{
#include "SheenBidi.h"
}

// Initialized to null. Client can set this to a callback.
rive::Font::FallbackProc rive::Font::gFallbackProc;

rive::rcp<rive::Font> HBFont::Decode(rive::Span<const uint8_t> span)
{
    auto blob = hb_blob_create_or_fail((const char*)span.data(),
                                       (unsigned)span.size(),
                                       HB_MEMORY_MODE_DUPLICATE,
                                       nullptr,
                                       nullptr);
    if (blob)
    {
        auto face = hb_face_create(blob, 0);
        hb_blob_destroy(blob);
        if (face)
        {
            auto font = hb_font_create(face);
            hb_face_destroy(face);
            if (font)
            {
                return rive::rcp<rive::Font>(new HBFont(font));
            }
        }
    }
    return nullptr;
}

//////////////

constexpr int kStdScale = 2048;
constexpr float gInvScale = 1.0f / kStdScale;

extern "C"
{
    static void rpath_move_to(hb_draw_funcs_t*,
                              void* rpath,
                              hb_draw_state_t*,
                              float x,
                              float y,
                              void*)
    {
        ((rive::RawPath*)rpath)->moveTo(x * gInvScale, -y * gInvScale);
    }
    static void rpath_line_to(hb_draw_funcs_t*,
                              void* rpath,
                              hb_draw_state_t*,
                              float x1,
                              float y1,
                              void*)
    {
        ((rive::RawPath*)rpath)->lineTo(x1 * gInvScale, -y1 * gInvScale);
    }
    static void rpath_quad_to(hb_draw_funcs_t*,
                              void* rpath,
                              hb_draw_state_t*,
                              float x1,
                              float y1,
                              float x2,
                              float y2,
                              void*)
    {
        ((rive::RawPath*)rpath)
            ->quadTo(x1 * gInvScale, -y1 * gInvScale, x2 * gInvScale, -y2 * gInvScale);
    }
    static void rpath_cubic_to(hb_draw_funcs_t*,
                               void* rpath,
                               hb_draw_state_t*,
                               float x1,
                               float y1,
                               float x2,
                               float y2,
                               float x3,
                               float y3,
                               void*)
    {
        ((rive::RawPath*)rpath)
            ->cubicTo(x1 * gInvScale,
                      -y1 * gInvScale,
                      x2 * gInvScale,
                      -y2 * gInvScale,
                      x3 * gInvScale,
                      -y3 * gInvScale);
    }
    static void rpath_close(hb_draw_funcs_t*, void* rpath, hb_draw_state_t*, void*)
    {
        ((rive::RawPath*)rpath)->close();
    }
}

static rive::Font::LineMetrics make_lmx(hb_font_t* font)
{
    // premable on font...
    hb_ot_font_set_funcs(font);
    hb_font_set_scale(font, kStdScale, kStdScale);

    hb_font_extents_t extents;
    hb_font_get_h_extents(font, &extents);
    return {-extents.ascender * gInvScale, -extents.descender * gInvScale};
}

HBFont::HBFont(hb_font_t* font) : HBFont(font, {}, {}, {}) {}

HBFont::HBFont(hb_font_t* font,
               std::unordered_map<hb_tag_t, float> axisValues,
               std::unordered_map<hb_tag_t, uint32_t> featureValues,
               std::vector<hb_feature_t> features) :
    Font(make_lmx(font)),
    m_font(font),
    m_features(features),
    m_featureValues(featureValues),
    m_axisValues(axisValues)
{
    m_drawFuncs = hb_draw_funcs_create();
    hb_draw_funcs_set_move_to_func(m_drawFuncs, rpath_move_to, nullptr, nullptr);
    hb_draw_funcs_set_line_to_func(m_drawFuncs, rpath_line_to, nullptr, nullptr);
    hb_draw_funcs_set_quadratic_to_func(m_drawFuncs, rpath_quad_to, nullptr, nullptr);
    hb_draw_funcs_set_cubic_to_func(m_drawFuncs, rpath_cubic_to, nullptr, nullptr);
    hb_draw_funcs_set_close_path_func(m_drawFuncs, rpath_close, nullptr, nullptr);
    hb_draw_funcs_make_immutable(m_drawFuncs);
}

HBFont::~HBFont()
{
    hb_draw_funcs_destroy(m_drawFuncs);
    hb_font_destroy(m_font);
}

static void fillLanguageFeatures(hb_face_t* face,
                                 hb_tag_t tag,
                                 uint32_t scriptIndex,
                                 uint32_t languageIndex,
                                 std::unordered_set<uint32_t>& features)
{
    auto featureCount = hb_ot_layout_language_get_feature_tags(face,
                                                               tag,
                                                               scriptIndex,
                                                               languageIndex,
                                                               0,
                                                               nullptr,
                                                               nullptr);
    auto featureTags = std::vector<hb_tag_t>(featureCount);
    hb_ot_layout_language_get_feature_tags(face,
                                           tag,
                                           scriptIndex,
                                           languageIndex,
                                           0,
                                           &featureCount,
                                           featureTags.data());

    for (auto featureTag : featureTags)
    {
        features.emplace(featureTag);
    }
}

static void fillFeatures(hb_face_t* face, hb_tag_t tag, std::unordered_set<uint32_t>& features)
{
    auto scriptCount = hb_ot_layout_table_get_script_tags(face, tag, 0, nullptr, nullptr);
    auto scripts = std::vector<hb_tag_t>(scriptCount);
    hb_ot_layout_table_get_script_tags(face, tag, 0, &scriptCount, scripts.data());
    for (uint32_t i = 0; i < scriptCount; ++i)
    {
        auto languageCount =
            hb_ot_layout_script_get_language_tags(face, tag, i, 0, nullptr, nullptr);

        if (languageCount > 0)
        {
            auto languages = std::vector<hb_tag_t>(languageCount);
            hb_ot_layout_script_get_language_tags(face,
                                                  tag,
                                                  i,
                                                  0,
                                                  &languageCount,
                                                  languages.data());

            for (uint32_t j = 0; j < languageCount; ++j)
            {
                fillLanguageFeatures(face, tag, i, j, features);
            }
        }
        else
        {
            fillLanguageFeatures(face, tag, i, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, features);
        }
    }
}

rive::SimpleArray<uint32_t> HBFont::features() const
{
    std::unordered_set<uint32_t> features;
    auto face = hb_font_get_face(m_font);
    fillFeatures(face, HB_OT_TAG_GSUB, features);
    fillFeatures(face, HB_OT_TAG_GPOS, features);

    rive::SimpleArray<uint32_t> result(features.size());
    uint32_t index = 0;
    for (auto tag : features)
    {
        result[index++] = tag;
    }
    return result;
}

rive::Font::Axis HBFont::getAxis(uint16_t index) const
{
    auto face = hb_font_get_face(m_font);
    assert(index < hb_ot_var_get_axis_count(face));
    unsigned n = 1;
    hb_ot_var_axis_info_t info;
    hb_ot_var_get_axis_infos(face, index, &n, &info);
    assert(n == 1);
    return {info.tag, info.min_value, info.default_value, info.max_value};
}

uint16_t HBFont::getAxisCount() const
{
    auto face = hb_font_get_face(m_font);
    return (uint16_t)hb_ot_var_get_axis_count(face);
}

float HBFont::getAxisValue(uint32_t axisTag) const
{
    auto itr = m_axisValues.find(axisTag);
    if (itr != m_axisValues.end())
    {
        return itr->second;
    }
    auto face = hb_font_get_face(m_font);

    // No value specified, we're using a default.
    uint32_t axisCount = hb_ot_var_get_axis_count(face);
    for (uint32_t i = 0; i < axisCount; ++i)
    {
        hb_ot_var_axis_info_t info;
        uint32_t n = 1;
        hb_ot_var_get_axis_infos(face, i, &n, &info);
        if (info.tag == axisTag)
        {
            return info.default_value;
        }
    }
    return 0.0f;
}

uint32_t HBFont::getFeatureValue(uint32_t featureTag) const
{
    auto itr = m_featureValues.find(featureTag);
    if (itr != m_featureValues.end())
    {
        return itr->second;
    }
    return (uint32_t)-1;
}

rive::rcp<rive::Font> HBFont::withOptions(rive::Span<const Coord> coords,
                                          rive::Span<const Feature> features) const
{
    // Merges previous options with current ones.
    std::unordered_map<hb_tag_t, float> axisValues = m_axisValues;
    for (size_t i = 0; i < coords.size(); ++i)
    {
        axisValues[coords[i].axis] = coords[i].value;
    }

    AutoSTArray<16, hb_variation_t> vars(axisValues.size());
    size_t i = 0;
    for (auto itr = axisValues.begin(); itr != axisValues.end(); itr++)
    {
        vars[i++] = {itr->first, itr->second};
    }

    auto font = hb_font_create_sub_font(m_font);
    hb_font_set_variations(font, vars.data(), (unsigned int)vars.size());
    std::vector<hb_feature_t> hbFeatures;
    std::unordered_map<hb_tag_t, uint32_t> featureValues = m_featureValues;
    for (auto feature : features)
    {
        featureValues[feature.tag] = feature.value;
    }
    for (auto itr = featureValues.begin(); itr != featureValues.end(); itr++)
    {
        hbFeatures.push_back(
            {itr->first, itr->second, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END});
    }

    return rive::rcp<rive::Font>(new HBFont(font, axisValues, featureValues, hbFeatures));
}

rive::RawPath HBFont::getPath(rive::GlyphID glyph) const
{
    rive::RawPath rpath;
    hb_font_draw_glyph(m_font, glyph, m_drawFuncs, &rpath);
    return rpath;
}

///////////////////////////////////////////////////////////

static rive::GlyphRun shape_run(const rive::Unichar text[],
                                const rive::TextRun& tr,
                                unsigned textOffset)
{
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf32(buf, text, tr.unicharCount, 0, tr.unicharCount);

    hb_buffer_set_direction(buf,
                            tr.dir == rive::TextDirection::rtl ? HB_DIRECTION_RTL
                                                               : HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, (hb_script_t)tr.script);
    hb_buffer_set_language(buf, hb_language_get_default());

    auto hbfont = (HBFont*)tr.font.get();

    hb_shape(hbfont->m_font,
             buf,
             hbfont->m_features.data(),
             (unsigned int)hbfont->m_features.size());

    unsigned int glyph_count;
    hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    // todo: check for missing glyphs, and perform font-substitution
    rive::GlyphRun gr(glyph_count);
    gr.font = tr.font;
    gr.size = tr.size;
    gr.lineHeight = tr.lineHeight;
    gr.letterSpacing = tr.letterSpacing;
    gr.styleId = tr.styleId;
    gr.dir = tr.dir;

    const float scale = tr.size / kStdScale;
    for (unsigned int i = 0; i < glyph_count; i++)
    {
        unsigned int index = tr.dir == rive::TextDirection::rtl ? glyph_count - 1 - i : i;
        gr.glyphs[i] = (uint16_t)glyph_info[index].codepoint;
        gr.textIndices[i] = textOffset + glyph_info[index].cluster;
        gr.advances[i] = gr.xpos[i] = glyph_pos[index].x_advance * scale + tr.letterSpacing;
        gr.offsets[i] =
            rive::Vec2D(glyph_pos[index].x_offset * scale, -glyph_pos[index].y_offset * scale);
    }
    gr.xpos[glyph_count] = 0; // so the next run can line up snug
    hb_buffer_destroy(buf);
    return gr;
}

static rive::GlyphRun extract_subset(const rive::GlyphRun& orig, size_t start, size_t end)
{
    auto count = end - start;
    rive::GlyphRun subset(rive::SimpleArray<rive::GlyphID>(&orig.glyphs[start], count),
                          rive::SimpleArray<uint32_t>(&orig.textIndices[start], count),
                          rive::SimpleArray<float>(&orig.advances[start], count),
                          rive::SimpleArray<float>(&orig.xpos[start], count + 1),
                          rive::SimpleArray<rive::Vec2D>(&orig.offsets[start], count));
    subset.font = std::move(orig.font);
    subset.size = orig.size;
    subset.lineHeight = orig.lineHeight;
    subset.letterSpacing = orig.letterSpacing;
    subset.dir = orig.dir;
    subset.xpos.back() = 0; // since we're now the end of a run
    subset.styleId = orig.styleId;

    return subset;
}

static void perform_fallback(rive::rcp<rive::Font> fallbackFont,
                             rive::SimpleArrayBuilder<rive::GlyphRun>& gruns,
                             const rive::Unichar text[],
                             const rive::GlyphRun& orig,
                             const rive::TextRun& origTextRun)
{
    assert(orig.glyphs.size() > 0);

    const size_t count = orig.glyphs.size();
    size_t startI = 0;
    while (startI < count)
    {
        size_t endI = startI + 1;
        if (orig.glyphs[startI] == 0)
        {
            while (endI < count && orig.glyphs[endI] == 0)
            {
                ++endI;
            }
            auto textStart = orig.textIndices[startI];
            auto textCount = orig.textIndices[endI - 1] - textStart + 1;
            auto tr = rive::TextRun{
                fallbackFont,
                orig.size,
                orig.lineHeight,
                origTextRun.letterSpacing,
                textCount,
                origTextRun.script,
                orig.styleId,
                orig.dir,
            };
            auto gr = shape_run(&text[textStart], tr, textStart);
            if (gr.glyphs.size() > 0)
            {
                gruns.add(std::move(gr));
            }
        }
        else
        {
            while (endI < count && orig.glyphs[endI] != 0)
            {
                ++endI;
            }
            gruns.add(extract_subset(orig, startI, endI));
        }
        startI = endI;
    }
}

rive::SimpleArray<rive::Paragraph> HBFont::onShapeText(rive::Span<const rive::Unichar> text,
                                                       rive::Span<const rive::TextRun> truns) const
{

    rive::SimpleArrayBuilder<rive::Paragraph> paragraphs;
    SBCodepointSequence codepointSequence = {SBStringEncodingUTF32,
                                             (void*)text.data(),
                                             text.size()};

    hb_unicode_funcs_t* ufuncs = hb_unicode_funcs_get_default();

    // Split runs by bidi types.
    uint32_t textIndex = 0;
    uint32_t runIndex = 0;
    uint32_t runStartTextIndex = 0;

    SBUInteger paragraphStart = 0;

    SBAlgorithmRef bidiAlgorithm = SBAlgorithmCreate(&codepointSequence);
    uint32_t unicharIndex = 0;
    uint32_t runTextIndex = 0;

    while (paragraphStart < text.size())
    {
        SBParagraphRef paragraph =
            SBAlgorithmCreateParagraph(bidiAlgorithm, paragraphStart, INT32_MAX, SBLevelDefaultLTR);
        SBUInteger paragraphLength = SBParagraphGetLength(paragraph);
        // Next iteration reads the next paragraph (if any remain).
        paragraphStart += paragraphLength;
        const SBLevel* bidiLevels = SBParagraphGetLevelsPtr(paragraph);
        SBLevel paragraphLevel = SBParagraphGetBaseLevel(paragraph);
        uint32_t paragraphTextIndex = 0;

        std::vector<rive::TextRun> bidiRuns;
        bidiRuns.reserve(truns.size());

        while (runIndex < truns.size())
        {
            const auto& tr = truns[runIndex];
            assert(tr.unicharCount != 0);
            SBLevel lastLevel = bidiLevels[paragraphTextIndex];
            hb_script_t lastScript = hb_unicode_script(ufuncs, text[textIndex]);
            rive::TextRun splitRun = {
                tr.font,
                tr.size,
                tr.lineHeight,
                tr.letterSpacing,
                tr.unicharCount - runTextIndex,
                (uint32_t)lastScript,
                tr.styleId,
                lastLevel & 1 ? rive::TextDirection::rtl : rive::TextDirection::ltr,
            };

            runStartTextIndex = textIndex;

            runTextIndex++;
            textIndex++;
            paragraphTextIndex++;
            bidiRuns.push_back(splitRun);

            while (runTextIndex < tr.unicharCount && paragraphTextIndex < paragraphLength)
            {
                hb_script_t script = hb_unicode_script(ufuncs, text[textIndex]);
                switch (script)
                {
                    case HB_SCRIPT_COMMON:
                    case HB_SCRIPT_INHERITED:
                        // Propagate last seen "real" script value.
                        script = lastScript;
                        break;
                    default:
                        break;
                }
                if (bidiLevels[paragraphTextIndex] != lastLevel || script != lastScript)
                {
                    lastScript = script;
                    auto& back = bidiRuns.back();
                    back.unicharCount = textIndex - runStartTextIndex;
                    lastLevel = bidiLevels[paragraphTextIndex];

                    rive::TextRun backRun = {
                        back.font,
                        back.size,
                        back.lineHeight,
                        back.letterSpacing,
                        tr.unicharCount - runTextIndex,
                        (uint32_t)script,
                        back.styleId,
                        lastLevel & 1 ? rive::TextDirection::rtl : rive::TextDirection::ltr,
                    };
                    runStartTextIndex = textIndex;
                    bidiRuns.push_back(backRun);
                }
                runTextIndex++;
                textIndex++;
                paragraphTextIndex++;
            }
            // Reached the end of the run?
            if (runTextIndex == tr.unicharCount)
            {
                runIndex++;
                runTextIndex = 0;
            }
            // We consumed the whole paragraph.
            if (paragraphTextIndex == paragraphLength)
            {
                // Close off the last run.
                auto& back = bidiRuns.back();
                back.unicharCount = textIndex - runStartTextIndex;
                break;
            }
        }

        rive::SimpleArrayBuilder<rive::GlyphRun> gruns(bidiRuns.size());

        for (const auto& tr : bidiRuns)
        {
            auto gr = shape_run(&text[unicharIndex], tr, unicharIndex);
            unicharIndex += tr.unicharCount;

            auto end = gr.glyphs.end();
            auto iter = std::find(gr.glyphs.begin(), end, 0);
            if (!gFallbackProc || iter == end)
            {
                if (gr.glyphs.size() > 0)
                {
                    gruns.add(std::move(gr));
                }
            }
            else
            {
                // found at least 1 zero in glyphs, so need to perform font-fallback
                size_t index = iter - gr.glyphs.begin();
                rive::Unichar missing = text[gr.textIndices[index]];
                // todo: consider sending more chars if that helps choose a font
                auto fallback = gFallbackProc({&missing, 1});
                if (fallback)
                {
                    perform_fallback(fallback, gruns, text.data(), gr, tr);
                }
                else if (gr.glyphs.size() > 0)
                {
                    gruns.add(std::move(gr)); // oh well, just keep the missing glyphs
                }
            }
        }

        // turn the advances we stored in xpos[] into actual x-positions
        // for logical order.
        float pos = 0;
        for (auto& gr : gruns)
        {
            for (auto& xp : gr.xpos)
            {
                float adv = xp;
                xp = pos;
                pos += adv;
            }
        }

        paragraphs.add({
            std::move(gruns),
            paragraphLevel & 1 ? rive::TextDirection::rtl : rive::TextDirection::ltr,
        });
        SBParagraphRelease(paragraph);
    }

    SBAlgorithmRelease(bidiAlgorithm);
    return paragraphs;
}

bool HBFont::hasGlyph(rive::Span<const rive::Unichar> missing) const
{
    hb_codepoint_t glyph;
    return !missing.empty() && hb_font_get_nominal_glyph(m_font, missing[0], &glyph);
}

#endif

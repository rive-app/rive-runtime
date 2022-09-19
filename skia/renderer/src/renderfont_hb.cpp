/*
 * Copyright 2022 Rive
 */

#include "rive/render_text.hpp"

#ifdef RIVE_TEXT
#include "renderfont_hb.hpp"

#include "rive/factory.hpp"
#include "renderer_utils.hpp"

#include "hb.h"
#include "hb-ot.h"

// Initialized to null. Client can set this to a callback.
HBRenderFont::FallbackProc HBRenderFont::gFallbackProc;

rive::rcp<rive::RenderFont> HBRenderFont::Decode(rive::Span<const uint8_t> span) {
    auto blob = hb_blob_create_or_fail((const char*)span.data(),
                                       (unsigned)span.size(),
                                       HB_MEMORY_MODE_DUPLICATE,
                                       nullptr,
                                       nullptr);
    if (blob) {
        auto face = hb_face_create(blob, 0);
        hb_blob_destroy(blob);
        if (face) {
            auto font = hb_font_create(face);
            hb_face_destroy(face);
            if (font) {
                return rive::rcp<rive::RenderFont>(new HBRenderFont(font));
            }
        }
    }
    return nullptr;
}

//////////////

constexpr int kStdScale = 2048;
constexpr float gInvScale = 1.0f / kStdScale;

extern "C" {
void rpath_move_to(hb_draw_funcs_t*, void* rpath, hb_draw_state_t*, float x, float y, void*) {
    ((rive::RawPath*)rpath)->moveTo(x * gInvScale, -y * gInvScale);
}
void rpath_line_to(hb_draw_funcs_t*, void* rpath, hb_draw_state_t*, float x1, float y1, void*) {
    ((rive::RawPath*)rpath)->lineTo(x1 * gInvScale, -y1 * gInvScale);
}
void rpath_quad_to(hb_draw_funcs_t*,
                   void* rpath,
                   hb_draw_state_t*,
                   float x1,
                   float y1,
                   float x2,
                   float y2,
                   void*) {
    ((rive::RawPath*)rpath)
        ->quadTo(x1 * gInvScale, -y1 * gInvScale, x2 * gInvScale, -y2 * gInvScale);
}
void rpath_cubic_to(hb_draw_funcs_t*,
                    void* rpath,
                    hb_draw_state_t*,
                    float x1,
                    float y1,
                    float x2,
                    float y2,
                    float x3,
                    float y3,
                    void*) {
    ((rive::RawPath*)rpath)
        ->cubicTo(x1 * gInvScale,
                  -y1 * gInvScale,
                  x2 * gInvScale,
                  -y2 * gInvScale,
                  x3 * gInvScale,
                  -y3 * gInvScale);
}
void rpath_close(hb_draw_funcs_t*, void* rpath, hb_draw_state_t*, void*) {
    ((rive::RawPath*)rpath)->close();
}
}

static rive::RenderFont::LineMetrics make_lmx(hb_font_t* font) {
    // premable on font...
    hb_ot_font_set_funcs(font);
    hb_font_set_scale(font, kStdScale, kStdScale);

    hb_font_extents_t extents;
    hb_font_get_h_extents(font, &extents);
    return {-extents.ascender * gInvScale, -extents.descender * gInvScale};
}

HBRenderFont::HBRenderFont(hb_font_t* font) :
    RenderFont(make_lmx(font)),
    m_Font(font) // we just take ownership, no need to call reference()
{
    m_DrawFuncs = hb_draw_funcs_create();
    hb_draw_funcs_set_move_to_func(m_DrawFuncs, rpath_move_to, nullptr, nullptr);
    hb_draw_funcs_set_line_to_func(m_DrawFuncs, rpath_line_to, nullptr, nullptr);
    hb_draw_funcs_set_quadratic_to_func(m_DrawFuncs, rpath_quad_to, nullptr, nullptr);
    hb_draw_funcs_set_cubic_to_func(m_DrawFuncs, rpath_cubic_to, nullptr, nullptr);
    hb_draw_funcs_set_close_path_func(m_DrawFuncs, rpath_close, nullptr, nullptr);
    hb_draw_funcs_make_immutable(m_DrawFuncs);
}

HBRenderFont::~HBRenderFont() {
    hb_draw_funcs_destroy(m_DrawFuncs);
    hb_font_destroy(m_Font);
}

std::vector<rive::RenderFont::Axis> HBRenderFont::getAxes() const {
    auto face = hb_font_get_face(m_Font);
    std::vector<rive::RenderFont::Axis> axes;

    const int count = hb_ot_var_get_axis_count(face);
    if (count > 0) {
        axes.resize(count);

        hb_ot_var_axis_info_t info;
        for (int i = 0; i < count; ++i) {
            unsigned n = 1;
            hb_ot_var_get_axis_infos(face, i, &n, &info);
            assert(n == 1);
            axes[i] = {info.tag, info.min_value, info.default_value, info.max_value};
            //       printf("[%d] %08X %g %g %g\n", i, info.tag, info.min_value, info.default_value,
            //       info.max_value);
        }
    }
    return axes;
}

std::vector<rive::RenderFont::Coord> HBRenderFont::getCoords() const {
    auto axes = this->getAxes();
    //  const int count = (int)axes.size();

    unsigned length;
    const float* values = hb_font_get_var_coords_design(m_Font, &length);

    std::vector<rive::RenderFont::Coord> coords(length);
    for (unsigned i = 0; i < length; ++i) {
        coords[i] = {axes[i].tag, values[i]};
    }
    return coords;
}

rive::rcp<rive::RenderFont> HBRenderFont::makeAtCoords(rive::Span<const Coord> coords) const {
    AutoSTArray<16, hb_variation_t> vars(coords.size());
    for (size_t i = 0; i < coords.size(); ++i) {
        vars[i] = {coords[i].axis, coords[i].value};
    }
    auto font = hb_font_create_sub_font(m_Font);
    hb_font_set_variations(font, vars.data(), vars.size());
    return rive::rcp<rive::RenderFont>(new HBRenderFont(font));
}

rive::RawPath HBRenderFont::getPath(rive::GlyphID glyph) const {
    rive::RawPath rpath;
    hb_font_get_glyph_shape(m_Font, glyph, m_DrawFuncs, &rpath);
    return rpath;
}

///////////////////////////////////////////////////////////

const hb_feature_t gFeatures[] = {
    {'liga', 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END},
    {'dlig', 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END},
    {'kern', 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END},
};
constexpr int gNumFeatures = sizeof(gFeatures) / sizeof(gFeatures[0]);

static rive::RenderGlyphRun shape_run(const rive::Unichar text[],
                                      const rive::RenderTextRun& tr,
                                      unsigned textOffset) {
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf32(buf, text, tr.unicharCount, 0, tr.unicharCount);

    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    auto hbfont = (HBRenderFont*)tr.font.get();
    hb_shape(hbfont->m_Font, buf, gFeatures, gNumFeatures);

    unsigned int glyph_count;
    hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    // todo: check for missing glyphs, and perform font-substitution

    rive::RenderGlyphRun gr(glyph_count);
    gr.font = tr.font;
    gr.size = tr.size;

    const float scale = tr.size / kStdScale;
    for (unsigned int i = 0; i < glyph_count; i++) {
        //            hb_position_t x_offset  = glyph_pos[i].x_offset;
        //            hb_position_t y_offset  = glyph_pos[i].y_offset;

        gr.glyphs[i] = (uint16_t)glyph_info[i].codepoint;
        gr.textOffsets[i] = textOffset + glyph_info[i].cluster;
        gr.xpos[i] = glyph_pos[i].x_advance * scale;
    }
    gr.xpos[glyph_count] = 0; // so the next run can line up snug
    hb_buffer_destroy(buf);
    return gr;
}

static rive::RenderGlyphRun extract_subset(const rive::RenderGlyphRun& orig,
                                           size_t start,
                                           size_t end) {
    auto count = end - start;
    rive::RenderGlyphRun subset(rive::SimpleArray<rive::GlyphID>(&orig.glyphs[start], count),
                                rive::SimpleArray<uint32_t>(&orig.textOffsets[start], count),
                                rive::SimpleArray<float>(&orig.xpos[start], count));
    subset.font = std::move(orig.font);
    subset.size = orig.size;
    subset.xpos.back() = 0; // since we're now the end of a run

    return subset;
}

static void perform_fallback(rive::rcp<rive::RenderFont> fallbackFont,
                             rive::SimpleArrayBuilder<rive::RenderGlyphRun>& gruns,
                             const rive::Unichar text[],
                             const rive::RenderGlyphRun& orig) {
    assert(orig.glyphs.size() > 0);

    const size_t count = orig.glyphs.size();
    size_t startI = 0;
    while (startI < count) {
        size_t endI = startI + 1;
        if (orig.glyphs[startI] == 0) {
            while (endI < count && orig.glyphs[endI] == 0) {
                ++endI;
            }
            auto textStart = orig.textOffsets[startI];
            auto textCount = orig.textOffsets[endI - 1] - textStart + 1;
            auto tr = rive::RenderTextRun{fallbackFont, orig.size, textCount};
            gruns.add(shape_run(&text[textStart], tr, textStart));
        } else {
            while (endI < count && orig.glyphs[endI] != 0) {
                ++endI;
            }
            gruns.add(extract_subset(orig, startI, endI));
        }
        startI = endI;
    }
}

rive::SimpleArray<rive::RenderGlyphRun>
HBRenderFont::onShapeText(rive::Span<const rive::Unichar> text,
                          rive::Span<const rive::RenderTextRun> truns) const {
    rive::SimpleArrayBuilder<rive::RenderGlyphRun> gruns(truns.size());

    /////////////////

    uint32_t unicharIndex = 0;
    for (const auto& tr : truns) {
        auto gr = shape_run(&text[unicharIndex], tr, unicharIndex);
        unicharIndex += tr.unicharCount;

        auto end = gr.glyphs.end();
        auto iter = std::find(gr.glyphs.begin(), end, 0);
        if (!gFallbackProc || iter == end) {
            gruns.add(std::move(gr));
        } else {
            // found at least 1 zero in glyphs, so need to perform font-fallback
            size_t index = iter - gr.glyphs.begin();
            rive::Unichar missing = text[gr.textOffsets[index]];
            // todo: consider sending more chars if that helps choose a font
            auto fallback = gFallbackProc({&missing, 1});
            if (fallback) {
                perform_fallback(fallback, gruns, text.data(), gr);
            } else {
                gruns.add(std::move(gr)); // oh well, just keep the missing glyphs
            }
        }
    }

    // now turn the advances (widths) we stored in xpos[] into actual x-positions
    float pos = 0;
    for (auto& gr : gruns) {
        for (auto& xp : gr.xpos) {
            float adv = xp;
            xp = pos;
            pos += adv;
        }
    }
    return std::move(gruns);
}

#endif
/*
 * Copyright 2022 Rive
 */

#include "renderfont_hb.hpp"

#include "rive/factory.hpp"
#include "rive/render_text.hpp"
#include "renderer_utils.hpp"

#include "hb.h"
#include "hb-ot.h"

rive::rcp<rive::RenderFont> HBRenderFont::Decode(rive::Span<const uint8_t> span) {
    auto blob = hb_blob_create_or_fail((const char*)span.data(), (unsigned)span.size(),
                                             HB_MEMORY_MODE_DUPLICATE, nullptr, nullptr);
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
void rpath_quad_to(hb_draw_funcs_t*, void* rpath, hb_draw_state_t*, float x1, float y1, float x2, float y2, void*) {
    ((rive::RawPath*)rpath)->quadTo(x1 * gInvScale, -y1 * gInvScale,
                                    x2 * gInvScale, -y2 * gInvScale);
}
void rpath_cubic_to(hb_draw_funcs_t*, void* rpath, hb_draw_state_t*,
                    float x1, float y1, float x2, float y2, float x3, float y3, void*) {
    ((rive::RawPath*)rpath)->cubicTo(x1 * gInvScale, -y1 * gInvScale,
                                     x2 * gInvScale, -y2 * gInvScale,
                                     x3 * gInvScale, -y3 * gInvScale);
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
    m_Font(font)    // we just take ownership, no need to call reference()
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
            axes[i] = { info.tag, info.min_value, info.default_value, info.max_value };
     //       printf("[%d] %08X %g %g %g\n", i, info.tag, info.min_value, info.default_value, info.max_value);
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
        coords[i] = { axes[i].tag, values[i] };
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

std::vector<rive::RenderGlyphRun>
HBRenderFont::onShapeText(rive::Span<const rive::Unichar> text,
                          rive::Span<const rive::RenderTextRun> truns) const {
    std::vector<rive::RenderGlyphRun> gruns;
    gruns.reserve(truns.size());

    /////////////////

    const hb_feature_t features[] = {
        { 'liga', 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
        { 'dlig', 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
        { 'kern', 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END },
    };
    constexpr int numFeatures = sizeof(features) / sizeof(features[0]);

    uint32_t unicharIndex = 0;
    rive::Vec2D origin = {0, 0};
    for (const auto& tr : truns) {
        hb_buffer_t *buf = hb_buffer_create();
        hb_buffer_add_utf32(buf, (const uint32_t*)&text[unicharIndex], tr.unicharCount, 0, tr.unicharCount);

        hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
        hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
        hb_buffer_set_language(buf, hb_language_from_string("en", -1));
    
        auto hbfont = (HBRenderFont*)tr.font.get();
        hb_shape(hbfont->m_Font, buf, features, numFeatures);
    
        unsigned int glyph_count;
        hb_glyph_info_t *glyph_info    = hb_buffer_get_glyph_infos(buf, &glyph_count);
        hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

        // todo: check for missing glyphs, and perform font-substitution

        rive::RenderGlyphRun gr;
        gr.font = tr.font;
        gr.size = tr.size;
        gr.glyphs.resize(glyph_count);
        gr.textOffsets.resize(glyph_count);
        gr.xpos.resize(glyph_count + 1);

        const float scale = tr.size / kStdScale;

        for (unsigned int i = 0; i < glyph_count; i++) {
//            hb_position_t x_offset  = glyph_pos[i].x_offset;
//            hb_position_t y_offset  = glyph_pos[i].y_offset;

            gr.glyphs[i] = (uint16_t)glyph_info[i].codepoint;
            gr.textOffsets[i] = unicharIndex + glyph_info[i].cluster;
            gr.xpos[i] = origin.x;

            origin.x += glyph_pos[i].x_advance * scale;
        }
        gr.xpos[glyph_count] = origin.x;
        gruns.push_back(std::move(gr));

        unicharIndex += tr.unicharCount;
        hb_buffer_destroy(buf);
    }

    return gruns;
}

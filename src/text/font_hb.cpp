/*
 * Copyright 2022 Rive
 */

#include "rive/text_engine.hpp"

#ifdef WITH_RIVE_TEXT
#include "rive/text/font_hb.hpp"

#include "rive/factory.hpp"
#include "rive/renderer_utils.hpp"
#include "rive/shapes/paint/color.hpp"

#include "hb.h"
#include "hb-ot.h"
#include <unordered_set>

extern "C"
{
#include "SheenBidi.h"
}

// Initialized to null. Client can set this to a callback.
rive::Font::FallbackProc rive::Font::gFallbackProc;

bool rive::Font::gFallbackProcEnabled = true;

rive::rcp<rive::Font> HBFont::Decode(rive::Span<const uint8_t> span)
{
    auto blob = hb_blob_create_or_fail((const char*)span.data(),
                                       (unsigned)span.size(),
                                       HB_MEMORY_MODE_DUPLICATE,
                                       nullptr,
                                       nullptr);
    if (blob)
    {
        auto face = hb_face_create_or_fail(blob, 0);
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

#if defined(RIVE_NO_CORETEXT) || !defined(__APPLE__)
rive::rcp<rive::Font> HBFont::FromSystem(void* systemFont,
                                         bool useSystemShaper,
                                         uint16_t weight,
                                         uint8_t width)
{
    return nullptr;
}
#endif

float HBFont::GetStyle(hb_font_t* font, uint32_t styleTag)
{
    return hb_style_get_value(font, (hb_style_tag_t)styleTag);
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
            ->quadToCubic(x1 * gInvScale,
                          -y1 * gInvScale,
                          x2 * gInvScale,
                          -y2 * gInvScale);
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
    static void rpath_close(hb_draw_funcs_t*,
                            void* rpath,
                            hb_draw_state_t*,
                            void*)
    {
        ((rive::RawPath*)rpath)->close();
    }
}

// Convert HarfBuzz color (BGRA byte order) to Rive ColorInt (ARGB).
static rive::ColorInt hbColorToColorInt(hb_color_t c)
{
    uint8_t b = hb_color_get_blue(c);
    uint8_t g = hb_color_get_green(c);
    uint8_t r = hb_color_get_red(c);
    uint8_t a = hb_color_get_alpha(c);
    return rive::colorARGB(a, r, g, b);
}

// ── COLRv1 paint callbacks ──────────────────────────────────────────────
// We flatten the COLRv1 paint tree into a list of (path, fill) layers.

struct PaintState
{
    hb_font_t* font;
    hb_draw_funcs_t* drawFuncs;
    std::vector<rive::Font::ColorGlyphLayer>* layers;
    rive::GlyphID clipGlyph = 0;
    bool hasClip = false;
    rive::ColorInt foreground = 0xFF000000;

    // Transform stack for gradient coordinates.
    // HarfBuzz gives gradient coords in the local space of the paint;
    // transforms map them to glyph space.
    std::vector<rive::Mat2D> transformStack;

    void pushTransform(float xx,
                       float yx,
                       float xy,
                       float yy,
                       float dx,
                       float dy)
    {
        rive::Mat2D m;
        m[0] = xx;
        m[1] = yx;
        m[2] = xy;
        m[3] = yy;
        m[4] = dx * gInvScale;
        m[5] = -dy * gInvScale;
        if (transformStack.empty())
        {
            transformStack.push_back(m);
        }
        else
        {
            transformStack.push_back(transformStack.back() * m);
        }
    }

    void popTransform()
    {
        if (!transformStack.empty())
        {
            transformStack.pop_back();
        }
    }

    rive::Vec2D mapPoint(float x, float y) const
    {
        // Scale from HB font units to Rive glyph units, and flip Y.
        float rx = x * gInvScale;
        float ry = -y * gInvScale;
        if (!transformStack.empty())
        {
            // The transform stack already incorporates gInvScale + Y flip
            // in pushTransform, so just apply raw coords.
            const auto& m = transformStack.back();
            return rive::Vec2D(m[0] * x + m[2] * y + m[4],
                               m[1] * x + m[3] * y + m[5]);
        }
        return rive::Vec2D(rx, ry);
    }

    float mapRadius(float r) const
    {
        float scaled = r * gInvScale;
        if (!transformStack.empty())
        {
            // Use the geometric mean of the scale factors.
            const auto& m = transformStack.back();
            float sx = std::sqrt(m[0] * m[0] + m[1] * m[1]);
            return r * sx; // Don't double-apply gInvScale; it's in the matrix.
        }
        return scaled;
    }

    // Helper: extract color stops from hb_color_line_t.
    static std::vector<rive::Font::GradientStop> extractStops(
        hb_color_line_t* colorLine,
        rive::ColorInt foreground)
    {
        unsigned int count = 0;
        hb_color_line_get_color_stops(colorLine, 0, &count, nullptr);
        std::vector<hb_color_stop_t> hbStops(count);
        hb_color_line_get_color_stops(colorLine, 0, &count, hbStops.data());
        std::vector<rive::Font::GradientStop> stops;
        stops.reserve(count);
        for (auto& s : hbStops)
        {
            rive::ColorInt c =
                s.is_foreground ? foreground : hbColorToColorInt(s.color);
            stops.push_back({s.offset, c});
        }
        return stops;
    }

    // Helper: emit a layer with the current clip glyph path.
    rive::Font::ColorGlyphLayer makeClipLayer() const
    {
        rive::Font::ColorGlyphLayer layer;
        hb_font_draw_glyph(font, clipGlyph, drawFuncs, &layer.path);
        return layer;
    }
};

static void paint_push_transform(hb_paint_funcs_t*,
                                 void* paint_data,
                                 float xx,
                                 float yx,
                                 float xy,
                                 float yy,
                                 float dx,
                                 float dy,
                                 void*)
{
    ((PaintState*)paint_data)->pushTransform(xx, yx, xy, yy, dx, dy);
}

static void paint_pop_transform(hb_paint_funcs_t*, void* paint_data, void*)
{
    ((PaintState*)paint_data)->popTransform();
}

static void paint_push_clip_glyph(hb_paint_funcs_t*,
                                  void* paint_data,
                                  hb_codepoint_t glyph,
                                  hb_font_t*,
                                  void*)
{
    auto* state = (PaintState*)paint_data;
    state->clipGlyph = (rive::GlyphID)glyph;
    state->hasClip = true;
}

static void paint_push_clip_rectangle(hb_paint_funcs_t*,
                                      void*,
                                      float,
                                      float,
                                      float,
                                      float,
                                      void*)
{}

static void paint_pop_clip(hb_paint_funcs_t*, void* paint_data, void*)
{
    ((PaintState*)paint_data)->hasClip = false;
}

static void paint_solid(hb_paint_funcs_t*,
                        void* paint_data,
                        hb_bool_t is_foreground,
                        hb_color_t color,
                        void*)
{
    auto* state = (PaintState*)paint_data;
    if (!state->hasClip)
        return;

    auto layer = state->makeClipLayer();
    layer.paintType = rive::Font::ColorGlyphPaintType::solid;
    if (is_foreground)
    {
        layer.useForeground = true;
        layer.color = state->foreground;
    }
    else
    {
        layer.color = hbColorToColorInt(color);
    }
    state->layers->push_back(std::move(layer));
}

static void paint_linear_gradient(hb_paint_funcs_t*,
                                  void* paint_data,
                                  hb_color_line_t* colorLine,
                                  float x0,
                                  float y0,
                                  float x1,
                                  float y1,
                                  float x2,
                                  float y2,
                                  void*)
{
    auto* state = (PaintState*)paint_data;
    if (!state->hasClip)
        return;

    auto layer = state->makeClipLayer();
    layer.paintType = rive::Font::ColorGlyphPaintType::linearGradient;
    layer.stops = PaintState::extractStops(colorLine, state->foreground);

    // OpenType linear gradient uses 3 points: p0 (start), p1 (end), p2
    // (rotation). The gradient line goes from p0 toward p1, and p2 is used
    // to rotate the direction. For Rive we need a 2-point definition.
    // The actual gradient direction is: start = p0, end = p1 projected
    // along the p0->p2 direction. For simplicity, just use p0 and p1.
    auto sp0 = state->mapPoint(x0, y0);
    auto sp1 = state->mapPoint(x1, y1);
    layer.x0 = sp0.x;
    layer.y0 = sp0.y;
    layer.x1 = sp1.x;
    layer.y1 = sp1.y;

    state->layers->push_back(std::move(layer));
}

static void paint_radial_gradient(hb_paint_funcs_t*,
                                  void* paint_data,
                                  hb_color_line_t* colorLine,
                                  float x0,
                                  float y0,
                                  float radius0,
                                  float x1,
                                  float y1,
                                  float radius1,
                                  void*)
{
    auto* state = (PaintState*)paint_data;
    if (!state->hasClip)
        return;

    auto layer = state->makeClipLayer();
    layer.paintType = rive::Font::ColorGlyphPaintType::radialGradient;
    layer.stops = PaintState::extractStops(colorLine, state->foreground);

    auto sp0 = state->mapPoint(x0, y0);
    auto sp1 = state->mapPoint(x1, y1);
    layer.x0 = sp0.x;
    layer.y0 = sp0.y;
    layer.x1 = sp1.x;
    layer.y1 = sp1.y;
    layer.r0 = state->mapRadius(radius0);
    layer.r1 = state->mapRadius(radius1);

    state->layers->push_back(std::move(layer));
}

static void paint_sweep_gradient(hb_paint_funcs_t*,
                                 void* paint_data,
                                 hb_color_line_t* colorLine,
                                 float cx,
                                 float cy,
                                 float startAngle,
                                 float endAngle,
                                 void*)
{
    auto* state = (PaintState*)paint_data;
    if (!state->hasClip)
        return;

    auto layer = state->makeClipLayer();
    layer.paintType = rive::Font::ColorGlyphPaintType::sweepGradient;
    layer.stops = PaintState::extractStops(colorLine, state->foreground);

    auto sc = state->mapPoint(cx, cy);
    layer.x0 = sc.x;
    layer.y0 = sc.y;
    layer.startAngle = startAngle;
    layer.endAngle = endAngle;

    state->layers->push_back(std::move(layer));
}

static void paint_push_group(hb_paint_funcs_t*, void*, void*) {}
static void paint_pop_group(hb_paint_funcs_t*,
                            void*,
                            hb_paint_composite_mode_t,
                            void*)
{}

static hb_bool_t paint_color_glyph(hb_paint_funcs_t*,
                                   void*,
                                   hb_codepoint_t,
                                   hb_font_t*,
                                   void*)
{
    return false;
}

static hb_bool_t paint_image(hb_paint_funcs_t*,
                             void* paint_data,
                             hb_blob_t* blob,
                             unsigned int width,
                             unsigned int height,
                             hb_tag_t format,
                             float slant,
                             hb_glyph_extents_t* extents,
                             void*)
{
    // We only support PNG images (SBIX / CBDT).
    if (format != HB_TAG('p', 'n', 'g', ' '))
    {
        return false;
    }

    unsigned int length;
    const char* data = hb_blob_get_data(blob, &length);
    if (data == nullptr || length == 0)
    {
        return false;
    }

    auto* state = (PaintState*)paint_data;

    rive::Font::ColorGlyphLayer layer;
    layer.paintType = rive::Font::ColorGlyphPaintType::image;
    layer.imageBytes.assign(reinterpret_cast<const uint8_t*>(data),
                            reinterpret_cast<const uint8_t*>(data) + length);
    layer.imageWidth = width;
    layer.imageHeight = height;
    if (extents != nullptr)
    {
        layer.imageBearingX = extents->x_bearing * gInvScale;
        layer.imageBearingY = -extents->y_bearing * gInvScale;
        layer.imageExtentX = extents->width * gInvScale;
        layer.imageExtentY = -extents->height * gInvScale;
    }

    state->layers->push_back(std::move(layer));
    return true;
}

// ────────────────────────────────────────────────────────────────────────

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
    hb_draw_funcs_set_move_to_func(m_drawFuncs,
                                   rpath_move_to,
                                   nullptr,
                                   nullptr);
    hb_draw_funcs_set_line_to_func(m_drawFuncs,
                                   rpath_line_to,
                                   nullptr,
                                   nullptr);
    hb_draw_funcs_set_quadratic_to_func(m_drawFuncs,
                                        rpath_quad_to,
                                        nullptr,
                                        nullptr);
    hb_draw_funcs_set_cubic_to_func(m_drawFuncs,
                                    rpath_cubic_to,
                                    nullptr,
                                    nullptr);
    hb_draw_funcs_set_close_path_func(m_drawFuncs,
                                      rpath_close,
                                      nullptr,
                                      nullptr);
    hb_draw_funcs_make_immutable(m_drawFuncs);

    // Initialize COLRv1 paint funcs.
    m_paintFuncs = hb_paint_funcs_create();
    hb_paint_funcs_set_push_transform_func(m_paintFuncs,
                                           paint_push_transform,
                                           nullptr,
                                           nullptr);
    hb_paint_funcs_set_pop_transform_func(m_paintFuncs,
                                          paint_pop_transform,
                                          nullptr,
                                          nullptr);
    hb_paint_funcs_set_push_clip_glyph_func(m_paintFuncs,
                                            paint_push_clip_glyph,
                                            nullptr,
                                            nullptr);
    hb_paint_funcs_set_push_clip_rectangle_func(m_paintFuncs,
                                                paint_push_clip_rectangle,
                                                nullptr,
                                                nullptr);
    hb_paint_funcs_set_pop_clip_func(m_paintFuncs,
                                     paint_pop_clip,
                                     nullptr,
                                     nullptr);
    hb_paint_funcs_set_color_func(m_paintFuncs, paint_solid, nullptr, nullptr);
    hb_paint_funcs_set_push_group_func(m_paintFuncs,
                                       paint_push_group,
                                       nullptr,
                                       nullptr);
    hb_paint_funcs_set_pop_group_func(m_paintFuncs,
                                      paint_pop_group,
                                      nullptr,
                                      nullptr);
    hb_paint_funcs_set_color_glyph_func(m_paintFuncs,
                                        paint_color_glyph,
                                        nullptr,
                                        nullptr);
    hb_paint_funcs_set_linear_gradient_func(m_paintFuncs,
                                            paint_linear_gradient,
                                            nullptr,
                                            nullptr);
    hb_paint_funcs_set_radial_gradient_func(m_paintFuncs,
                                            paint_radial_gradient,
                                            nullptr,
                                            nullptr);
    hb_paint_funcs_set_sweep_gradient_func(m_paintFuncs,
                                           paint_sweep_gradient,
                                           nullptr,
                                           nullptr);
    hb_paint_funcs_set_image_func(m_paintFuncs, paint_image, nullptr, nullptr);
    hb_paint_funcs_make_immutable(m_paintFuncs);

    // Initialize color glyph (COLR/CPAL/SBIX/CBDT) support.
    hb_face_t* face = hb_font_get_face(font);
    m_hasColorLayers = hb_ot_color_has_layers(face);
    m_hasColorPaint = hb_ot_color_has_paint(face);
    m_hasPNG = hb_ot_color_has_png(face);
    if (m_hasColorLayers || m_hasColorPaint)
    {
        unsigned int colorCount = 0;
        hb_ot_color_palette_get_colors(face, 0, 0, &colorCount, nullptr);
        if (colorCount > 0)
        {
            m_paletteColors.resize(colorCount);
            hb_ot_color_palette_get_colors(face,
                                           0,
                                           0,
                                           &colorCount,
                                           m_paletteColors.data());
        }
    }
}

HBFont::~HBFont()
{
    hb_draw_funcs_destroy(m_drawFuncs);
    hb_paint_funcs_destroy(m_paintFuncs);
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

static void fillFeatures(hb_face_t* face,
                         hb_tag_t tag,
                         std::unordered_set<uint32_t>& features)
{
    auto scriptCount =
        hb_ot_layout_table_get_script_tags(face, tag, 0, nullptr, nullptr);
    auto scripts = std::vector<hb_tag_t>(scriptCount);
    hb_ot_layout_table_get_script_tags(face,
                                       tag,
                                       0,
                                       &scriptCount,
                                       scripts.data());
    for (uint32_t i = 0; i < scriptCount; ++i)
    {
        auto languageCount = hb_ot_layout_script_get_language_tags(face,
                                                                   tag,
                                                                   i,
                                                                   0,
                                                                   nullptr,
                                                                   nullptr);

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
            fillLanguageFeatures(face,
                                 tag,
                                 i,
                                 HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX,
                                 features);
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

uint16_t HBFont::getWeight() const
{
    uint32_t tag = HB_TAG('w', 'g', 'h', 't');
    float res = HBFont::GetStyle(m_font, tag);
    return static_cast<uint16_t>(res);
}

bool HBFont::isItalic() const
{
    uint32_t tag = HB_TAG('i', 't', 'a', 'l');
    float res = HBFont::GetStyle(m_font, tag);
    return res != 0.0;
}

rive::rcp<rive::Font> HBFont::withOptions(
    rive::Span<const Coord> coords,
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
        hbFeatures.push_back({itr->first,
                              itr->second,
                              HB_FEATURE_GLOBAL_START,
                              HB_FEATURE_GLOBAL_END});
    }

    return rive::rcp<rive::Font>(
        new HBFont(font, axisValues, featureValues, hbFeatures));
}

rive::RawPath HBFont::getPath(rive::GlyphID glyph) const
{
    rive::RawPath rpath;
    hb_font_draw_glyph(m_font, glyph, m_drawFuncs, &rpath);
    return rpath;
}

bool HBFont::hasColorGlyphs() const
{
    return m_hasColorLayers || m_hasColorPaint || m_hasPNG;
}

bool HBFont::isColorGlyph(rive::GlyphID glyph) const
{
    if (!m_hasColorLayers && !m_hasColorPaint && !m_hasPNG)
    {
        return false;
    }
    hb_face_t* face = hb_font_get_face(m_font);
    // Check COLRv0 layers first.
    if (m_hasColorLayers)
    {
        unsigned int total =
            hb_ot_color_glyph_get_layers(face, glyph, 0, nullptr, nullptr);
        if (total > 0)
        {
            return true;
        }
    }
    // Check COLRv1 paint.
    if (m_hasColorPaint)
    {
        if (hb_ot_color_glyph_has_paint(face, glyph))
        {
            return true;
        }
    }
    // Check SBIX/CBDT PNG.
    if (m_hasPNG)
    {
        hb_blob_t* blob = hb_ot_color_glyph_reference_png(m_font, glyph);
        if (blob != nullptr)
        {
            bool has = hb_blob_get_length(blob) > 0;
            hb_blob_destroy(blob);
            return has;
        }
    }
    return false;
}

size_t HBFont::getColorLayers(rive::GlyphID glyph,
                              std::vector<ColorGlyphLayer>& out,
                              rive::ColorInt foreground) const
{
    if (!m_hasColorLayers && !m_hasColorPaint && !m_hasPNG)
    {
        return 0;
    }

    // Check cache first.
    auto cacheIt = m_colorLayerCache.find(glyph);
    if (cacheIt != m_colorLayerCache.end())
    {
        // Copy from cache, but update foreground colors.
        for (const auto& cached : cacheIt->second)
        {
            ColorGlyphLayer layer;
            layer.paintType = cached.paintType;
            layer.path = cached.path;
            layer.useForeground = cached.useForeground;
            layer.color = cached.useForeground ? foreground : cached.color;
            // Copy gradient data if present.
            layer.stops = cached.stops;
            layer.x0 = cached.x0;
            layer.y0 = cached.y0;
            layer.x1 = cached.x1;
            layer.y1 = cached.y1;
            layer.r0 = cached.r0;
            layer.r1 = cached.r1;
            layer.startAngle = cached.startAngle;
            layer.endAngle = cached.endAngle;
            // Copy image data if present.
            layer.imageBytes = cached.imageBytes;
            layer.imageWidth = cached.imageWidth;
            layer.imageHeight = cached.imageHeight;
            layer.imageBearingX = cached.imageBearingX;
            layer.imageBearingY = cached.imageBearingY;
            layer.imageExtentX = cached.imageExtentX;
            layer.imageExtentY = cached.imageExtentY;
            out.push_back(std::move(layer));
        }
        return cacheIt->second.size();
    }

    std::vector<ColorGlyphLayer> layers;
    hb_face_t* face = hb_font_get_face(m_font);

    // Try COLRv0 first.
    if (m_hasColorLayers)
    {
        unsigned int layerCount =
            hb_ot_color_glyph_get_layers(face, glyph, 0, nullptr, nullptr);
        if (layerCount > 0)
        {
            std::vector<hb_ot_color_layer_t> hbLayers(layerCount);
            hb_ot_color_glyph_get_layers(face,
                                         glyph,
                                         0,
                                         &layerCount,
                                         hbLayers.data());

            layers.reserve(layerCount);
            for (unsigned int i = 0; i < layerCount; i++)
            {
                ColorGlyphLayer layer;
                hb_font_draw_glyph(m_font,
                                   hbLayers[i].glyph,
                                   m_drawFuncs,
                                   &layer.path);
                if (hbLayers[i].color_index == 0xFFFF)
                {
                    layer.useForeground = true;
                    layer.color = foreground;
                }
                else
                {
                    layer.useForeground = false;
                    if (hbLayers[i].color_index < m_paletteColors.size())
                    {
                        layer.color = hbColorToColorInt(
                            m_paletteColors[hbLayers[i].color_index]);
                    }
                    else
                    {
                        layer.color = 0xFF000000;
                    }
                }
                layers.push_back(std::move(layer));
            }
        }
    }

    // Fall back to COLRv1 paint if no COLRv0 layers found.
    if (layers.empty() && (m_hasColorPaint || m_hasPNG))
    {
        // hb_font_paint_glyph handles both COLRv1 and SBIX/CBDT.
        // For COLRv1 it calls paint_solid/gradient callbacks;
        // for SBIX/CBDT it calls paint_image.
        PaintState state;
        state.font = m_font;
        state.drawFuncs = m_drawFuncs;
        state.layers = &layers;
        state.foreground = foreground;

        hb_font_paint_glyph(m_font,
                            glyph,
                            m_paintFuncs,
                            &state,
                            0,                       // palette_index
                            HB_COLOR(0, 0, 0, 255)); // foreground
    }

    if (layers.empty())
    {
        return 0;
    }

    size_t count = layers.size();
    // Cache the result.
    m_colorLayerCache[glyph] = layers;
    // Move into output.
    for (auto& layer : layers)
    {
        out.push_back(std::move(layer));
    }
    return count;
}

///////////////////////////////////////////////////////////

static rive::GlyphRun shape_run(const rive::Unichar text[],
                                const rive::TextRun& tr,
                                unsigned textOffset)
{
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf32(buf, text, tr.unicharCount, 0, tr.unicharCount);

    hb_buffer_set_direction(buf,
                            tr.level & 1 ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, (hb_script_t)tr.script);
    hb_buffer_set_language(buf, hb_language_get_default());

    auto hbfont = (HBFont*)tr.font.get();

    hb_shape(hbfont->m_font,
             buf,
             hbfont->m_features.data(),
             (unsigned int)hbfont->m_features.size());

    unsigned int glyph_count;
    hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t* glyph_pos =
        hb_buffer_get_glyph_positions(buf, &glyph_count);

    // todo: check for missing glyphs, and perform font-substitution
    rive::GlyphRun gr(glyph_count);
    gr.font = tr.font;
    gr.size = tr.size;
    gr.lineHeight = tr.lineHeight;
    gr.letterSpacing = tr.letterSpacing;
    gr.styleId = tr.styleId;
    gr.level = tr.level;

    const float scale = tr.size / kStdScale;
    for (unsigned int i = 0; i < glyph_count; i++)
    {
        unsigned int index = tr.level & 1 ? glyph_count - 1 - i : i;
        gr.glyphs[i] = (uint16_t)glyph_info[index].codepoint;
        gr.textIndices[i] = textOffset + glyph_info[index].cluster;
        gr.advances[i] = gr.xpos[i] =
            glyph_pos[index].x_advance * scale + tr.letterSpacing;
        gr.offsets[i] = rive::Vec2D(glyph_pos[index].x_offset * scale,
                                    -glyph_pos[index].y_offset * scale);
    }
    gr.xpos[glyph_count] = 0; // so the next run can line up snug
    hb_buffer_destroy(buf);
    return gr;
}

static rive::GlyphRun extract_subset(const rive::GlyphRun& orig,
                                     size_t start,
                                     size_t end)
{
    auto count = end - start;
    rive::GlyphRun subset(
        rive::SimpleArray<rive::GlyphID>(&orig.glyphs[start], count),
        rive::SimpleArray<uint32_t>(&orig.textIndices[start], count),
        rive::SimpleArray<float>(&orig.advances[start], count),
        rive::SimpleArray<float>(&orig.xpos[start], count + 1),
        rive::SimpleArray<rive::Vec2D>(&orig.offsets[start], count));
    subset.font = std::move(orig.font);
    subset.size = orig.size;
    subset.lineHeight = orig.lineHeight;
    subset.letterSpacing = orig.letterSpacing;
    subset.level = orig.level;
    subset.xpos.back() = 0; // since we're now the end of a run
    subset.styleId = orig.styleId;

    return subset;
}

static void perform_fallback(rive::rcp<rive::Font> fallbackFont,
                             rive::SimpleArrayBuilder<rive::GlyphRun>& gruns,
                             const rive::Unichar text[],
                             const rive::GlyphRun& orig,
                             const rive::TextRun& origTextRun,
                             const uint32_t fallbackIndex)
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
            auto textCount =
                endI == count
                    ? origTextRun.unicharCount -
                          (orig.textIndices[startI] - orig.textIndices[0])
                    : orig.textIndices[endI] - textStart;

            auto tr = rive::TextRun{
                fallbackFont,
                orig.size,
                orig.lineHeight,
                origTextRun.letterSpacing,
                textCount,
                origTextRun.script,
                orig.styleId,
                orig.level,
            };

            static_cast<HBFont*>(fallbackFont.get())
                ->shapeFallbackRun(gruns,
                                   text,
                                   textStart,
                                   tr,
                                   origTextRun,
                                   fallbackIndex);
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

void HBFont::shapeFallbackRun(rive::SimpleArrayBuilder<rive::GlyphRun>& gruns,
                              const rive::Unichar text[],
                              const unsigned textStart,
                              const rive::TextRun& textRun,
                              const rive::TextRun& originalTextRun,
                              const uint32_t fallbackIndex)
{
    auto gr = shape_run(&text[textStart], textRun, textStart);
    auto end = gr.glyphs.end();
    auto iter = std::find(gr.glyphs.begin(), end, 0);
    if (iter == end)
    {
        if (gr.glyphs.size() > 0)
        {
            gruns.add(std::move(gr));
        }
    }
    else
    {
        // found at least 1 zero in glyphs, so need to perform
        // font-fallback
        size_t index = iter - gr.glyphs.begin();
        rive::Unichar missing = text[gr.textIndices[index]];
        auto fallback = HBFont::gFallbackProc(missing, fallbackIndex, this);
        if (fallback && fallback.get() != this)
        {
            perform_fallback(fallback,
                             gruns,
                             text,
                             gr,
                             textRun,
                             fallbackIndex + 1);
        }
        else if (gr.glyphs.size() > 0)
        {
            gruns.add(std::move(gr));
        }
    }
}

rive::SimpleArray<rive::Paragraph> HBFont::onShapeText(
    rive::Span<const rive::Unichar> text,
    rive::Span<const rive::TextRun> truns,
    int textDirectionFlag) const
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

    SBLevel defaultLevel;
    switch (textDirectionFlag)
    {
        case 0:
            defaultLevel = 0;
            break;
        case 1:
            defaultLevel = 1;
            break;
        default:
            defaultLevel = SBLevelDefaultLTR;
            break;
    }

    while (paragraphStart < text.size())
    {
        SBParagraphRef paragraph = SBAlgorithmCreateParagraph(bidiAlgorithm,
                                                              paragraphStart,
                                                              INT32_MAX,
                                                              defaultLevel);
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
            auto point = text[textIndex];
            hb_script_t lastScript = hb_unicode_script(ufuncs, point);

            rive::TextRun splitRun = {
                tr.font,
                tr.size,
                tr.lineHeight,
                tr.letterSpacing,
                tr.unicharCount - runTextIndex,
                (uint32_t)lastScript,
                tr.styleId,
                (uint8_t)lastLevel,
            };

            runStartTextIndex = textIndex;

            runTextIndex++;
            textIndex++;
            paragraphTextIndex++;
            bidiRuns.push_back(splitRun);

            while (runTextIndex < tr.unicharCount &&
                   paragraphTextIndex < paragraphLength)
            {
                auto point = text[textIndex];
                hb_script_t script =
                    hb_unicode_general_category(ufuncs, point) ==
                            HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK
                        ? HB_SCRIPT_INHERITED
                        : hb_unicode_script(ufuncs, point);

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
                if (bidiLevels[paragraphTextIndex] != lastLevel ||
                    script != lastScript)
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
                        (uint8_t)lastLevel,
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
            if (!gFallbackProc || iter == end || !gFallbackProcEnabled)
            {
                if (gr.glyphs.size() > 0)
                {
                    gruns.add(std::move(gr));
                }
            }
            else
            {
                // found at least 1 zero in glyphs, so need to perform
                // font-fallback
                size_t index = iter - gr.glyphs.begin();
                rive::Unichar missing = text[gr.textIndices[index]];
                // todo: consider sending more chars if that helps choose a font
                auto fallback = gFallbackProc(missing, 0, this);
                if (fallback)
                {
                    perform_fallback(fallback, gruns, text.data(), gr, tr, 1);
                }
                else if (gr.glyphs.size() > 0)
                {
                    // oh well, just keep the missing glyphs
                    gruns.add(std::move(gr));
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
            (uint8_t)paragraphLevel,
        });
        SBParagraphRelease(paragraph);
    }

    SBAlgorithmRelease(bidiAlgorithm);
    return paragraphs;
}

bool HBFont::hasGlyph(const rive::Unichar missing) const
{
    hb_codepoint_t glyph;
    return hb_font_get_nominal_glyph(m_font, missing, &glyph);
}

#endif

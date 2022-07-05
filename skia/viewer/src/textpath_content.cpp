/*
 * Copyright 2022 Rive
 */

#include "viewer_content.hpp"

#include "rive/refcnt.hpp"
#include "rive/render_text.hpp"

#include "contour_measure.hpp"
#include "skia_factory.hpp"
#include "skia_renderer.hpp"
#include "line_breaker.hpp"
#include "to_skia.hpp"

using namespace rive;

using RenderFontTextRuns = std::vector<RenderTextRun>;
using RenderFontGlyphRuns = std::vector<RenderGlyphRun>;
using RenderFontFactory = rcp<RenderFont> (*)(const Span<const uint8_t>);

template <typename Handler>
void visit(const std::vector<RenderGlyphRun>& gruns, Vec2D origin, Handler proc) {
    for (const auto& gr : gruns) {
        for (size_t i = 0; i < gr.glyphs.size(); ++i) {
            auto path = gr.font->getPath(gr.glyphs[i]);
            auto mx = Mat2D::fromTranslate(origin.x + gr.xpos[i], origin.y)
                    * Mat2D::fromScale(gr.size, gr.size);
            path.transformInPlace(mx);
            proc(path);
        }
    }
}

static Vec2D ave(Vec2D a, Vec2D b) {
    return (a + b) * 0.5f;
}

static RawPath make_quad_path(Span<const Vec2D> pts) {
    const int N = pts.size();
    RawPath path;
    if (N >= 2) {
        path.move(pts[0]);
        if (N == 2) {
            path.line(pts[1]);
        } else if (N == 3) {
            path.quad(pts[1], pts[2]);
        } else {
            for (int i = 1; i < N - 2; ++i) {
                path.quad(pts[i], ave(pts[i], pts[i+1]));
            }
            path.quad(pts[N-2], pts[N-1]);
        }
    }
    return path;
}

////////////////////////////////////////////////////////////////////////////////////

#include "renderfont_skia.hpp"
#include "renderfont_hb.hpp"
#include "renderfont_coretext.hpp"

static SkiaFactory skiaFactory;

static std::unique_ptr<RenderPath> make_rpath(const RawPath& path) {
    return skiaFactory.makeRenderPath(path.points(), path.verbsU8(), FillRule::nonZero);
}

static void stroke_path(Renderer* renderer, const RawPath& path, float size, ColorInt color) {
    auto paint = skiaFactory.makeRenderPaint();
    paint->color(color);
    paint->thickness(size);
    paint->style(RenderPaintStyle::stroke);
    renderer->drawPath(make_rpath(path).get(), paint.get());
}

static void fill_rect(Renderer* renderer, const AABB& r, RenderPaint* paint) {
    RawPath rp;
    rp.addRect(r);
    renderer->drawPath(make_rpath(rp).get(), paint);
}

static void fill_point(Renderer* renderer, Vec2D p, float r, RenderPaint* paint) {
    fill_rect(renderer, {p.x - r, p.y - r, p.x + r, p.y + r}, paint);
}

typedef rcp<RenderFont> (*RenderFontFactory)(Span<const uint8_t>);

static RenderTextRun append(std::vector<Unichar>* unichars, rcp<RenderFont> font,
                            float size, const char text[]) {
    uint32_t n = 0;
    while (text[n]) {
        unichars->push_back(text[n]);   // todo: utf8 -> unichar
        n += 1;
    }
    return { std::move(font), size, n };
}

class TextPathContent : public ViewerContent {
    std::vector<Unichar> m_unichars;
    RenderFontGlyphRuns m_gruns;
    std::unique_ptr<RenderPaint> m_paint;

    std::vector<Vec2D> m_pathpts;
    Vec2D m_lastPt = {0,0};
    int m_trackingIndex = -1;
    Mat2D m_trans;

    float m_alignment = 0,
          m_scaleY = 1,
          m_offsetY = 0,
          m_windowWidth = 1,    // %
          m_windowOffset = 0;   // %

    RenderFontTextRuns make_truns(RenderFontFactory fact) {
        auto loader = [fact](const char filename[]) -> rcp<RenderFont> {
            auto bytes = ViewerContent::LoadFile(filename);
            if (bytes.size() == 0) {
                assert(false);
                return nullptr;
            }
            return fact(toSpan(bytes));
        };

        const char* fontFiles[] = {
            "../../test/assets/RobotoFlex.ttf",
            "../../test/assets/LibreBodoni-Italic-VariableFont_wght.ttf",
        };

        auto font0 = loader(fontFiles[0]);
        auto font1 = loader(fontFiles[1]);
        assert(font0);
        assert(font1);

        RenderFont::Coord c1 = {'wght', 100.f},
                          c2 = {'wght', 800.f};

        RenderFontTextRuns truns;

        truns.push_back(append(&m_unichars, font0->makeAtCoord(c2), 60, "U"));
        truns.push_back(append(&m_unichars, font0->makeAtCoord(c1), 30, "neasy"));
        truns.push_back(append(&m_unichars, font1, 30, " fits the crown"));
        truns.push_back(append(&m_unichars, font1->makeAtCoord(c1), 30, " that often"));
        truns.push_back(append(&m_unichars, font0, 30, " lies the head."));

        return truns;
    }

public:
    TextPathContent() {
        auto truns = this->make_truns(CoreTextRenderFont::Decode);
        m_gruns = truns[0].font->shapeText(toSpan(m_unichars), toSpan(truns));

        m_paint = skiaFactory.makeRenderPaint();
        m_paint->color(0xFFFFFFFF);

        m_pathpts.push_back({ 20, 300});
        m_pathpts.push_back({220, 100});
        m_pathpts.push_back({420, 500});
        m_pathpts.push_back({620, 100});
        m_pathpts.push_back({820, 300});

        m_trans = Mat2D::fromTranslate(200, 200) * Mat2D::fromScale(2, 2);
    }

    void draw_warp(Renderer* renderer, const RawPath& warp) {
        stroke_path(renderer, warp, 0.5, 0xFF00FF00);

        auto paint = skiaFactory.makeRenderPaint();
        paint->color(0xFF008800);
        const float r = 4;
        for (auto p : m_pathpts) {
            fill_point(renderer, p, r, paint.get());
        }
    }

    static size_t count_glyphs(const RenderFontGlyphRuns& gruns) {
        size_t n = 0;
        for (const auto& gr : gruns) {
            n += gr.glyphs.size();
        }
        return n;
    }

    void modify(float amount) {
        m_paint->color(0xFFFFFFFF); 
    }

    void draw(Renderer* renderer, const RenderFontGlyphRuns& gruns) {
        auto get_path = [this](const RenderGlyphRun& run, int index, float dx) {
            auto path = run.font->getPath(run.glyphs[index]);
            path.transformInPlace(Mat2D::fromTranslate(run.xpos[index] + dx, m_offsetY)
                                * Mat2D::fromScale(run.size, run.size * m_scaleY));
            return path;
        };

        renderer->save();
        renderer->transform(m_trans);

        RawPath warp = make_quad_path(toSpan(m_pathpts));
        this->draw_warp(renderer, warp);

        ContourMeasure meas(warp);
        const float warpLength = meas.length();
        const float textLength = gruns.back().xpos.back();
        const float offset = (warpLength - textLength) * m_alignment;

        const size_t glyphCount = count_glyphs(gruns);
        size_t glyphIndex = 0;
        float windowEnd = m_windowOffset + m_windowWidth;

        for (const auto& gr : gruns) {
            for (size_t i = 0; i < gr.glyphs.size(); ++i) {
                float percent = glyphIndex / (float)(glyphCount - 1);
                float amount = (percent >= m_windowOffset && percent <= windowEnd);

                float scaleY = m_scaleY;
                m_paint->color(0xFF666666);
                m_paint->style(RenderPaintStyle::fill);
                if (amount > 0) {
                    this->modify(amount);
                }

                auto path = meas.warp(get_path(gr, i, offset));
                renderer->drawPath(make_rpath(path).get(), m_paint.get());
                glyphIndex += 1;
                m_scaleY = scaleY;
            }
        }
        renderer->restore();
    }

    void handleDraw(SkCanvas* canvas, double) override {
        SkiaRenderer renderer(canvas);

       this->draw(&renderer, m_gruns);
    }

    void handlePointerMove(float x, float y) override {
        m_lastPt = m_trans.invertOrIdentity() * Vec2D{x, y};
        if (m_trackingIndex >= 0) {
            m_pathpts[m_trackingIndex] = m_lastPt;
        }
    }
    void handlePointerDown() override {
        auto close_to = [](Vec2D a, Vec2D b) {
            return Vec2D::distance(a, b) <= 10;
        };
        for (size_t i = 0; i < m_pathpts.size(); ++i) {
            if (close_to(m_lastPt, m_pathpts[i])) {
                m_trackingIndex = i;
                break;
            }
        }
    }

    void handlePointerUp() override {
        m_trackingIndex = -1;
    }

    void handleResize(int width, int height) override {}
    
    void handleImgui() override {
        ImGui::Begin("path", nullptr);
        ImGui::SliderFloat("Alignment", &m_alignment, -3, 4); 
        ImGui::SliderFloat("Scale Y", &m_scaleY, 0.25f, 3.0f); 
        ImGui::SliderFloat("Offset Y", &m_offsetY, -100, 100); 
        ImGui::SliderFloat("Window Offset", &m_windowOffset, -1.1f, 1.1f); 
        ImGui::SliderFloat("Window Width", &m_windowWidth, 0, 1.2f); 
        ImGui::End();
    }
};

std::unique_ptr<ViewerContent> ViewerContent::TextPath(const char filename[]) {
    return std::make_unique<TextPathContent>();
}

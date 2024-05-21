
/*
 * Copyright 2022 Rive
 */

#include "viewer/viewer_content.hpp"
#include "rive/text/utf.hpp"

#include "rive/math/raw_path.hpp"
#include "rive/refcnt.hpp"
#include "rive/factory.hpp"
#include "rive/text_engine.hpp"
#include "rive/math/contour_measure.hpp"

using namespace rive;
#if 0
using FontTextRuns = std::vector<TextRun>;
using FontGlyphRuns = rive::SimpleArray<GlyphRun>;
using FontFactory = rcp<Font> (*)(const Span<const uint8_t>);

template <typename Handler> void visit(const Span<GlyphRun>& gruns, Vec2D origin, Handler proc)
{
    for (const auto& gr : gruns)
    {
        for (size_t i = 0; i < gr.glyphs.size(); ++i)
        {
            auto path = gr.font->getPath(gr.glyphs[i]);
            auto mx = Mat2D::fromTranslate(origin.x + gr.xpos[i], origin.y) *
                      Mat2D::fromScale(gr.size, gr.size);
            path.transformInPlace(mx);
            proc(path);
        }
    }
}

static Vec2D ave(Vec2D a, Vec2D b) { return (a + b) * 0.5f; }

static RawPath make_quad_path(Span<const Vec2D> pts)
{
    const int N = pts.size();
    RawPath path;
    if (N >= 2)
    {
        path.move(pts[0]);
        if (N == 2)
        {
            path.line(pts[1]);
        }
        else if (N == 3)
        {
            path.quad(pts[1], pts[2]);
        }
        else
        {
            for (int i = 1; i < N - 2; ++i)
            {
                path.quad(pts[i], ave(pts[i], pts[i + 1]));
            }
            path.quad(pts[N - 2], pts[N - 1]);
        }
    }
    return path;
}

static void warp_in_place(ContourMeasure* meas, RawPath* path)
{
    for (auto& pt : path->points())
    {
        pt = meas->warp(pt);
    }
}

////////////////////////////////////////////////////////////////////////////////////

static rcp<RenderPath> make_rpath(RawPath& path)
{
    return ViewerContent::RiveFactory()->makeRenderPath(path, FillRule::nonZero);
}

static void stroke_path(Renderer* renderer, RawPath& path, float size, ColorInt color)
{
    auto paint = ViewerContent::RiveFactory()->makeRenderPaint();
    paint->color(color);
    paint->thickness(size);
    paint->style(RenderPaintStyle::stroke);
    renderer->drawPath(make_rpath(path).get(), paint.get());
}

static void fill_rect(Renderer* renderer, const AABB& r, RenderPaint* paint)
{
    RawPath rp;
    rp.addRect(r);
    renderer->drawPath(make_rpath(rp).get(), paint);
}

static void fill_point(Renderer* renderer, Vec2D p, float r, RenderPaint* paint)
{
    fill_rect(renderer, {p.x - r, p.y - r, p.x + r, p.y + r}, paint);
}

static TextRun append(std::vector<Unichar>* unichars, rcp<Font> font, float size, const char text[])
{
    const uint8_t* ptr = (const uint8_t*)text;
    uint32_t n = 0;
    while (*ptr)
    {
        unichars->push_back(rive::UTF::NextUTF8(&ptr));
        n += 1;
    }
    return {std::move(font), size, n};
}

class TextPathContent : public ViewerContent
{
    std::vector<Unichar> m_unichars;
    FontGlyphRuns m_gruns;
    rcp<RenderPaint> m_paint;
    AABB m_gbounds;

    std::vector<Vec2D> m_pathpts;
    Vec2D m_lastPt = {0, 0};
    int m_trackingIndex = -1;
    Mat2D m_trans;

    Mat2D m_oneLineXform;
    bool m_trackingOneLine = false;
    float m_oneLineX = 0;
    float m_flareRadius = 50;

    float m_alignment = 0, m_scaleY = 1, m_offsetY = 0,
          m_windowWidth = 1, // %
        m_windowOffset = 0;  // %

    FontTextRuns make_truns(FontFactory fact)
    {
        auto loader = [fact](const char filename[]) -> rcp<Font> {
            auto bytes = ViewerContent::LoadFile(filename);
            if (bytes.size() == 0)
            {
                assert(false);
                return nullptr;
            }
            return fact(bytes);
        };

        const char* fontFiles[] = {
            "../../../test/assets/RobotoFlex.ttf",
            "../../../test/assets/LibreBodoni-Italic-VariableFont_wght.ttf",
        };

        auto font0 = loader(fontFiles[0]);
        auto font1 = loader(fontFiles[1]);
        assert(font0);
        assert(font1);

        Font::Coord c1 = {'wght', 100.f}, c2 = {'wght', 800.f};

        FontTextRuns truns;

        truns.push_back(append(&m_unichars, font0->makeAtCoord(c2), 60, "U"));
        truns.push_back(append(&m_unichars, font0->makeAtCoord(c1), 30, "ne漢字asy"));
        truns.push_back(append(&m_unichars, font1, 30, " fits the crown"));
        truns.push_back(append(&m_unichars, font1->makeAtCoord(c1), 30, " that often"));
        truns.push_back(append(&m_unichars, font0, 30, " lies the head."));

        return truns;
    }

public:
    TextPathContent()
    {
        auto compute_bounds = [](const rive::SimpleArray<GlyphRun>& gruns) {
            AABB bounds = {};
            for (const auto& gr : gruns)
            {
                bounds.minY = std::min(bounds.minY, gr.font->lineMetrics().ascent * gr.size);
                bounds.maxY = std::max(bounds.maxY, gr.font->lineMetrics().descent * gr.size);
            }
            bounds.minX = gruns.front().xpos.front();
            bounds.maxX = gruns.back().xpos.back();
            printf("%g %g %g %g\n", bounds.left(), bounds.top(), bounds.right(), bounds.bottom());
            return bounds;
        };

        auto truns = this->make_truns(ViewerContent::DecodeFont);

        m_gruns = truns[0].font->shapeText(m_unichars, truns);

        m_gbounds = compute_bounds(m_gruns);
        m_oneLineXform = Mat2D::fromScale(2.5, 2.5) * Mat2D::fromTranslate(20, 80);

        m_paint = ViewerContent::RiveFactory()->makeRenderPaint();
        m_paint->color(0xFFFFFFFF);

        m_pathpts.push_back({20, 300});
        m_pathpts.push_back({220, 100});
        m_pathpts.push_back({420, 500});
        m_pathpts.push_back({620, 100});
        m_pathpts.push_back({820, 300});

        m_trans = Mat2D::fromTranslate(200, 200) * Mat2D::fromScale(2, 2);
    }

    void draw_warp(Renderer* renderer, RawPath& warp)
    {
        stroke_path(renderer, warp, 0.5, 0xFF00FF00);

        auto paint = ViewerContent::RiveFactory()->makeRenderPaint();
        paint->color(0xFF008800);
        const float r = 4;
        for (auto p : m_pathpts)
        {
            fill_point(renderer, p, r, paint.get());
        }
    }

    static size_t count_glyphs(const FontGlyphRuns& gruns)
    {
        size_t n = 0;
        for (const auto& gr : gruns)
        {
            n += gr.glyphs.size();
        }
        return n;
    }

    void modify(float amount) { m_paint->color(0xFFFFFFFF); }

    void draw(Renderer* renderer, const FontGlyphRuns& gruns)
    {
        auto get_path = [this](const GlyphRun& run, int index, float dx) {
            auto path = run.font->getPath(run.glyphs[index]);
            path.transformInPlace(Mat2D::fromTranslate(run.xpos[index] + dx, m_offsetY) *
                                  Mat2D::fromScale(run.size, run.size * m_scaleY));
            return path;
        };

        renderer->save();
        renderer->transform(m_trans);

        RawPath warp = make_quad_path(m_pathpts);
        this->draw_warp(renderer, warp);

        auto meas = ContourMeasureIter(&warp).next();

        const float warpLength = meas->length();
        const float textLength = gruns.back().xpos.back();
        const float offset = (warpLength - textLength) * m_alignment;

        const size_t glyphCount = count_glyphs(gruns);
        size_t glyphIndex = 0;
        float windowEnd = m_windowOffset + m_windowWidth;

        for (const auto& gr : gruns)
        {
            for (size_t i = 0; i < gr.glyphs.size(); ++i)
            {
                float percent = glyphIndex / (float)(glyphCount - 1);
                float amount = (percent >= m_windowOffset && percent <= windowEnd);

                float scaleY = m_scaleY;
                m_paint->color(0xFF666666);
                m_paint->style(RenderPaintStyle::fill);
                if (amount > 0)
                {
                    this->modify(amount);
                }

                auto path = get_path(gr, i, offset);
                warp_in_place(meas.get(), &path);
                renderer->drawPath(make_rpath(path).get(), m_paint.get());
                glyphIndex += 1;
                m_scaleY = scaleY;
            }
        }
        renderer->restore();
    }

    void drawOneLine(Renderer* renderer)
    {
        auto paint = ViewerContent::RiveFactory()->makeRenderPaint();
        paint->color(0xFF88FFFF);

        if (m_trackingOneLine)
        {
            float mx = m_oneLineX / m_gbounds.width();
            const ColorInt colors[] = {0xFF88FFFF, 0xFF88FFFF, 0xFFFFFFFF, 0xFF88FFFF, 0xFF88FFFF};
            const float stops[] = {0, mx / 2, mx, (1 + mx) / 2, 1};
            paint->shader(ViewerContent::RiveFactory()->makeLinearGradient(m_gbounds.left(),
                                                                           0,
                                                                           m_gbounds.right(),
                                                                           0,
                                                                           colors,
                                                                           stops,
                                                                           5));
        }

        struct EaseWindow
        {
            float center, radius;

            float map(float x) const
            {
                float dist = std::abs(center - x);
                if (dist > radius)
                {
                    return 0;
                }
                float t = (radius - dist) / radius;
                return t * t * (3 - 2 * t);
            }
        };

        auto wrap_path = [](const RawPath& src, float x, float rad) {
            return src.morph([x, rad](Vec2D p) {
                Vec2D newpt = p;
                newpt.y = p.y * 4 + 18;

                const float t = EaseWindow{x, rad}.map(p.x);
                return Vec2D::lerp(p, newpt, t);
            });
        };

        visit(m_gruns, {0, 0}, [&](RawPath& rp) {
            RawPath* ptr = &rp;
            RawPath storage;
            if (m_trackingOneLine)
            {
                storage = wrap_path(rp, m_oneLineX, m_flareRadius);
                ptr = &storage;
            }
            renderer->drawPath(make_rpath(*ptr).get(), paint.get());
        });
    }

    void handleDraw(rive::Renderer* renderer, double) override
    {
        renderer->save();
        this->draw(renderer, m_gruns);
        renderer->restore();

        renderer->save();
        renderer->transform(m_oneLineXform);
        this->drawOneLine(renderer);
        renderer->restore();
    }

    void handlePointerMove(float x, float y) override
    {
        auto contains = [](const AABB& r, Vec2D p) {
            return r.left() <= p.x && p.x < r.right() && r.top() <= p.y && p.y < r.bottom();
        };

        // are we on onLine?
        {
            m_trackingOneLine = false;
            auto pos = m_oneLineXform.invertOrIdentity() * Vec2D{x, y};
            if (contains(m_gbounds.inset(-8, 0), pos))
            {
                m_trackingOneLine = true;
                m_oneLineX = pos.x;
                return;
            }
        }

        // are we on the path?
        m_lastPt = m_trans.invertOrIdentity() * Vec2D{x, y};
        if (m_trackingIndex >= 0)
        {
            m_pathpts[m_trackingIndex] = m_lastPt;
        }
    }
    void handlePointerDown(float x, float y) override
    {
        auto close_to = [](Vec2D a, Vec2D b) { return Vec2D::distance(a, b) <= 10; };
        for (size_t i = 0; i < m_pathpts.size(); ++i)
        {
            if (close_to(m_lastPt, m_pathpts[i]))
            {
                m_trackingIndex = i;
                break;
            }
        }
    }

    void handlePointerUp(float x, float y) override { m_trackingIndex = -1; }

    void handleResize(int width, int height) override {}

#ifndef RIVE_SKIP_IMGUI
    void handleImgui() override
    {
        ImGui::Begin("path", nullptr);
        ImGui::SliderFloat("Alignment", &m_alignment, -3, 4);
        ImGui::SliderFloat("Scale Y", &m_scaleY, 0.25f, 3.0f);
        ImGui::SliderFloat("Offset Y", &m_offsetY, -100, 100);
        ImGui::SliderFloat("Window Offset", &m_windowOffset, -1.1f, 1.1f);
        ImGui::SliderFloat("Window Width", &m_windowWidth, 0, 1.2f);
        ImGui::SliderFloat("Flare radius", &m_flareRadius, 10, 100);
        ImGui::End();
    }
#endif
};

std::unique_ptr<ViewerContent> ViewerContent::TextPath(const char filename[])
{
    return rivestd::make_unique<TextPathContent>();
}
#else
std::unique_ptr<ViewerContent> ViewerContent::TextPath(const char filename[]) { return nullptr; }
#endif

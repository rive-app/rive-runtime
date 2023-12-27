/*
 * Copyright 2022 Rive
 */

#include "viewer/viewer_content.hpp"
#include "rive/factory.hpp"
#include "rive/renderer.hpp"
#include "rive/math/contour_measure.hpp"

using namespace rive;

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

static RawPath trim(ContourMeasure* cm, float startT, float endT)
{
    // start and end are 0...1
    auto startD = startT * cm->length();
    auto endD = endT * cm->length();
    if (startD > endD)
    {
        std::swap(startD, endD);
    }

    RawPath path;
    cm->getSegment(startD, endD, &path, true);
    return path;
}

class TrimPathContent : public ViewerContent
{
    std::vector<Vec2D> m_pathpts;
    int m_trackingIndex = -1;

    float m_trimFrom = 0, m_trimTo = 1;

public:
    TrimPathContent()
    {
        m_pathpts.push_back({20, 300});
        m_pathpts.push_back({220, 100});
        m_pathpts.push_back({420, 500});
        m_pathpts.push_back({620, 100});
        m_pathpts.push_back({820, 300});
    }

    void handleDraw(rive::Renderer* renderer, double) override
    {
        auto path = make_quad_path(m_pathpts);

        RawPath cubicpath;
        cubicpath.move(m_pathpts[0]);
        cubicpath.cubic(m_pathpts[1], m_pathpts[2], m_pathpts[3]);
        cubicpath.line(m_pathpts[4]);

        RawPath* ps[] = {&path, &cubicpath};

        renderer->save();
        for (auto p : ps)
        {
            renderer->save();

            auto cm = ContourMeasureIter(*p, false).next();
            auto p1 = trim(cm.get(), m_trimFrom, m_trimTo);
            stroke_path(renderer, p1, 20, 0xFFFF0000);

            stroke_path(renderer, *p, 4, 0xFFFFFFFF);

            renderer->restore();
            renderer->translate(0, 500);
        }
        renderer->restore();

        auto paint = ViewerContent::RiveFactory()->makeRenderPaint();
        paint->color(0xFF008800);
        const float r = 6;
        for (auto p : m_pathpts)
        {
            fill_point(renderer, p, r, paint.get());
        }
    }

    void handlePointerMove(float x, float y) override
    {
        if (m_trackingIndex >= 0)
        {
            m_pathpts[m_trackingIndex] = Vec2D{x, y};
        }
    }
    void handlePointerDown(float x, float y) override
    {
        auto pt = Vec2D{x, y};
        auto close_to = [](Vec2D a, Vec2D b) { return Vec2D::distance(a, b) <= 10; };
        for (size_t i = 0; i < m_pathpts.size(); ++i)
        {
            if (close_to(pt, m_pathpts[i]))
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
        ImGui::Begin("trim", nullptr);
        ImGui::SliderFloat("From", &m_trimFrom, 0, 1);
        ImGui::SliderFloat("To", &m_trimTo, 0, 1);
        ImGui::End();
    }
#endif
};

std::unique_ptr<ViewerContent> ViewerContent::TrimPath(const char[])
{
    return rivestd::make_unique<TrimPathContent>();
}

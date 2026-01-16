/*
 * Copyright 2024 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/bezier_utils.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/wangs_formula.hpp"

using namespace rivegm;

namespace rive::gpu
{
class FeatherGM : public GM
{
public:
    FeatherGM() : GM(1756, 2048)
    {
        m_paint = TestingWindow::Get()->factory()->makeRenderPaint();
        m_paint->color(0xffffffff);
    }

    ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(Renderer* renderer) override
    {
        renderer->scale(1.463f, 1.463f);
        for (int y = 0; y < 7; ++y)
        {
            renderer->save();
            for (int x = 0; x < 6; ++x)
            {
                renderer->save();
                renderer->translate(50, 50);
                // For the y=0 case, this checks that epsilon size feathers
                // generate smooth AA.
                m_paint->feather(expf(y));
                drawCell(renderer, x, y, m_paint.get());
                renderer->restore();
                renderer->translate(200, 0);
            }
            renderer->restore();
            renderer->translate(0, 200);
        }
    }

private:
    virtual void drawCell(Renderer*, int x, int y, RenderPaint*) = 0;

    rcp<RenderPaint> m_paint;
};

// Check that basic shapes feather correctly (enough).
class FeatherShapesGM : public FeatherGM
{
public:
    void onOnceBeforeDraw() override
    {
        m_shapes.reserve(6);

        auto& square = m_shapes.emplace_back(makeShape());
        square->addRect(0, 0, 100, 100);

        auto& circle = m_shapes.emplace_back(makeShape());
        path_addOval(circle.get(),
                     AABB{0, 0, 100, 100},
                     rivegm::PathDirection::clockwise);

        auto shark = makeShape();
        shark->moveTo(376, 1422);
        shark->cubicTo(774, 526, 60, 660, 398, 329);
        shark->cubicTo(639.333374f,
                       149.666656f,
                       905.333313f,
                       258.666656f,
                       1196,
                       656);
        shark->cubicTo(686, 460, 686, 660, 1370, 1006);
        // "cuspy" flat line with control points outside T=0..1.
        shark->cubicTo(1701.333374f,
                       867.333374f,
                       44.666626f,
                       1560.666626f,
                       376,
                       1422);
        m_shapes.emplace_back(makeShape())
            ->addRenderPath(shark.get(),
                            Mat2D().scale({.11f, .11f}).translate({-40, -38}));

        auto& cusp = m_shapes.emplace_back(makeShape());
        cusp->lineTo(100, 0);
        cusp->cubicTo(0, 100, 0, 0, 100, 100);
        cusp->lineTo(0, 100);
        cusp->cubicTo(50, 67, -50, 33, 0, 0);

        float r = 40;
        auto& rrect = m_shapes.emplace_back(makeShape());
        rrect->moveTo(r, 0);
        rrect->lineTo(100 - r, 0);
        rrect->cubicTo(100 - r / 2, 0, 100, r / 2, 100, r);
        rrect->lineTo(100, 100 - r);
        rrect->cubicTo(100, 100 - r / 5, 100 - r / 5, 100, 100 - r, 100);
        rrect->lineTo(r, 100);
        rrect->cubicTo(0 + r / 3, 100, 0, 100 - r / 3, 0, 100 - r);
        rrect->lineTo(0, r);
        rrect->cubicTo(0, 0, 0, 0, r, 0);

        auto& irrect = m_shapes.emplace_back(makeShape());
        irrect->addRenderPath(square.get(), Mat2D());
        irrect->addRenderPath(rrect.get(),
                              Mat2D(-60.f / 100, 0, 0, 60.f / 100, 80, 20));
    }

protected:
    class Shape : public RenderPath
    {
    public:
        Shape() : m_path(FillRule::clockwise) {}

        RenderPath* renderPath() override { return m_path.get(); }
        void rewind() override { m_path = Path(); }
        void fillRule(FillRule value) override { m_path->fillRule(value); }
        void moveTo(float x, float y) override
        {
            m_path->moveTo(x, y);
            m_begin = m_pen = {x, y};
        }
        void lineTo(float x, float y) override
        {
            m_path->lineTo(x, y);
            m_pen = {x, y};
        }
        void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
            override
        {
            m_path->cubicTo(ox, oy, ix, iy, x, y);
            m_pen = {x, y};
        }
        void close() override
        {
            m_path->close();
            m_pen = m_begin;
        }
        void addRenderPath(RenderPath* path, const Mat2D& transform) override
        {
            auto shape = static_cast<Shape*>(path);
            m_path->addRenderPath(shape->renderPath(), transform);
            m_pen = shape->m_pen;
            m_begin = shape->m_begin;
        }

        void addRawPath(const RawPath& path) override
        {
            m_path->addRawPath(path);
        }

    protected:
        Path m_path;
        Vec2D m_pen = {0, 0};
        Vec2D m_begin = {0, 0};
    };

    virtual std::unique_ptr<Shape> makeShape()
    {
        return std::make_unique<Shape>();
    }

    void drawCell(Renderer* renderer, int x, int y, RenderPaint* paint) override
    {
        renderer->drawPath(m_shapes[x]->renderPath(), paint);
    }

    std::vector<std::unique_ptr<Shape>> m_shapes;
};
GMREGISTER(feather_shapes, return new FeatherShapesGM)

// Validate corners by tessellating shapes into polygons and then feathering
// them.
class FeatherPolyShapesGM : public FeatherShapesGM
{
private:
    class PolyShape : public Shape
    {
    public:
        void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
            override
        {
            Vec2D p[4] = {m_pen, {ox, oy}, {ix, iy}, {x, y}};
            int n = wangs_formula::cubic(p, 8);
            n = std::max(n, 3);
            n = (n & ~1) + 1;
            math::EvalCubic ec(p);
            float dt = 2.f / n;
            float4 t = dt * float4{.5f, .5f, 1, 1};
            for (int i = 1; i < n; i += 2, t += dt)
            {
                float4 result = ec(t);
                Shape::lineTo(result.x, result.y);
                Shape::lineTo(result.z, result.w);
            }
            Shape::lineTo(x, y);
        }
    };

    virtual std::unique_ptr<Shape> makeShape()
    {
        return std::make_unique<PolyShape>();
    }
};
GMREGISTER(feather_polyshapes, return new FeatherPolyShapesGM)

// Check that corners don't have artifacts.
class FeatherCornerGM : public FeatherGM
{
private:
    virtual void drawCell(Renderer* renderer,
                          int x,
                          int y,
                          RenderPaint* paint) override
    {
        float theta;
        if (x == 0)
        {
            theta = math::PI;
        }
        else if (x == 1)
        {
            theta = math::PI / 2;
        }
        else
        {
            theta = math::PI * powf((5 - x) / 5.f, 2.71828f);
        }
        renderer->clipPath(PathBuilder::Rect({-20, -20, 120, 120}));
        float left = 3 * math::PI;
        Vec2D v0 = Vec2D(cosf(left), sinf(left));
        Vec2D v1 = Vec2D(cosf(left - theta), sinf(left - theta));
        Path path(FillRule::clockwise);
        path->move(200 * v0 + Vec2D{0, 200});
        path->line(200 * v0);
        path->lineTo(0, 0);
        path->line(200 * v1);
        path->line(200 * v1 + Vec2D{0, 200});
        renderer->translate(50, 50);
        renderer->drawPath(path, paint);
    }
};
GMREGISTER(feather_corner, return new FeatherCornerGM)

// Check that tightly rounded corners don't have artifacts.
class FeatherRoundCornerGM : public FeatherGM
{
private:
    virtual void drawCell(Renderer* renderer,
                          int x,
                          int y,
                          RenderPaint* paint) override
    {
        float theta = math::PI * powf((5 - x) / 5.f, 1.5f);
        renderer->clipPath(PathBuilder::Rect({-20, -20, 120, 120}));
        float down = math::PI / 2;
        Vec2D v0 = Vec2D(cosf(down + theta / 2), sinf(down + theta / 2));
        Vec2D v1 = Vec2D(cosf(down - theta / 2), sinf(down - theta / 2));
        Path path(FillRule::clockwise);
        path->move(200 * v0 + Vec2D{0, 200});
        path->line(200 * v0);
        path->line(75 * v0);
        path->cubic({0, 0}, {0, 0}, 75 * v1);
        path->line(200 * v1);
        path->line(200 * v1 + Vec2D{0, 200});
        renderer->translate(50, 50);
        renderer->drawPath(path, paint);
    }
};
GMREGISTER(feather_roundcorner, return new FeatherRoundCornerGM)

// Check that the cusp points on a squashed ellipse don't have artifacts.
class FeatherEllipseGM : public FeatherGM
{

private:
    virtual void drawCell(Renderer* renderer,
                          int x,
                          int y,
                          RenderPaint* paint) override
    {
        auto unitCircle = PathBuilder::Circle(0, 0, 1);
        float squash = powf((5 - x) / 5.f, 2.71828f);
        Path ellipse(FillRule::clockwise);
        ellipse->addRenderPath(unitCircle, Mat2D::fromScale(50 * squash, 50));
        renderer->translate(50, 50);
        renderer->drawPath(ellipse, paint);
    }
};
GMREGISTER(feather_ellipse, return new FeatherEllipseGM)

// Check that a non-degenerate cubic cusps and near-cusps don't have artifacts.
class FeatherCuspGM : public FeatherGM
{
private:
    virtual void drawCell(Renderer* renderer,
                          int x,
                          int y,
                          RenderPaint* paint) override
    {
        float dx = 10 * copysignf(powf(fabsf(x - 3.f), 1.75f), x - 3);
        Path cusp(FillRule::clockwise);
        cusp->moveTo(0, 100);
        cusp->moveTo(0, 100);
        cusp->cubicTo(100 + dx, 0, 0 - dx, 0, 100, 100);
        renderer->drawPath(cusp, paint);
    }
};
GMREGISTER(feather_cusp, return new FeatherCuspGM)

// Check that basic strokes feather correctly (enough).
class FeatherStrokesGM : public FeatherGM
{
public:
    FeatherStrokesGM() : FeatherGM()
    {
        m_strokes.reserve(6);

        auto& square = m_strokes.emplace_back();
        square->addRect(0, 0, 100, 100);

        m_strokes.emplace_back(PathBuilder::Circle(50, 50, 50));

        auto& serp = m_strokes.emplace_back();
        serp->moveTo(0, 100);
        serp->cubicTo(60, 0, 30, 0, 100, 100);

        auto& cusp = m_strokes.emplace_back();
        cusp->lineTo(100, 0);
        cusp->cubicTo(0, 100, 0, 0, 100, 100);
        cusp->lineTo(0, 100);
        cusp->cubicTo(50, 67, -50, 33, 0, 0);

        auto& loop = m_strokes.emplace_back();
        loop->moveTo(25, 100);
        loop->cubicTo(250, -20, -150, -20, 75, 100);

        auto& irrect = m_strokes.emplace_back();
        float r = 40;
        Path rrect;
        rrect->moveTo(r, 0);
        rrect->lineTo(100 - r, 0);
        rrect->cubicTo(100 - r / 2, 0, 100, r / 2, 100, r);
        rrect->lineTo(100, 100 - r);
        rrect->cubicTo(100, 100 - r / 5, 100 - r / 5, 100, 100 - r, 100);
        rrect->lineTo(r, 100);
        rrect->cubicTo(0 + r / 3, 100, 0, 100 - r / 3, 0, 100 - r);
        rrect->lineTo(0, r);
        rrect->cubicTo(0, 0, 0, 0, r, 0);
        irrect->addRenderPath(square, Mat2D());
        irrect->addRenderPath(rrect,
                              Mat2D(-80.f / 100, 0, 0, 80.f / 100, 90, 10));
    }

private:
    virtual void drawCell(Renderer* renderer,
                          int x,
                          int y,
                          RenderPaint* paint) override
    {
        paint->style(RenderPaintStyle::stroke);
        paint->thickness(15);
        // Feathers ignore the join.
        paint->join((y & 1) ? StrokeJoin::bevel : StrokeJoin::miter);
        // Feathers ignore the cap.
        paint->cap((y & 1) ? StrokeCap::square : StrokeCap::butt);
        renderer->drawPath(m_strokes[x].get(), paint);
    }

    std::vector<Path> m_strokes;
};
GMREGISTER(feather_strokes, return new FeatherStrokesGM)
} // namespace rive::gpu

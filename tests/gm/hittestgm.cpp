/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/math/hit_test.hpp"

using namespace rivegm;

using V2D = rive::Vec2D;

class IPathSink
{
public:
    virtual ~IPathSink() {}

    virtual void move(V2D) = 0;
    virtual void line(V2D) = 0;
    virtual void cubic(V2D, V2D, V2D) = 0;
    virtual void close() = 0;
};

class RenderPathSink : public IPathSink
{
    rive::RenderPath* m_Path;

public:
    RenderPathSink(rive::RenderPath* rp) : m_Path(rp) {}

    void move(V2D v) override { m_Path->move(v); }
    void line(V2D v) override { m_Path->line(v); }
    void cubic(V2D a, V2D b, V2D c) override { m_Path->cubic(a, b, c); }
    void close() override { m_Path->close(); }
};

class HitTesterSink : public IPathSink
{
    rive::HitTester* m_HT;

public:
    HitTesterSink(rive::HitTester* ht) : m_HT(ht) {}

    void move(V2D v) override { m_HT->move(v); }
    void line(V2D v) override { m_HT->line(v); }
    void cubic(V2D a, V2D b, V2D c) override { m_HT->cubic(a, b, c); }
    void close() override { m_HT->close(); }
};

class BounderSink : public IPathSink
{
    rive::AABB m_Bounds;
    bool m_First = true;

    void join(V2D v)
    {
        assert(!m_First);
        m_Bounds.minX = std::min(m_Bounds.minX, v.x);
        m_Bounds.minY = std::min(m_Bounds.minY, v.y);
        m_Bounds.maxX = std::max(m_Bounds.maxX, v.x);
        m_Bounds.maxY = std::max(m_Bounds.maxY, v.y);
    }

public:
    BounderSink() {}

    rive::AABB bounds() const
    {
        assert(!m_First);
        return m_Bounds;
    }

    void move(V2D v) override
    {
        if (m_First)
        {
            m_Bounds.minX = m_Bounds.maxX = v.x;
            m_Bounds.minY = m_Bounds.maxY = v.y;
            m_First = false;
        }
        else
        {
            this->join(v);
        }
    }
    void line(V2D v) override { this->join(v); }
    void cubic(V2D a, V2D b, V2D c) override
    {
        this->join(a);
        this->join(b);
        this->join(c);
    }
    void close() override {}
};

static std::string fillrule_to_name(rive::FillRule fr)
{
    return fr == rive::FillRule::nonZero ? "nonZero" : "evenOdd";
}

class HitTestGM : public GM
{
    rive::FillRule m_FillRule;

public:
    HitTestGM(rive::FillRule fr) :
        GM(320, 460, ("hittest_" + fillrule_to_name(fr)).c_str()),
        m_FillRule(fr)
    {}

    void onDraw(rive::Renderer* renderer) override
    {
        auto make = [](IPathSink& sink) {
            sink.move({100, 10});
            sink.line({200, 100});
            sink.line({50, 230});
            sink.close();

            sink.move({120, 100});
            sink.line({300, 40});
            sink.line({250, 200});
            sink.close();

            sink.move({10, 300});
            sink.cubic({110, 150}, {210, 450}, {310, 300});
            sink.close();
        };

        Path path;
        path->fillRule(m_FillRule);
        {
            RenderPathSink rps(path.get());
            make(rps);
        }

        BounderSink bounder;
        make(bounder);

        const int tol = 4;
        const auto bounds = bounder.bounds().round().inset(-tol, -tol);

        Paint paint;
        paint->color(0xFFDDDDDD);
        draw_rect(renderer, bounds, paint.get());

        paint->color(0xFFFF0000);
        renderer->drawPath(path.get(), paint.get());

        rive::HitTester tester;
        HitTesterSink hts(&tester);

        paint->color(0x800000FF);
        for (int y = bounds.top; y < bounds.bottom; ++y)
        {
            for (int x = bounds.left; x < bounds.right; ++x)
            {
                const int t = 1;
                rive::IAABB r = {x, y, x + 1, y + 1};
                tester.reset(r.inset(-t, -t));

                make(hts);
                if (tester.test(m_FillRule))
                {
                    draw_rect(renderer, r, paint.get());
                }
            }
        }
    }
};

GMREGISTER_SLOW(return new HitTestGM(rive::FillRule::evenOdd))
GMREGISTER_SLOW(return new HitTestGM(rive::FillRule::nonZero))

/*
 * Copyright 2022 Rive
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "bicubic.hpp"

#include "rive/math/math_types.hpp"
#include "rive/math/hit_test.hpp"

#include "assets/nomoon.png.hpp"

class Random
{
    //  See "Numerical Recipes in C", 1992 page 284 for these constants
    //  For the LCG that sets the initial state from a seed
    enum
    {
        kMul = 1664525,
        kAdd = 1013904223
    };
    uint32_t m_Seed;

public:
    Random(uint32_t seed = 0) : m_Seed(seed) {}

    uint32_t nextU()
    {
        m_Seed = m_Seed * kMul + kAdd;
        return m_Seed;
    }

    float nextF()
    {
        double x = this->nextU() * (1.0 / (1 << 30));
        return (float)(x - std::floor(x));
    }

    float nextF(float min, float max)
    {
        return this->nextF() * (max - min) + min;
    }
};

using namespace rivegm;

static void make_patch(rive::Vec2D pts[16], const rive::AABB& r)
{
    const float sx = r.width() / 3;
    const float sy = r.height() / 3;

    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            const int index = y * 4 + x;
            pts[index] = {
                r.left() + x * sx,
                r.top() + y * sy,
            };
        }
    }
}

static void perterb_patch(rive::Vec2D pts[16], float scale)
{
    Random rand;
    auto rf = [&]() { return rand.nextF(-scale, scale); };

    for (int i = 0; i < 16; ++i)
    {
        auto dx = rf();
        auto dy = rf();
        pts[i] += {dx, dy};
    }
}

class MeshGM : public GM
{
public:
    MeshGM() : GM(800, 600) {}

    void onDraw(rive::Renderer* ren) override
    {
        auto img = LoadImage(assets::nomoon_png());

        constexpr float kGridCellSize = 20;
        Path grid;
        grid->fillRule(rive::FillRule::evenOdd);
        for (size_t v = 0; v < 400 / (kGridCellSize * 2); ++v)
        {
            grid->addRect(0, v * kGridCellSize * 2, 1600, kGridCellSize);
        }
        for (size_t u = 0; u < 1600 / (kGridCellSize * 2); ++u)
        {
            grid->addRect(u * kGridCellSize * 2, 0, kGridCellSize, 400);
        }
        Paint gray;
        gray->color(0xa0a0a0a0);

        BicubicPatch patch;

        rive::AABB r = {50, 50, 350, 350};
        make_patch(patch.m_Pts, r);
        perterb_patch(patch.m_Pts, 50);
        auto mesh = patch.mesh(TestingWindow::Get()->factory());
        uint32_t vertexCount =
            mesh.pts ? rive::math::lossless_numeric_cast<uint32_t>(
                           mesh.pts->sizeInBytes() / sizeof(rive::Vec2D))
                     : 0;
        uint32_t indexCount =
            mesh.indices ? rive::math::lossless_numeric_cast<uint32_t>(
                               mesh.indices->sizeInBytes() / sizeof(uint16_t))
                         : 0;

        ren->scale(.5f, .5f);

        constexpr rive::BlendMode blendModes[] = {rive::BlendMode::srcOver,
                                                  rive::BlendMode::darken,
                                                  rive::BlendMode::luminosity,
                                                  rive::BlendMode::exclusion};

        // Draw meshes that exercise all variations in the mesh shader:
        // viewMatrix, opacity, blendMode, clipRect, clip.
        for (size_t j = 0; j < 3; ++j)
        {
            if (j == 1)
            {
                ren->clipPath(
                    PathBuilder::Rect({50, 120, 1600 - 50, 800 - 120}));
            }
            ren->drawPath(grid, gray);
            ren->save();
            for (size_t i = 0; i < 4; ++i)
            {
                ren->save();
                ren->translate(200, 200);
                ren->rotate(i);
                ren->translate(-200, -200);
                if (j == 2)
                {
                    ren->clipPath(PathBuilder::Circle(200, 200, 150));
                }
                if (img != nullptr)
                {
                    ren->drawImageMesh(img.get(),
                                       rive::ImageSampler::LinearClamp(),
                                       mesh.pts,
                                       mesh.uvs,
                                       mesh.indices,
                                       vertexCount,
                                       indexCount,
                                       blendModes[i],
                                       1.f - .1f * (i + 1));
                }
                ren->restore();
                ren->translate(400, 0);
            }
            ren->restore();
            ren->translate(0, 400);
        }

        if (false)
        {
            Paint paint;
            for (int i = 0; i < 16; ++i)
            {
                auto p = patch.m_Pts[i];
                draw_rect(ren, {p.x - 2, p.y - 2, p.x + 3, p.y + 3}, paint);
            }
        }
    }
};
GMREGISTER(mesh, return new MeshGM)

class MeshHitTestGM : public GM
{
    BicubicPatch::Rec rec;
    rive::AABB r = {15, 15, 45, 45};
    int fN;

public:
    MeshHitTestGM(int N) : GM(60, 60) { fN = N; }

    void onOnceBeforeDraw() override
    {
        BicubicPatch patch;

        make_patch(patch.m_Pts, r);
        perterb_patch(patch.m_Pts, 6);

        rec = patch.buffers();
    }

    void onDraw(rive::Renderer* ren) override
    {
        Paint paint;

        for (float y = 0; y < 60; y += 1)
        {
            for (float x = 0; x < 60; x += 1)
            {
                auto area = rive::AABB(x, y, x + fN, y + fN);
                rive::ColorInt color = 0xFF000000;
                auto ia = area.round();
                if (rive::HitTester::testMesh(ia, rec.pts, rec.indices))
                {
                    color = 0xFFFFFFFF;
                }

                paint->color(color);
                draw_rect(ren, area, paint.get());
            }
        }
    }
};

GMREGISTER(mesh_ht_7, return new MeshHitTestGM(7))
GMREGISTER(mesh_ht_1, return new MeshHitTestGM(1))

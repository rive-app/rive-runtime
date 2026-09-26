/*
 * Copyright 2026 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"

#include "rive/math/math_types.hpp"

#include "assets/nomoon.png.hpp"

using namespace rivegm;
using namespace rive;

constexpr int QuadSize = 120;
constexpr int Columns = 4;
constexpr int Rows = 4;
constexpr int Pad = 10;

struct QuadMesh
{
    rcp<RenderBuffer> pts;
    rcp<RenderBuffer> uvs;
    rcp<RenderBuffer> indices;
    uint32_t vertexCount = 4;
    uint32_t indexCount = 6;
};

static rcp<RenderBuffer> makeVertexBuffer(Factory* factory,
                                          const Vec2D values[4])
{
    auto buffer =
        factory->makeRenderBuffer(RenderBufferType::vertex,
                                  RenderBufferFlags::mappedOnceAtInitialization,
                                  4 * sizeof(Vec2D));
    memcpy(buffer->map(), values, 4 * sizeof(Vec2D));
    buffer->unmap();
    return buffer;
}

static QuadMesh makeQuad(Factory* factory)
{
    const Vec2D pts[4] = {{0, 0},
                          {QuadSize, 0},
                          {QuadSize, QuadSize},
                          {0, QuadSize}};
    const Vec2D uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    const uint16_t indices[6] = {0, 1, 2, 0, 2, 3};

    QuadMesh mesh;
    mesh.pts = makeVertexBuffer(factory, pts);
    mesh.uvs = makeVertexBuffer(factory, uvs);
    mesh.indices =
        factory->makeRenderBuffer(RenderBufferType::index,
                                  RenderBufferFlags::mappedOnceAtInitialization,
                                  sizeof(indices));
    memcpy(mesh.indices->map(), indices, sizeof(indices));
    mesh.indices->unmap();
    return mesh;
}

static Mat2D cellTransform(int column, int row)
{
    return Mat2D::fromTranslate(Pad + column * (QuadSize + Pad),
                                Pad + row * (QuadSize + Pad));
}

// Use a gray background so we can see opacity and additiveness
DEF_SIMPLE_GM_WITH_CLEAR_COLOR(mesh_instanced,
                               0xff808080,
                               Columns*(QuadSize + Pad) + Pad,
                               Rows*(QuadSize + Pad) + Pad,
                               ren)
{
    auto img = LoadImage(assets::nomoon_png());
    if (img == nullptr)
    {
        return;
    }

    Factory* factory = TestingWindow::Get()->factory();
    QuadMesh mesh = makeQuad(factory);

    auto instances = factory->makeImageMeshInstances(Columns * Rows);
    Span<ImageMeshInstanceData> data = instances->edit();

    for (int column = 0; column < Columns; ++column)
    {
        const float t = column / float(Columns - 1);

        // Row 0: transform only
        Mat2D spin = Mat2D::fromTranslate(QuadSize * 0.5f, QuadSize * 0.5f) *
                     Mat2D::fromRotation(t * math::PI * 0.5f) *
                     Mat2D::fromTranslate(QuadSize * -0.5f, QuadSize * -0.5f);
        data[column].transform = cellTransform(column, 0) * spin;

        // Row 1: opacity ramp
        data[Columns + column].transform = cellTransform(column, 1);
        data[Columns + column].opacity = 1.0f - 0.25f * column;

        // Row 2: UV transform
        data[Columns * 2 + column].transform = cellTransform(column, 2);
        data[Columns * 2 + column].uvScale = {0.5f, 0.5f};
        data[Columns * 2 + column].uvTranslate = {(column & 1) ? 0.5f : 0.0f,
                                                  (column & 2) ? 0.5f : 0.0f};

        // Row 3: additiveness ramp.
        data[Columns * 3 + column].transform = cellTransform(column, 3);
        data[Columns * 3 + column].additiveness = t;
    }
    instances->endEdit();

    ren->drawImageMeshInstanced(img.get(),
                                ImageSampler::LinearClamp(),
                                mesh.pts,
                                mesh.uvs,
                                mesh.indices,
                                mesh.vertexCount,
                                mesh.indexCount,
                                instances);
}

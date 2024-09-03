/*
 * Copyright 2022 Rive
 */

#include "bicubic.hpp"
#include <vector>

template <typename T> T lerp(const T& a, const T& b, float t) { return a * (1 - t) + b * t; }

using namespace rivegm;

rive::Vec2D BicubicPatch::eval(float u, float v) const
{
    float weights[16];
    this->evalWeights(u, v, weights);

    rive::Vec2D result{0, 0};
    for (int i = 0; i < 16; ++i)
    {
        result += m_Pts[i] * weights[i];
    }
    return result;
}

void BicubicPatch::evalWeights(float u, float v, float weights[16]) const
{
    auto compute_bezier_coeff = [](float c[4], float t) {
        float s = 1 - t;
        c[0] = s * s * s;
        c[1] = 3 * t * s * s;
        c[2] = 3 * t * t * s;
        c[3] = t * t * t;
    };

    float cu[4], cv[4];
    compute_bezier_coeff(cu, u);
    compute_bezier_coeff(cv, v);

    int i = 0;
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            weights[i++] = cu[x] * cv[y];
        }
    }
}

BicubicPatch::Rec BicubicPatch::buffers(const rive::AABB* uvBounds) const
{
    constexpr int N = 16;

    constexpr int QUADS = N * N;
    constexpr int VERTS = (N + 1) * (N + 1);
    constexpr int TRIS = QUADS * 2;
    constexpr int INDXS = TRIS * 3;

    const rive::AABB unit = {0, 0, 1, 1};
    if (!uvBounds)
    {
        uvBounds = &unit;
    }

    Rec rec;
    rec.pts.resize(VERTS);
    rec.uvs.resize(VERTS);

    auto pos = rec.pts.data();
    auto tex = rec.uvs.data();

    const float normalize = 1.0f / N;

    for (int y = 0; y <= N; ++y)
    {
        const float v = y * normalize;
        const float tex_v = lerp(uvBounds->top(), uvBounds->bottom(), v);
        for (int x = 0; x <= N; ++x)
        {
            const float u = x * normalize;
            *pos++ = this->eval(u, v);
            *tex++ = {
                lerp(uvBounds->left(), uvBounds->right(), u),
                tex_v,
            };
        }
    }
    assert(pos - rec.pts.data() == VERTS);
    assert(tex - rec.uvs.data() == VERTS);

    rec.indices.resize(INDXS);
    uint16_t* ndx = rec.indices.data();

    constexpr int W = N + 1;

    int index = 0;
    for (int y = 0; y < N; ++y)
    {
        for (int x = 0; x < N; ++x)
        {
            ndx[0] = index;
            ndx[1] = index + 1;
            ndx[2] = index + 1 + W;
            ndx[3] = index;
            ndx[4] = index + 1 + W;
            ndx[5] = index + W;
            ndx += 6;
            index += 1;
        }
        index += 1; // skip the last column
    }
    assert(ndx - rec.indices.data() == INDXS);
    for (int i = 0; i < INDXS; ++i)
    {
        assert(rec.indices[i] < VERTS);
    }

    return rec;
}

BicubicPatch::Mesh BicubicPatch::mesh(rive::Factory* factory, const rive::AABB* uvBounds) const
{
    auto rec = BicubicPatch::buffers(uvBounds);

    auto ptsBuffer = factory->makeRenderBuffer(rive::RenderBufferType::vertex,
                                               rive::RenderBufferFlags::none,
                                               rec.pts.size() * sizeof(rive::Vec2D));
    if (ptsBuffer)
    {
        void* ptsData = ptsBuffer->map();
        memcpy(ptsData, rec.pts.data(), ptsBuffer->sizeInBytes());
        ptsBuffer->unmap();
    }

    auto uvsBuffer = factory->makeRenderBuffer(rive::RenderBufferType::vertex,
                                               rive::RenderBufferFlags::mappedOnceAtInitialization,
                                               rec.uvs.size() * sizeof(rive::Vec2D));
    if (uvsBuffer)
    {
        void* uvsData = uvsBuffer->map();
        memcpy(uvsData, rec.uvs.data(), uvsBuffer->sizeInBytes());
        uvsBuffer->unmap();
    }

    auto idxBuffer = factory->makeRenderBuffer(rive::RenderBufferType::index,
                                               rive::RenderBufferFlags::mappedOnceAtInitialization,
                                               rec.indices.size() * sizeof(uint16_t));
    if (idxBuffer)
    {
        void* idxData = idxBuffer->map();
        memcpy(idxData, rec.indices.data(), idxBuffer->sizeInBytes());
        idxBuffer->unmap();
    }

    return {std::move(ptsBuffer), std::move(uvsBuffer), std::move(idxBuffer)};
}

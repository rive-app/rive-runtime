/*
 * Copyright 2022 Rive
 */

#ifndef BicubicPatch_DEFINED
#define BicubicPatch_DEFINED

#include "rive/math/vec2d.hpp"
#include "rive/factory.hpp"
#include <vector>

namespace rivegm
{

struct BicubicPatch
{
    // These are order 4x4, row-major, relative to the
    // uvBounds that is passed to mesh()
    //
    // [ 0]  [ 1]  [ 2]  [ 3]
    // [ 4]  [ 5]  [ 6]  [ 7]
    // [ 8]  [ 9]  [10]  [11]
    // [12]  [13]  [14]  [15]
    //
    rive::Vec2D m_Pts[16];

    rive::Vec2D eval(float u, float v) const;

    void evalWeights(float u, float v, float weights[16]) const;

    struct Mesh
    {
        rive::rcp<rive::RenderBuffer> pts;     // float pairs
        rive::rcp<rive::RenderBuffer> uvs;     // float pairs
        rive::rcp<rive::RenderBuffer> indices; // uint16_t
    };
    Mesh mesh(rive::Factory*, const rive::AABB* uvBounds = nullptr) const;

    struct Rec
    {
        std::vector<rive::Vec2D> pts;
        std::vector<rive::Vec2D> uvs;
        std::vector<uint16_t> indices;
    };
    Rec buffers(const rive::AABB* uvBounds = nullptr) const;
};

} // namespace rivegm

#endif

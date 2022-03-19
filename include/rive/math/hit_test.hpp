/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_HITTEST_HPP_
#define _RIVE_HITTEST_HPP_

#include "rive/math/aabb.hpp"
#include "rive/math/path_types.hpp"
#include "rive/math/vec2d.hpp"

#include <cstdint>
#include <vector>

namespace rive {

class HitTester {
    std::vector<int> m_DW;  // width * height delta-windings
    Vec2D            m_First, m_Prev;
    Vec2D            m_offset;
    float            m_height;
    int              m_IWidth, m_IHeight;
    int              m_WindingMask;
    bool             m_ExpectsMove;

    void recurse_cubic(Vec2D b, Vec2D c, Vec2D d, int count);

public:
    HitTester() {}
    HitTester(const IAABB& area) { reset(area); }

    void reset();
    void reset(const IAABB& area, FillRule = rive::FillRule::nonZero);

    void move(Vec2D);
    void line(Vec2D);
    void quad(Vec2D, Vec2D);
    void cubic(Vec2D, Vec2D, Vec2D);
    void close();
    
    void addRect(const AABB&, const Mat2D&, PathDirection = PathDirection::ccw);

    bool test();    // calls reset() afterwards
};

}

#endif


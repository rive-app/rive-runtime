/*
 * Copyright 2022 Rive
 */

#include "rive/math/raw_path.hpp"
#include <cmath>

using namespace rive;

AABB RawPath::bounds() const {
    if (this->empty()) {
        return {0, 0, 0, 0};
    }

    float l, t, r, b;
    l = r = m_Points[0].x;
    t = b = m_Points[0].y;
    for (size_t i = 1; i < m_Points.size(); ++i) {
        const float x = m_Points[i].x;
        const float y = m_Points[i].y;
        l = std::min(l, x);
        r = std::max(r, x);
        t = std::min(t, y);
        b = std::max(b, y);
    }
    return {l, t, r, b};
}

void RawPath::move(Vec2D a) {
    m_Points.push_back(a);
    m_Verbs.push_back(PathVerb::move);
}

void RawPath::line(Vec2D a) {
    m_Points.push_back(a);
    m_Verbs.push_back(PathVerb::line);
}

void RawPath::quad(Vec2D a, Vec2D b) {
    m_Points.push_back(a);
    m_Points.push_back(b);
    m_Verbs.push_back(PathVerb::quad);
}

void RawPath::cubic(Vec2D a, Vec2D b, Vec2D c) {
    m_Points.push_back(a);
    m_Points.push_back(b);
    m_Points.push_back(c);
    m_Verbs.push_back(PathVerb::cubic);
}

void RawPath::close() {
    const auto n = m_Verbs.size();
    if (n > 0 && m_Verbs[n - 1] != PathVerb::close) {
        m_Verbs.push_back(PathVerb::close);
    }
}

RawPath RawPath::transform(const Mat2D& m) const {
    RawPath path;

    path.m_Verbs = m_Verbs;

    path.m_Points.resize(m_Points.size());
    for (size_t i = 0; i < m_Points.size(); ++i) {
        path.m_Points[i] = m * m_Points[i];
    }
    return path;
}

void RawPath::transformInPlace(const Mat2D& m) {
    for (auto& p : m_Points) {
        p = m * p;
    }
}

void RawPath::addRect(const AABB& r, PathDirection dir) {
    // We manually close the rectangle, in case we want to stroke
    // this path. We also call close() so we get proper joins
    // (and not caps).

    m_Points.reserve(5);
    m_Verbs.reserve(6);

    moveTo(r.left(), r.top());
    if (dir == PathDirection::clockwise) {
        lineTo(r.right(), r.top());
        lineTo(r.right(), r.bottom());
        lineTo(r.left(), r.bottom());
    } else {
        lineTo(r.left(), r.bottom());
        lineTo(r.right(), r.bottom());
        lineTo(r.right(), r.top());
    }
    close();
}

void RawPath::addOval(const AABB& r, PathDirection dir) {
    // see https://spencermortensen.com/articles/bezier-circle/
    constexpr float C = 0.5519150244935105707435627f;
    // precompute clockwise unit circle, starting and ending at {1, 0}
    constexpr rive::Vec2D unit[] = {
        {1, 0},
        {1, C},
        {C, 1}, // quadrant 1 ( 4:30)
        {0, 1},
        {-C, 1},
        {-1, C}, // quadrant 2 ( 7:30)
        {-1, 0},
        {-1, -C},
        {-C, -1}, // quadrant 3 (10:30)
        {0, -1},
        {C, -1},
        {1, -C}, // quadrant 4 ( 1:30)
        {1, 0},
    };

    const auto center = r.center();
    const float dx = center.x;
    const float dy = center.y;
    const float sx = r.width() * 0.5f;
    const float sy = r.height() * 0.5f;

    auto map = [dx, dy, sx, sy](rive::Vec2D p) {
        return rive::Vec2D(p.x * sx + dx, p.y * sy + dy);
    };

    m_Points.reserve(13);
    m_Verbs.reserve(6);

    if (dir == PathDirection::clockwise) {
        move(map(unit[0]));
        for (int i = 1; i <= 12; i += 3) {
            cubic(map(unit[i + 0]), map(unit[i + 1]), map(unit[i + 2]));
        }
    } else {
        move(map(unit[12]));
        for (int i = 11; i >= 0; i -= 3) {
            cubic(map(unit[i - 0]), map(unit[i - 1]), map(unit[i - 2]));
        }
    }
    close();
}

void RawPath::addPoly(Span<const Vec2D> span, bool isClosed) {
    if (span.size() == 0) {
        return;
    }

    // should we permit must moveTo() or just moveTo()/close() ?

    m_Points.reserve(span.size() + isClosed);
    m_Verbs.reserve(span.size() + isClosed);

    move(span[0]);
    for (size_t i = 1; i < span.size(); ++i) {
        line(span[i]);
    }
    if (isClosed) {
        close();
    }
}

//////////////////////////////////////////////////////////////////////////

namespace rive {
    int path_verb_to_point_count(PathVerb v) {
        static uint8_t ptCounts[] = {
            1, // move
            1, // line
            2, // quad
            2, // conic (unused)
            3, // cubic
            0, // close
        };
        size_t index = (size_t)v;
        assert(index < sizeof(ptCounts));
        return ptCounts[index];
    }
} // namespace rive

RawPath::Iter::Rec RawPath::Iter::next() {
    // initialize with "false"
    Rec rec = {nullptr, -1, (PathVerb)-1};

    if (m_currVerb < m_stopVerb) {
        rec.pts = m_currPts;
        rec.verb = *m_currVerb++;
        rec.count = path_verb_to_point_count(rec.verb);

        m_currPts += rec.count;
    }
    return rec;
}

void RawPath::Iter::backUp() {
    --m_currVerb;
    const int n = path_verb_to_point_count(*m_currVerb);
    m_currPts -= n;
}

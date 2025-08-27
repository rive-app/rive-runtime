/*
 * Copyright 2022 Rive
 */

#include "bench.hpp"

#include "rive/math/raw_path.hpp"

using namespace rive;

static void map(const Mat2D& m,
                std::vector<Vec2D>& dst,
                const std::vector<Vec2D>& pts)
{
    size_t n = pts.size();
#if 0
    // Scalar implementation.
    for (size_t i = 0; i < n; ++i)
        dst[i] = m * pts[i];
#else
    // Vector implementation.
    m.mapPoints(dst.data(), pts.data(), n);
#endif
}

// Measure the speed of Mat2D::mapPoints().
class MapPoints : public Bench
{
public:
    MapPoints(const Mat2D& matrix) : m_matrix(matrix)
    {
        m_points.resize(32768 / sizeof(Vec2D)); // 32k so we fit in L1.
        srand(0);
        for (Vec2D& pt : m_points)
            pt = Vec2D(float(rand()), float(rand())) * 100 / (float)RAND_MAX;
        m_dst.resize(m_points.size());
    }

private:
    int run() const override
    {
        map(m_matrix, m_dst, m_points);
        for (int i = 1; i < 4096; ++i)
            map(m_matrix, m_dst, m_dst);
        return static_cast<int>(m_dst.back().x);
    }

    Mat2D m_matrix;
    std::vector<Vec2D> m_points;
    mutable std::vector<Vec2D> m_dst;
};

class MapPointsScaleTrans : public MapPoints
{
public:
    MapPointsScaleTrans() : MapPoints(Mat2D(-2, 0, 0, 3, -4, 5)) {}
};

class MapPointsAffine : public MapPoints
{
public:
    MapPointsAffine() : MapPoints(Mat2D(2, -3, -4, 5, 6, -7)) {}
};

REGISTER_BENCH(MapPointsScaleTrans);
REGISTER_BENCH(MapPointsAffine);

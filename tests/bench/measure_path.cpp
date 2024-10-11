/*
 * Copyright 2022 Rive
 */

#include "bench.hpp"

#include "rive/math/raw_path.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/math/simd.hpp"

using namespace rive;

static Vec2D randpt()
{
    return Vec2D(float(rand()), float(rand())) * 1000 / (float)RAND_MAX;
}

// Measure the speed MetricsPath::computeLength().
class MeasurePath : public Bench
{
public:
    MeasurePath()
    {
        // Make a random path.
        srand(0);
        for (int i = 0; i < 1000000; ++i)
        {
            int r = rand() % 22;
            if (r < 3)
            {
                m_path.line(randpt());
            }
            else
            {
                m_path.cubic(randpt(), randpt(), randpt());
            }
        }
    }

private:
    int run() const override
    {
        std::swap(m_matrix[4], m_matrix[5]);
        RawPath container;
        container.addPath(m_path, &m_matrix);

        return ContourMeasureIter(&container).next()->length();
    }

    mutable Mat2D m_matrix{1, 0, 0, 1, -1, 1};
    mutable RawPath m_path;
};

REGISTER_BENCH(MeasurePath);

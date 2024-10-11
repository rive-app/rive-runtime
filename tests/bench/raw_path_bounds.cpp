/*
 * Copyright 2022 Rive
 */

#include "bench.hpp"

#include "rive/math/raw_path.hpp"

using namespace rive;

static Vec2D randpt()
{
    return Vec2D(float(rand()), float(rand())) * 100 / (float)RAND_MAX;
}

// Measure the speed of RawPath::bounds().
class RawPathBounds : public Bench
{
public:
    RawPathBounds()
    {
        // Make a random path.
        srand(0);
        for (int i = 0; i < 1000000; ++i)
        {
            int r = rand() % 22;
            if (r == 0)
            {
                m_path.move(randpt());
            }
            else if (r == 1)
            {
                m_path.close();
            }
            else if (r < 12)
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
        AABB bounds = m_path.bounds();
        return (int)(bounds.minX + bounds.minY + bounds.maxX + bounds.maxY);
    }

    RawPath m_path;
};

REGISTER_BENCH(RawPathBounds);

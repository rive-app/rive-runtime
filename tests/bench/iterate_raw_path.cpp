/*
 * Copyright 2022 Rive
 */

#include "bench.hpp"

#include "rive/math/raw_path.hpp"
#include "rive/math/simd.hpp"

using namespace rive;

static Vec2D randpt() { return Vec2D(float(rand()), float(rand())) * 100 / (float)RAND_MAX; }

// Measure the speed of RawPath iteration. In an attempt to be representative of algorithms that
// need to know p0 for lines and cubics, we sum up the midpoints of lines and cubics.
class IterateRawPath : public Bench
{
public:
    IterateRawPath()
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
        float4 midpointsSum{};
        float2 startPt{};
        for (auto [verb, p] : m_path)
        {
            switch (verb)
            {
                case PathVerb::move:
                    startPt = simd::load2f(&p[0].x);
                    break;
                case PathVerb::line:
                {
                    float4 p0_1 = simd::load4f(&p[0].x);
                    midpointsSum += p0_1 * .5f;
                    break;
                }
                case PathVerb::cubic:
                {
                    float4 p0_1 = simd::load4f(&p[0].x);
                    midpointsSum += p0_1 * float4{.125f, .125f, .75f, .75f};
                    float4 p2_3 = simd::load4f(&p[2].x);
                    midpointsSum += p2_3 * float4{.75f, .75f, .125f, .125f};
                    break;
                }
                case PathVerb::close:
                {
                    float4 p0_1;
                    p0_1.xy = simd::load2f(&p[0].x);
                    p0_1.zw = startPt;
                    midpointsSum += p0_1 * .5f;
                    break;
                }
                case PathVerb::quad:
                    RIVE_UNREACHABLE();
            }
        }
        midpointsSum.xy += midpointsSum.zw;
        return (int)(midpointsSum.x + midpointsSum.y);
    }

    RawPath m_path;
};

REGISTER_BENCH(IterateRawPath);

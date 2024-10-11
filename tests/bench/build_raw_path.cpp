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

// Measure the speed of building a RawPath, without growing arrays or figuring
// out the actual path contents.
class BuildRawPath : public Bench
{
public:
    BuildRawPath()
    {
        // Decide the path data we will build with upfront.
        srand(0);
        for (int i = 0; i < 100000; ++i)
        {
            switch (rand() % 4)
            {
                case 0:
                    m_verbs.push_back(PathVerb::move);
                    m_pts.push_back(randpt());
                    break;
                case 1:
                    m_verbs.push_back(PathVerb::line);
                    for (int i = 0; i < 10; ++i)
                    { // 10 lines.
                        m_pts.push_back(randpt());
                    }
                    break;
                case 2:
                    m_verbs.push_back(PathVerb::cubic);
                    for (int i = 0; i < 10 * 3; ++i)
                    { // 10 cubics.
                        m_pts.push_back(randpt());
                    }
                    break;
                case 3:
                    m_verbs.push_back(PathVerb::close);
                    break;
            }
        }
        run(); // Run once to preallocate the RawPath arrays.
    }

private:
    int run() const override
    {
        const Vec2D* p = m_pts.data();
        m_path.rewind();
        for (PathVerb verb : m_verbs)
        {
            switch (verb)
            {
                case PathVerb::move:
                    m_path.move(*p++);
                    break;
                case PathVerb::line: // 10 lines.
                    for (int i = 0; i < 10; ++i)
                    {
                        m_path.line(*p++);
                    }
                    break;
                case PathVerb::cubic: // 10 cubics.
                    for (int i = 0; i < 10; ++i)
                    {
                        m_path.cubic(p[0], p[1], p[2]);
                        p += 3;
                    }
                    break;
                case PathVerb::close:
                    m_path.close();
                    break;
                case PathVerb::quad:
                    RIVE_UNREACHABLE();
            }
        }
        return static_cast<int>(m_path.points().count());
    }

    // Decide verbs and points upfront so we just measure path building time.
    std::vector<PathVerb> m_verbs;
    std::vector<Vec2D> m_pts;

    mutable RawPath m_path;
};

REGISTER_BENCH(BuildRawPath);

/*
 * Copyright 2026 Rive
 */

#include "bench.hpp"

#include "rive/math/raw_path.hpp"
#include "rive_render_path.hpp"

using namespace rive;

static Vec2D randpt()
{
    return Vec2D(float(rand()), float(rand())) * 100 / (float)RAND_MAX;
}

// Measure the handoff every dirty ShapePaintPath makes once a frame. Points
// are random, so nothing is prunable: the worst case for the untrusted variant.
class AddRawPathToRenderPath : public Bench
{
public:
    AddRawPathToRenderPath()
    {
        srand(0);
        for (int i = 0; i < 1000; ++i)
        {
            RawPath path;
            path.move(randpt());
            for (int j = 0; j < 64; ++j)
            {
                if (rand() % 4 == 0)
                {
                    path.line(randpt());
                }
                else
                {
                    path.cubic(randpt(), randpt(), randpt());
                }
            }
            path.close();
            m_rawPaths.push_back(std::move(path));
            m_renderPaths.push_back(make_rcp<RiveRenderPath>());
        }
        run(); // Run once to preallocate the RiveRenderPath arrays.
    }

protected:
    int run() const override
    {
        for (size_t i = 0; i < m_rawPaths.size(); ++i)
        {
            m_renderPaths[i]->rewind();
            m_renderPaths[i]->addRawPath(m_rawPaths[i]);
        }
        return static_cast<int>(m_renderPaths.size());
    }

    std::vector<RawPath> m_rawPaths;
    std::vector<rcp<RiveRenderPath>> m_renderPaths;
};

// What a script's geometry costs over the trusted handoff above.
class AddUntrustedRawPathToRenderPath : public AddRawPathToRenderPath
{
private:
    int run() const override
    {
        for (size_t i = 0; i < m_rawPaths.size(); ++i)
        {
            m_renderPaths[i]->rewind();
            m_renderPaths[i]->addUntrustedRawPath(m_rawPaths[i]);
        }
        return static_cast<int>(m_renderPaths.size());
    }
};

REGISTER_BENCH(AddRawPathToRenderPath);
REGISTER_BENCH(AddUntrustedRawPathToRenderPath);

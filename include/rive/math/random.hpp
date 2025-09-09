/*
 * Copyright 2022 Rive
 */
#ifndef _RIVE_RANDOM_HPP_
#define _RIVE_RANDOM_HPP_

#include <vector>
#include <cstdlib>

namespace rive
{

class RandomProvider
{
#ifdef TESTING
private:
    static int m_randomCallsCount;
    static std::vector<float> m_randomResults;

public:
    static void addRandomValue(float value)
    {
        m_randomResults.push_back(value);
    }
    static void clearRandoms()
    {
        m_randomCallsCount = 0;
        m_randomResults.clear();
    }
    static float generateRandomFloat()
    {
        m_randomCallsCount++;
        if (m_randomResults.size() > 0)
        {
            auto newRandom = m_randomResults[0];
            m_randomResults.erase(m_randomResults.begin());
            return newRandom;
        }
        return 0;
    }
    static int totalCalls() { return m_randomCallsCount; };
#else
public:
    static float generateRandomFloat()
    {
        return (float)rand() / float(RAND_MAX);
    }
#endif
};

} // namespace rive

#endif

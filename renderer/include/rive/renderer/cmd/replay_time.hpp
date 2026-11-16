/*
 * Copyright 2026 Rive
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace rive::cmd
{

// How long recent frames took. Polling quantizes each sample, so it is a
// mean, trimmed so one slow frame (a cold load's first) cannot move it.
class ReplayTime
{
public:
  float note(double ms)
  {
    m_samples[m_sampleCount++ % kSamples] = static_cast<float>(ms);
    const size_t count = std::min(m_sampleCount, kSamples);
    if (count < kMinSamples)
    {
      return 0;
    }
    std::array<float, kSamples> sorted = m_samples;
    std::sort(sorted.begin(), sorted.begin() + count);
    const size_t trim = std::max<size_t>(count / 4, 1);
    float sum = 0;
    for (size_t i = trim; i < count - trim; ++i)
    {
      sum += sorted[i];
    }
    return sum / (count - 2 * trim);
  }

private:
  static constexpr size_t kSamples = 9;
  static constexpr size_t kMinSamples = 3;

  std::array<float, kSamples> m_samples{};
  size_t m_sampleCount = 0;
};

} // namespace rive::cmd

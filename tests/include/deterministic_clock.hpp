#ifndef _RIVE_TESTS_DETERMINISTIC_CLOCK_HPP_
#define _RIVE_TESTS_DETERMINISTIC_CLOCK_HPP_

#include "rive/file.hpp"

// Runs a test in deterministic mode and hands out one stamp per frame, so
// velocity comes from the stamps rather than from how fast the calls run.
class DeterministicClock
{
public:
  DeterministicClock() { rive::File::deterministicMode = true; }
  ~DeterministicClock() { rive::File::deterministicMode = m_wasDeterministic; }

  float next() { return m_time += 1.0f / 60.0f; }

private:
  bool m_wasDeterministic = rive::File::deterministicMode;
  float m_time = 0.0f;
};

#endif

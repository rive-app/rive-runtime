#include "rive/stable_id.hpp"
#include <atomic>

using namespace rive;

uint32_t StableId::get()
{
  static std::atomic<uint32_t> s_last{0};
  // Again when the counter wraps onto 0.
  while (m_value == 0)
  {
    m_value = ++s_last;
  }
  return m_value;
}

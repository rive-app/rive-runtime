#ifndef _RIVE_ASYNC_TIMEOUTS_HPP_
#define _RIVE_ASYNC_TIMEOUTS_HPP_

#include <algorithm>
#include <chrono>

namespace rive
{
/// Script waits take seconds as a double, where infinity, or anything past
/// about thirty years, means no limit, and a negative or NaN means none at all.
inline bool unboundedSeconds(double seconds) { return seconds > 1e9; }

/// The deadline a bounded wait of seconds from now ends at.
inline std::chrono::steady_clock::time_point deadlineAfter(double seconds)
{
  return std::chrono::steady_clock::now() +
         std::chrono::duration_cast<std::chrono::steady_clock::duration>(
             std::chrono::duration<double>(std::max(0.0, seconds)));
}
} // namespace rive

#endif

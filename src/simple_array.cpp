#ifdef TESTING
#include "rive/simple_array.hpp"
namespace rive
{
namespace SimpleArrayTesting
{
int mallocCount = 0;
int reallocCount = 0;
int freeCount = 0;
void resetCounters()
{
    mallocCount = 0;
    reallocCount = 0;
    freeCount = 0;
}
} // namespace SimpleArrayTesting
} // namespace rive

#endif
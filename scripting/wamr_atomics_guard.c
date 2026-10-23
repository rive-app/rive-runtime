#include "bh_atomic.h"

#if BH_ATOMIC_32_IS_ATOMIC != 1
#error                                                                         \
    "WAMR's suspend flags must be atomic, as other threads interrupt calls; delete a WAMR checkout older than our patches"
#endif

/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_sampler.hpp"

namespace rive::ore
{

#if !defined(ORE_BACKEND_GL)

void Sampler::onRefCntReachedZero() const { delete this; }

#endif // !ORE_BACKEND_GL

} // namespace rive::ore

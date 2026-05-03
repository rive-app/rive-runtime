/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_sampler.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_D3D11) && !defined(ORE_BACKEND_D3D12)

void Sampler::onRefCntReachedZero() const { delete this; }

#endif // ORE_BACKEND_D3D11 && !ORE_BACKEND_D3D12

} // namespace rive::ore

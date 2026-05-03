/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_sampler.hpp"

namespace rive::ore
{

void Sampler::onRefCntReachedZero() const
{
    // wgpu::Sampler RAII destructor releases the GPU resource.
    delete this;
}

} // namespace rive::ore

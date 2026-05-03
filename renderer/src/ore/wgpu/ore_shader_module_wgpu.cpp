/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_shader_module.hpp"

namespace rive::ore
{

void ShaderModule::onRefCntReachedZero() const
{
    // wgpu::ShaderModule RAII destructor releases the GPU resource.
    delete this;
}

} // namespace rive::ore

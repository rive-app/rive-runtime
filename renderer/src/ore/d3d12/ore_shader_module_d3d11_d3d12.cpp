/*
 * Copyright 2025 Rive
 */

// Combined D3D11 + D3D12 ShaderModule implementation for Windows.
// Compiled when both ORE_BACKEND_D3D11 and ORE_BACKEND_D3D12 are active.

#if defined(ORE_BACKEND_D3D11) && defined(ORE_BACKEND_D3D12)

#include "rive/renderer/ore/ore_shader_module.hpp"

namespace rive::ore
{

void ShaderModule::onRefCntReachedZero() const { delete this; }

} // namespace rive::ore

#endif // ORE_BACKEND_D3D11 && ORE_BACKEND_D3D12

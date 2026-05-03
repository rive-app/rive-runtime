/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_shader_module.hpp"

namespace rive::ore
{

// --- Public method definitions (D3D12-only builds) ---
// When both D3D11 and D3D12 are compiled, the combined
// ore_context_d3d11_d3d12.cpp file provides these methods with dispatch.
#if defined(ORE_BACKEND_D3D12) && !defined(ORE_BACKEND_D3D11)

void ShaderModule::onRefCntReachedZero() const { delete this; }

#endif // D3D12-only

} // namespace rive::ore

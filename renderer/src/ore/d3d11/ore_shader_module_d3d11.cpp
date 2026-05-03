/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_shader_module.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_D3D11) && !defined(ORE_BACKEND_D3D12)

void ShaderModule::onRefCntReachedZero() const { delete this; }

#endif // ORE_BACKEND_D3D11 && !ORE_BACKEND_D3D12

} // namespace rive::ore

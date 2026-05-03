/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_D3D11) && !defined(ORE_BACKEND_D3D12)

void Pipeline::onRefCntReachedZero() const { delete this; }

void BindGroupLayout::onRefCntReachedZero() const { delete this; }

#endif // ORE_BACKEND_D3D11 && !ORE_BACKEND_D3D12

} // namespace rive::ore

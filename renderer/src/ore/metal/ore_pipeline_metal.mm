/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"

namespace rive::ore
{

#if !defined(ORE_BACKEND_GL)

void Pipeline::onRefCntReachedZero() const { delete this; }

void BindGroupLayout::onRefCntReachedZero() const { delete this; }

#endif // !ORE_BACKEND_GL

} // namespace rive::ore

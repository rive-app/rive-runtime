/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_shader_module.hpp"

namespace rive::ore
{

#if !defined(ORE_BACKEND_GL)

void ShaderModule::onRefCntReachedZero() const { delete this; }

#endif // !ORE_BACKEND_GL

} // namespace rive::ore

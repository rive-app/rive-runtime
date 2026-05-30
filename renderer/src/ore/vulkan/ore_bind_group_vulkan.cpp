/*
 * Copyright 2025 Rive
 */

#include "ore_bind_group_vulkan.hpp"

namespace rive::ore
{

// Pool destruction handles the set, no per-set free.
BindGroupVulkan::~BindGroupVulkan() = default;

} // namespace rive::ore

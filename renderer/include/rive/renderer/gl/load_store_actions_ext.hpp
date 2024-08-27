/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "rive/enum_bitset.hpp"
#include <iostream>

namespace rive::gpu
{
// When using EXT_shader_pixel_local_storage, we have to emulate the render pass load/store actions
// using a shader. These bits define specific actions that can be turned on or off in that shader.
enum class LoadStoreActionsEXT
{
    none = 0,
    clearColor = 1,
    loadColor = 2,
    storeColor = 4,
    clearCoverage = 8,
    clearClip = 16,
};
RIVE_MAKE_ENUM_BITSET(LoadStoreActionsEXT)

// Determines the specific load actions that need to be emulated for the given render pass, and
// unpacks the clear color, if required.
LoadStoreActionsEXT BuildLoadActionsEXT(const gpu::FlushDescriptor&,
                                        std::array<float, 4>* clearColor4f);

// Appends load_store_ext.glsl to the stream, with the appropriate #defines prepended.
std::ostream& BuildLoadStoreEXTGLSL(std::ostream&, LoadStoreActionsEXT);
} // namespace rive::gpu

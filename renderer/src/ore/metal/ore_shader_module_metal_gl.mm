/*
 * Copyright 2025 Rive
 */

// Combined Metal + GL ShaderModule implementation for macOS.
// Compiled when both ORE_BACKEND_METAL and ORE_BACKEND_GL are active.

#if defined(ORE_BACKEND_METAL) && defined(ORE_BACKEND_GL)

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"

namespace rive::ore
{

void ShaderModule::onRefCntReachedZero() const
{
    if (m_glShader != 0)
    {
        glDeleteShader(m_glShader);
    }
    delete this;
}

} // namespace rive::ore

#endif // ORE_BACKEND_METAL && ORE_BACKEND_GL

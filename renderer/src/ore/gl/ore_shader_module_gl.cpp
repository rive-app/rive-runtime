/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_GL) && !defined(ORE_BACKEND_METAL) &&                  \
    !defined(ORE_BACKEND_VK)

void ShaderModule::onRefCntReachedZero() const
{
    if (m_glShader != 0)
    {
        glDeleteShader(m_glShader);
    }
    delete this;
}

#endif // ORE_BACKEND_GL && !ORE_BACKEND_METAL

} // namespace rive::ore

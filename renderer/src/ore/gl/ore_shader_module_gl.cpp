/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "ore_shader_module_gl.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_GL)

void ShaderModuleGL::onRefCntReachedZero() const
{
    if (m_glShader != 0)
    {
        glDeleteShader(m_glShader);
    }
    delete this;
}

#endif // ORE_BACKEND_GL

} // namespace rive::ore

/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "ore_shader_module_gl.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_GL)

ShaderModuleGL::~ShaderModuleGL()
{
    if (m_glShader != 0)
    {
        glDeleteShader(m_glShader);
    }
}

#endif // ORE_BACKEND_GL

} // namespace rive::ore

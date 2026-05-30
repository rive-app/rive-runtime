/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "ore_pipeline_gl.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_GL)

PipelineGL::~PipelineGL()
{
    if (m_glProgram != 0)
    {
        glDeleteProgram(m_glProgram);
    }
}

#endif // ORE_BACKEND_GL

} // namespace rive::ore

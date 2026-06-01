/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "ore_sampler_gl.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_GL)

SamplerGL::~SamplerGL()
{
    if (m_glSampler != 0)
    {
        GLuint samp = m_glSampler;
        glDeleteSamplers(1, &samp);
    }
}

#endif // ORE_BACKEND_GL

} // namespace rive::ore

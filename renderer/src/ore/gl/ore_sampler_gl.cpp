/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_GL) && !defined(ORE_BACKEND_METAL) &&                  \
    !defined(ORE_BACKEND_VK)

void Sampler::onRefCntReachedZero() const
{
    if (m_glSampler != 0)
    {
        GLuint samp = m_glSampler;
        glDeleteSamplers(1, &samp);
    }
    delete this;
}

#endif // ORE_BACKEND_GL && !ORE_BACKEND_METAL

} // namespace rive::ore

/*
 * Copyright 2025 Rive
 */

// Combined Metal + GL Sampler implementation for macOS.
// Compiled when both ORE_BACKEND_METAL and ORE_BACKEND_GL are active.

#if defined(ORE_BACKEND_METAL) && defined(ORE_BACKEND_GL)

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"

namespace rive::ore
{

void Sampler::onRefCntReachedZero() const
{
    if (m_glSampler != 0)
    {
        GLuint samp = m_glSampler;
        glDeleteSamplers(1, &samp);
    }
    delete this;
}

} // namespace rive::ore

#endif // ORE_BACKEND_METAL && ORE_BACKEND_GL

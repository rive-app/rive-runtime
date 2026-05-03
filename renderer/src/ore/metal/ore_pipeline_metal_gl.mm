/*
 * Copyright 2025 Rive
 */

// Combined Metal + GL Pipeline implementation for macOS.
// Compiled when both ORE_BACKEND_METAL and ORE_BACKEND_GL are active.

#if defined(ORE_BACKEND_METAL) && defined(ORE_BACKEND_GL)

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"

namespace rive::ore
{

void Pipeline::onRefCntReachedZero() const
{
    if (m_glProgram != 0)
    {
        glDeleteProgram(m_glProgram);
    }
    delete this;
}

void BindGroupLayout::onRefCntReachedZero() const { delete this; }

} // namespace rive::ore

#endif // ORE_BACKEND_METAL && ORE_BACKEND_GL

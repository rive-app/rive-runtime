/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"

namespace rive::ore
{

#if defined(ORE_BACKEND_GL) && !defined(ORE_BACKEND_METAL) &&                  \
    !defined(ORE_BACKEND_VK)

void Pipeline::onRefCntReachedZero() const
{
    if (m_glProgram != 0)
    {
        glDeleteProgram(m_glProgram);
    }
    delete this;
}

void BindGroupLayout::onRefCntReachedZero() const { delete this; }

#endif // ORE_BACKEND_GL && !ORE_BACKEND_METAL && !ORE_BACKEND_VK

} // namespace rive::ore

/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_context.hpp"

namespace rive::ore
{

#if !defined(ORE_BACKEND_GL)

void ShaderModule::onRefCntReachedZero() const
{
    if (m_vkShaderModule != VK_NULL_HANDLE &&
        m_vkDestroyShaderModule != nullptr)
    {
        m_vkDestroyShaderModule(m_vkDevice, m_vkShaderModule, nullptr);
    }
    delete this;
}

#endif // !ORE_BACKEND_GL

} // namespace rive::ore

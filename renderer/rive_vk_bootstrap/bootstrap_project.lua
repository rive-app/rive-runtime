-- Compile vk-bootstrap into the current project, and configure the project for Vulkan.
if not _OPTIONS['with_vulkan'] then
    error('script called without `--with_vulkan` option')
end

if not vulkan_headers or not vulkan_memory_allocator then
    error('Please `dofile` packages/runtime/renderer/premake5_pls_renderer.lua first.')
end

local dependency = require('dependency')
vk_bootstrap = dependency.github('charles-lunarg/vk-bootstrap', 'v1.3.292')

includedirs({ 'include' })

externalincludedirs({
    vulkan_headers .. '/include',
    vulkan_memory_allocator .. '/include',
    vk_bootstrap .. '/src',
})

files({
    'rive_vk_bootstrap.cpp',
    vk_bootstrap .. '/src/VkBootstrap.cpp',
})

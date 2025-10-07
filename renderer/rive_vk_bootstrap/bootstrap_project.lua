-- Compile vk-bootstrap into the current project, and configure the project for Vulkan.
if not _OPTIONS['with_vulkan'] then
    error('script called without `--with_vulkan` option')
end

if not vulkan_headers or not vulkan_memory_allocator then
    error('Please `dofile` packages/runtime/renderer/premake5_pls_renderer.lua first.')
end

local dependency = require('dependency')

includedirs({ 'include' })

externalincludedirs({
    vulkan_headers .. '/include',
    vulkan_memory_allocator .. '/include',
})

files({
    'include/**.hpp',
    'src/*.cpp',
    'src/*.hpp',
})

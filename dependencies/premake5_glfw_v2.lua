dofile('rive_build_config.lua')

local dependency = require('dependency')
glfw = dependency.github('glfw/glfw', '3.4')

project('glfw3')
do
    kind('StaticLib')

    includedirs({ glfw .. '/include' })

    files({
        glfw .. '/src/internal.h',
        glfw .. '/src/platform.h',
        glfw .. '/src/mappings.h',
        glfw .. '/src/context.c',
        glfw .. '/src/init.c',
        glfw .. '/src/input.c',
        glfw .. '/src/monitor.c',
        glfw .. '/src/platform.c',
        glfw .. '/src/vulkan.c',
        glfw .. '/src/window.c',
        glfw .. '/src/egl_context.c',
        glfw .. '/src/osmesa_context.c',
        glfw .. '/src/null_platform.h',
        glfw .. '/src/null_joystick.h',
        glfw .. '/src/null_init.c',
        glfw .. '/src/null_monitor.c',
        glfw .. '/src/null_window.c',
        glfw .. '/src/null_joystick.c',
    })

    filter({ 'system:windows' })
    do
        defines({
            '_GLFW_WIN32',
            'UNICODE',
            '_UNICODE',
        })

        files({
            glfw .. '/src/win32_time.h',
            glfw .. '/src/win32_thread.h',
            glfw .. '/src/win32_module.c',
            glfw .. '/src/win32_time.c',
            glfw .. '/src/win32_thread.c',
            glfw .. '/src/win32_platform.h',
            glfw .. '/src/win32_joystick.h',
            glfw .. '/src/win32_init.c',
            glfw .. '/src/win32_joystick.c',
            glfw .. '/src/win32_monitor.c',
            glfw .. '/src/win32_window.c',
            glfw .. '/src/wgl_context.c',
        })
    end

    filter({ 'system:macosx' })
    do
        defines({ '_GLFW_COCOA' })
        removebuildoptions({ '-fobjc-arc' })

        files({
            glfw .. '/src/cocoa_time.h',
            glfw .. '/src/cocoa_time.c',
            glfw .. '/src/posix_thread.h',
            glfw .. '/src/posix_module.c',
            glfw .. '/src/posix_thread.c',
            glfw .. '/src/cocoa_platform.h',
            glfw .. '/src/cocoa_joystick.h',
            glfw .. '/src/cocoa_init.m',
            glfw .. '/src/cocoa_joystick.m',
            glfw .. '/src/cocoa_monitor.m',
            glfw .. '/src/cocoa_window.m',
            glfw .. '/src/nsgl_context.m',
        })
    end

    filter({ 'system:linux' })
    do
        defines({
            '_GLFW_X11',
            '_DEFAULT_SOURCE',
        })

        files({
            glfw .. '/src/posix_time.h',
            glfw .. '/src/posix_thread.h',
            glfw .. '/src/posix_module.c',
            glfw .. '/src/posix_time.c',
            glfw .. '/src/posix_thread.c',
            glfw .. '/src/x11_platform.h',
            glfw .. '/src/xkb_unicode.h',
            glfw .. '/src/x11_init.c',
            glfw .. '/src/x11_monitor.c',
            glfw .. '/src/x11_window.c',
            glfw .. '/src/xkb_unicode.c',
            glfw .. '/src/glx_context.c',
            glfw .. '/src/linux_joystick.h',
            glfw .. '/src/linux_joystick.c',
            glfw .. '/src/posix_poll.h',
            glfw .. '/src/posix_poll.c',
        })
    end

    filter({ 'system:not windows', 'system:not macosx', 'system:not linux' })
    do
        -- Don't build GLFW on mobile or web platforms. It's integrated into
        -- emscripten and unavailable on mobile.
        kind('None')
    end
end

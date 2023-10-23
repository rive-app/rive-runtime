workspace 'rive'
configurations {'debug', 'release'}

require 'setup_compiler'

-- Are we in the "rive-pls" or "rive" repository?
local handle = io.popen('git remote -v')
local git_remote = handle:read('*a')
handle:close()
if string.find(git_remote, 'rive%-pls') then
    -- In rive-pls. Rive runtime is a submodule.
    RIVE_RUNTIME_DIR = path.getabsolute('../submodules/rive-cpp')
else
    -- In rive. Rive runtime is further up the tree.
    RIVE_RUNTIME_DIR = path.getabsolute('../../runtime')
end

filter 'system:windows or macosx or linux'
do
    -- Define RIVE_DESKTOP_GL outside of a project so that it also gets defined for consumers. It is
    -- the responsibility of consumers to call gladLoadCustomLoader() when RIVE_DESKTOP_GL is
    -- defined.
    defines {'RIVE_DESKTOP_GL'}
end

filter 'system:android'
do
    defines {'RIVE_GLES'}
end

filter {'system:ios', 'options:variant=system'}
do
    defines {'RIVE_IOS'}
end

newoption {
    trigger = "with-dawn",
    description = "compile in support for webgpu via dawn",
}
filter {"options:with-dawn"}
do
    defines {'RIVE_DAWN'}
end

newoption {
    trigger = "with-webgpu",
    description = "compile in native support for webgpu",
}
filter {"options:with-webgpu"}
do
    defines {'RIVE_WEBGPU'}
end

filter {'system:ios', 'options:variant=simulator'}
do
    defines {'RIVE_IOS_SIMULATOR'}
end

filter 'system:emscripten'
do
    defines {'RIVE_WEBGL'}
end

newoption {
    trigger = 'obfuscate',
    description = 'force-include ../obfuscator/pls_renames.h to obfuscate PLS variable names'
}
filter {'options:obfuscate'}
do
    forceincludes {'../obfuscator/pls_renames.h'}
end

filter {}

-- Minify PLS shaders, and precompile them into metal libraries if targeting ios or macosx.
newoption {
    trigger = 'human-readable-shaders',
    description = "don't minimize or obfuscate shaders"
}
project 'rive_pls_shaders'
do
    kind 'Makefile'
    local makecommand = '@make -C ' .. path.getabsolute('../renderer/shaders')
    if _OPTIONS['human-readable-shaders'] then
        makecommand = makecommand .. ' FLAGS=--human-readable'
    end

    cleancommands {makecommand .. ' clean'}
    buildcommands {makecommand .. ' minify'}
    rebuildcommands {makecommand .. ' minify'}

    filter 'system:macosx'
    do
        buildcommands {makecommand .. ' rive_pls_macosx_metallib'}
        rebuildcommands {makecommand .. ' rive_pls_macosx_metallib'}
    end

    filter {'system:ios', 'options:variant=system'}
    do
        buildcommands {makecommand .. ' rive_pls_ios_metallib'}
        rebuildcommands {makecommand .. ' rive_pls_ios_metallib'}
    end

    filter {'system:ios', 'options:variant=simulator'}
    do
        buildcommands {makecommand .. ' rive_pls_ios_simulator_metallib'}
        rebuildcommands {makecommand .. ' rive_pls_ios_simulator_metallib'}
    end

    filter {'options:with-dawn or with-webgpu'}
    do
        buildcommands {makecommand .. ' spirv'}
        rebuildcommands {makecommand .. ' spirv'}
    end

    filter 'system:windows'
    do
        architecture 'x64'
    end
end

newoption {
    trigger = 'nop-obj-c',
    description = "include Metal classes, but as no-ops (for compilers that don't support Obj-C)"
}
newoption {
    trigger = 'no-rive-decoders',
    description = "don't use the rive_decoders library (built-in image decoding will fail)"
}
newoption {
    trigger = 'universal-release',
    description = "(Apple only): build a universal binary to release to the store"
}
project 'rive_pls_renderer'
do
    dependson 'rive_pls_shaders'
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++17'
    targetdir '%{cfg.buildcfg}'
    objdir 'obj/%{cfg.buildcfg}'
    includedirs {
        '../include',
        '../glad',
        '../renderer',
        RIVE_RUNTIME_DIR .. '/include',
    }
    flags {'FatalWarnings'}

    files {
        '../renderer/*.cpp',
        '../renderer/decoding/*.cpp',
    }

    -- The Visual Studio clang toolset doesn't recognize -ffp-contract.
    filter 'system:not windows'
    do
        buildoptions {
            '-ffp-contract=on',
            '-fassociative-math'
        }
    end

    filter 'system:windows or macosx or linux or android'
    do
        files {
            '../renderer/gl/buffer_ring_gl.cpp',
            '../renderer/gl/gl_state.cpp',
            '../renderer/gl/gl_utils.cpp',
            '../renderer/gl/load_store_actions_ext.cpp',
            '../renderer/gl/pls_render_buffer_gl_impl.cpp',
            '../renderer/gl/pls_render_context_gl_impl.cpp',
            '../renderer/gl/pls_render_target_gl.cpp'
        }
    end

    filter 'system:windows or macosx or linux'
    do
        files {
            '../renderer/gl/pls_impl_webgl.cpp', -- Emulate WebGL with ANGLE.
            '../renderer/gl/pls_impl_rw_texture.cpp',
            '../glad/glad.c',
            '../glad/glad_custom.c'
        } -- GL loader library for ANGLE.
    end

    filter 'system:android'
    do
        files {
            '../renderer/gl/load_gles_extensions.cpp',
            '../renderer/gl/pls_impl_ext_native.cpp',
            '../renderer/gl/pls_impl_framebuffer_fetch.cpp'
        }

        filter {'system:android', 'options:arch=x86'}
        do
            targetdir '%{cfg.system}/x86/%{cfg.buildcfg}/lib'
            objdir '%{cfg.system}/x86/%{cfg.buildcfg}/obj'
        end

        filter {'system:android', 'options:arch=x64'}
        do
            targetdir '%{cfg.system}/x64/%{cfg.buildcfg}/lib'
            objdir '%{cfg.system}/x64/%{cfg.buildcfg}/obj'
        end

        filter {'system:android', 'options:arch=arm'}
        do
            targetdir '%{cfg.system}/arm/%{cfg.buildcfg}/lib'
            objdir '%{cfg.system}/arm/%{cfg.buildcfg}/obj'
        end

        filter {'system:android', 'options:arch=arm64'}
        do
            targetdir '%{cfg.system}/arm64/%{cfg.buildcfg}/lib'
            objdir '%{cfg.system}/arm64/%{cfg.buildcfg}/obj'
        end
    end

    filter {'system:macosx or ios', 'options:not nop-obj-c'}
    do
        files {'../renderer/metal/*.mm'}
        buildoptions {'-fobjc-arc'}
    end

    filter {"options:with-dawn"}
    do
        includedirs {
            "../dependencies/dawn/include",
            "../dependencies/dawn/out/release/gen/include",
        }
        files {
            "../dependencies/dawn/out/release/gen/src/dawn/webgpu_cpp.cpp",
        }
    end

    filter {"options:with-webgpu or with-dawn"}
    do
        files {
            "../renderer/webgpu/**.cpp",
            '../renderer/gl/load_store_actions_ext.cpp',
        }
    end

    filter {'options:nop-obj-c'}
    do
        files {'../renderer/metal/pls_metal_nop.cpp'}
    end

    filter {'options:not no-rive-decoders'}
    do
        includedirs {RIVE_RUNTIME_DIR .. '/decoders/include'}
        defines {'RIVE_DECODERS'}
    end

    filter 'system:windows'
    do
        architecture 'x64'
        files {'../renderer/d3d/*.cpp'}
    end

    if os.host() == 'macosx' then
        iphoneos_sysroot = os.outputof('xcrun --sdk iphoneos --show-sdk-path')
        iphonesimulator_sysroot = os.outputof('xcrun --sdk iphonesimulator --show-sdk-path')

        filter 'system:ios'
        do
            buildoptions {'-Wno-deprecated-declarations'} -- Suppress warnings that GL is deprecated.
        end

        filter {'system:ios', 'options:variant=system'}
        do
            targetdir 'iphoneos_%{cfg.buildcfg}'
            objdir 'obj/iphoneos_%{cfg.buildcfg}'
            buildoptions {
                '--target=arm64-apple-ios13.0.0',
                '-mios-version-min=13.0.0',
                '-isysroot ' .. iphoneos_sysroot
            }
        end

        filter {'system:ios', 'options:variant=simulator'}
        do
            targetdir 'iphonesimulator_%{cfg.buildcfg}'
            objdir 'obj/iphonesimulator_%{cfg.buildcfg}'
            buildoptions {
                '--target=arm64-apple-ios13.0.0-simulator',
                '-mios-version-min=13.0.0',
                '-isysroot ' .. iphonesimulator_sysroot
            }
        end

        filter {'system:macosx', 'options:universal-release'}
        do
            buildoptions {
                '-arch x86_64',
                '-arch arm64',
            }
        end

        filter {'system:ios', 'options:variant=system', 'options:universal-release'}
        do
            buildoptions {
                '-arch arm64',
            }
        end

        filter {'system:ios', 'options:variant=simulator', 'options:universal-release'}
        do
            buildoptions {
                '-arch x86_64',
                '-arch arm64',
            }
        end
    end

    filter 'system:emscripten'
    do
        targetdir(_OPTIONS['emsdk'] .. '_%{cfg.buildcfg}')
        objdir(_OPTIONS['emsdk'] .. '_%{cfg.buildcfg}')
        files {'../renderer/gl/pls_impl_webgl.cpp'}
        buildoptions {'-pthread'}
    end

    filter 'configurations:debug'
    do
        defines {'DEBUG'}
        symbols 'On'
    end

    filter 'configurations:release'
    do
        defines {'RELEASE'}
        defines {'NDEBUG'}
        optimize 'On'
    end
end

newoption {
    trigger = 'variant',
    value = 'type',
    description = 'Choose a particular variant to build',
    allowed = {
        {'system', 'Builds the static library for the provided system'},
        {'simulator', 'Builds for an emulator/simulator for the provided system'}
    },
    default = 'system'
}

newoption {
    trigger = 'arch',
    value = 'ABI',
    description = 'The ABI with the right toolchain for this build, generally with Android',
    allowed = {
        {'x86'},
        {'x64'},
        {'arm'},
        {'arm64'}
    }
}

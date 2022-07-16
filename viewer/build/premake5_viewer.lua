workspace 'rive'
configurations {
    'debug',
    'release'
}

dependencies = os.getenv('DEPENDENCIES')

rive = '../../'
rive_skia = '../../skia'
skia = dependencies .. '/skia'
libpng = dependencies .. '/libpng'

project 'rive_viewer'
do
    kind 'ConsoleApp'
    language 'C++'
    cppdialect 'C++17'
    toolset 'clang'
    targetdir('%{cfg.system}/bin/%{cfg.buildcfg}/' .. _OPTIONS.renderer .. '/' .. _OPTIONS.graphics)
    objdir('%{cfg.system}/obj/%{cfg.buildcfg}/' .. _OPTIONS.renderer .. '/' .. _OPTIONS.graphics)
    includedirs {
        '../include',
        rive .. '/include',
        dependencies,
        dependencies .. '/sokol',
        dependencies .. '/imgui'
    }

    links {
        'rive'
    }

    libdirs {
        rive .. '/build/%{cfg.system}/bin/%{cfg.buildcfg}'
    }

    files {
        '../src/**.cpp',
        dependencies .. '/imgui/imgui.cpp',
        dependencies .. '/imgui/imgui_widgets.cpp',
        dependencies .. '/imgui/imgui_tables.cpp',
        dependencies .. '/imgui/imgui_draw.cpp'
    }

    buildoptions {
        '-Wall',
        '-fno-exceptions',
        '-fno-rtti'
    }

    filter {
        'system:macosx'
    }
    do
        links {
            'Cocoa.framework',
            'IOKit.framework',
            'CoreVideo.framework',
            'OpenGL.framework'
        }
        files {
            '../src/**.m',
            '../src/**.mm'
        }
    end

    filter {
        'system:macosx',
        'options:graphics=gl'
    }
    do
        links {
            'OpenGL.framework'
        }
    end

    filter {
        'system:macosx',
        'options:graphics=metal'
    }
    do
        links {
            'Metal.framework',
            'MetalKit.framework',
            'QuartzCore.framework'
        }
    end

    filter {
        'options:renderer=skia',
        'options:graphics=gl'
    }
    do
        defines {
            'SK_GL',
            'SOKOL_GLCORE33'
        }
        files {
            '../src/skia/viewer_skia_gl.cpp'
        }
        libdirs {
            skia .. '/out/gl/%{cfg.buildcfg}'
        }
    end

    filter {
        'options:renderer=skia',
        'options:graphics=metal'
    }
    do
        defines {
            'SK_METAL',
            'SOKOL_METAL'
        }
        libdirs {
            skia .. '/out/metal/%{cfg.buildcfg}'
        }
    end

    filter {
        'options:renderer=skia',
        'options:graphics=d3d'
    }
    do
        defines {
            'SK_DIRECT3D'
        }
        libdirs {
            skia .. '/out/d3d/%{cfg.buildcfg}'
        }
    end

    filter {
        'options:renderer=skia'
    }
    do
        includedirs {
            skia,
            skia .. '/include/core',
            skia .. '/include/effects',
            skia .. '/include/gpu',
            skia .. '/include/config',
            rive_skia .. '/renderer/include'
        }
        defines {
            'RIVE_RENDERER_SKIA'
        }
        libdirs {
            rive_skia .. '/renderer/build/%{cfg.system}/bin/%{cfg.buildcfg}',
            '../../../../third_party/harfbuzz/build/%{cfg.buildcfg}/bin'
        }
        links {
            'skia',
            'rive_skia_renderer',
            'rive_harfbuzz'
        }
    end

    filter 'configurations:debug'
    do
        buildoptions {
            '-g'
        }
        defines {
            'DEBUG'
        }
        symbols 'On'
    end

    filter 'configurations:release'
    do
        buildoptions {
            '-flto=full'
        }
        defines {
            'RELEASE'
        }
        defines {
            'NDEBUG'
        }
        optimize 'On'
    end

    -- CLI config options
    newoption {
        trigger = 'graphics',
        value = 'gl',
        description = 'The graphics api to use.',
        allowed = {
            {
                'gl'
            },
            {
                'metal'
            },
            {
                'd3d'
            }
        }
    }

    newoption {
        trigger = 'renderer',
        value = 'skia',
        description = 'The renderer to use.',
        allowed = {
            {
                'skia'
            }
        }
    }
end

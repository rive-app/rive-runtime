workspace('rive')
configurations({ 'debug', 'release' })

require('setup_compiler')

RIVE_RUNTIME_DIR = os.getenv('RIVE_RUNTIME_DIR') or '../../../'
SKIA_DIR_NAME = os.getenv('SKIA_DIR_NAME') or 'skia'

BASE_DIR = path.getabsolute(RIVE_RUNTIME_DIR .. '/build')
location('./')
dofile(path.join(BASE_DIR, 'premake5.lua'))

BASE_DIR = path.getabsolute(RIVE_RUNTIME_DIR .. '/skia/renderer/build')
location('./')
dofile(path.join(BASE_DIR, 'premake5.lua'))

project('rive_thumbnail_generator')
kind('ConsoleApp')
language('C++')
cppdialect('C++17')
targetdir('%{cfg.system}/bin/%{cfg.buildcfg}')
objdir('%{cfg.system}/obj/%{cfg.buildcfg}')

includedirs({
    RIVE_RUNTIME_DIR .. '/include',
    RIVE_RUNTIME_DIR .. '/skia/renderer/include',
    RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME,
    RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/include/core',
    RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/include/effects',
    RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/include/gpu',
    RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/include/config',
})

if os.host() == 'macosx' then
    links({
        'Cocoa.framework',
        'rive',
        'skia',
        'rive_skia_renderer',
        'rive_harfbuzz',
        'rive_sheenbidi',
    })
else
    links({
        'rive',
        'rive_skia_renderer',
        'skia',
        'GL',
        'rive_harfbuzz',
        'rive_sheenbidi',
    })
end

libdirs({
    '../../../build/%{cfg.system}/bin/%{cfg.buildcfg}',
    '../../dependencies/skia/out/static',
    '../../renderer/build/%{cfg.system}/bin/%{cfg.buildcfg}',
})

files({ '../src/**.cpp' })

buildoptions({ '-Wall', '-fno-exceptions', '-fno-rtti' })

filter('configurations:debug')
defines({ 'DEBUG' })
symbols('On')

filter('configurations:release')
defines({ 'RELEASE' })
defines({ 'NDEBUG' })
optimize('On')

filter({ 'options:with_rive_layout' })
    do
        defines({ 'YOGA_EXPORT=' })
        includedirs({ yoga })
        links({
            'rive_yoga',
        })
    end

-- Clean Function --
newaction({
    trigger = 'clean',
    description = 'clean the build',
    execute = function()
        print('clean the build...')
        os.rmdir('./bin')
        os.rmdir('./obj')
        os.remove('Makefile')
        -- no wildcards in os.remove, so use shell
        os.execute('rm *.make')
        print('build cleaned')
    end,
})

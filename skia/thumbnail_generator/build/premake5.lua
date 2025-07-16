dofile('rive_build_config.lua')

RIVE_RUNTIME_DIR = os.getenv('RIVE_RUNTIME_DIR') or '../../../'
SKIA_DIR_NAME = os.getenv('SKIA_DIR_NAME') or 'skia'

dofile(path.join(RIVE_RUNTIME_DIR, 'premake5_v2.lua'))
BASE_DIR = path.getabsolute(RIVE_RUNTIME_DIR .. '/skia/renderer')
dofile(path.join(BASE_DIR, 'premake5_v2.lua'))


project('rive_thumbnail_generator')
do 
    kind('ConsoleApp')
    exceptionhandling('On')
    rtti('On')

    includedirs({
        RIVE_RUNTIME_DIR .. '/include',
        RIVE_RUNTIME_DIR .. '/skia/renderer/include',
        RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME,
        RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/include/core',
        RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/include/effects',
        RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/include/gpu',
        RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/include/config',
        '/usr/local/include',
        '/usr/include',
        yoga,
    })
    defines({ 'YOGA_EXPORT=' })

    if os.host() == 'macosx' then
        links({
            'Cocoa.framework',
            'skia',
            'rive',
            'rive_skia_renderer',
            'rive_harfbuzz',
            'rive_sheenbidi',
            'rive_yoga',
        })
    else
        links({
            'skia',
            'GL',
            'rive',
            'rive_skia_renderer',
            'rive_harfbuzz',
            'rive_sheenbidi',
            'rive_yoga',
        })
    end

    libdirs({
        -- hmm nothing here? 
        -- RIVE_RUNTIME_DIR .. '/build/%{cfg.system}/bin/%{cfg.buildcfg}',
        -- RIVE_RUNTIME_DIR .. '/dependencies/%{cfg.system}/cache/bin/%{cfg.buildcfg}',
        RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/out/static',
        RIVE_RUNTIME_DIR .. '/skia/renderer/build/%{cfg.system}/bin/%{cfg.buildcfg}',
        '/usr/local/lib',
        '/usr/lib',
    })
    files({ '../src/**.cpp',})

end

dofile('rive_build_config.lua')

SKIA_DIR = os.getenv('SKIA_DIR')
dependencies = os.getenv('DEPENDENCIES')

if SKIA_DIR == nil and dependencies ~= nil then
    SKIA_DIR = dependencies .. '/skia'
else
    if SKIA_DIR == nil then
        SKIA_DIR = 'skia'
    end
    SKIA_DIR = '../dependencies/' .. SKIA_DIR
end

project('rive_skia_renderer')
do
    kind('StaticLib')
    includedirs({ 'include', '../../cg_renderer/include', '../../include' })

    libdirs({ '../../build/%{cfg.system}/bin/' .. RIVE_BUILD_CONFIG })

    files({ 'src/**.cpp' })

    flags({ 'FatalCompileWarnings' })

    filter({ 'system:macosx or linux or windows or ios' })
    do
        includedirs({ SKIA_DIR })
        libdirs({ SKIA_DIR .. '/out/static' })
    end

    filter({ 'system:android' })
    do
        includedirs({ SKIA_DIR })

        filter({ 'system:android', 'options:arch=x86' })
        do
            libdirs({ SKIA_DIR .. '/out/x86' })
        end

        filter({ 'system:android', 'options:arch=x64' })
        do
            libdirs({ SKIA_DIR .. '/out/x64' })
        end

        filter({ 'system:android', 'options:arch=arm' })
        do
            libdirs({ SKIA_DIR .. '/out/arm' })
        end

        filter({ 'system:android', 'options:arch=arm64' })
        do
            libdirs({ SKIA_DIR .. '/out/arm64' })
        end
    end

    filter({ 'options:with_rive_text' })
    do
        defines({ 'WITH_RIVE_TEXT' })
    end
    filter({ 'options:with_rive_audio=system' })
    do
        defines({ 'WITH_RIVE_AUDIO' })
    end
    filter({ 'options:with_rive_audio=external' })
    do
        defines({
            'WITH_RIVE_AUDIO',
            'EXTERNAL_RIVE_AUDIO_ENGINE',
            'MA_NO_DEVICE_IO',
        })
    end
end

newoption({ trigger = 'with_rive_text', description = 'Enables text experiments' })

newoption({
    trigger = 'with_rive_audio',
    value = 'disabled',
    description = 'The audio mode to use.',
    allowed = { { 'disabled' }, { 'system' }, { 'external' } },
})

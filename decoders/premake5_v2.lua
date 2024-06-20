dofile('rive_build_config.lua')

rive = path.getabsolute('../')

dofile(rive .. '/dependencies/premake5_libpng_v2.lua')
dofile(rive .. '/dependencies/premake5_libjpeg_v2.lua')

project('rive_decoders')
do
    dependson('libpng', 'zlib', 'libjpeg')
    kind('StaticLib')
    flags({ 'FatalWarnings' })

    includedirs({ 'include', '../include', libpng, libjpeg })

    files({ 'src/**.cpp' })

    filter({ 'options:not no-libjpeg-renames' })
    do
        includedirs({
            rive .. '/dependencies',
        })
        forceincludes({ 'rive_libjpeg_renames.h' })
    end
end

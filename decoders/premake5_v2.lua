dofile('setup_compiler.lua')

rive = path.getabsolute('../')

dofile(rive .. '/dependencies/premake5_libpng_v2.lua')

project('rive_decoders')
do
    dependson('libpng')
    kind('StaticLib')
    flags({ 'FatalWarnings' })

    includedirs({ 'include', '../include', libpng })

    files({ 'src/**.cpp' })
end

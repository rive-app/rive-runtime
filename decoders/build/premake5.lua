dofile('rive_build_config.lua')

rive = path.getabsolute('../../')

dofile(rive .. '/dependencies/premake5_libpng_v2.lua')

project('rive_decoders')
dependson('libpng')
kind('StaticLib')
language('C++')
targetdir('%{cfg.buildcfg}')
objdir('obj/%{cfg.buildcfg}')
fatalwarnings { "All" }

includedirs({ '../include', '../../include', libpng })

files({ '../src/**.cpp' })

filter({ 'system:windows' })
do
    architecture('x64')
end

filter('configurations:debug')
do
    defines({ 'DEBUG' })
    symbols('On')
end

filter('configurations:release')
do
    defines({ 'RELEASE' })
    defines({ 'NDEBUG' })
    optimize('On')
end

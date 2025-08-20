local dependency = require('dependency')
optick = dependency.github(RIVE_OPTICK_URL, RIVE_OPTICK_VERSION)
project('optick')
do
    kind('StaticLib')
    language('C++')
    cppdialect('C++11')
 
    files { optick .. "/src/**.cpp" }
    -- but exclude this one
    removefiles {
      optick .. "/src/optick_gpu.vulkan.cpp"
    }

    includedirs({ optick .. '/src' })
end

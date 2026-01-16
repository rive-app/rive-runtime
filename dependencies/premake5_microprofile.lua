local dependency = require('dependency')
microprofile = dependency.github(RIVE_MICROPROFILE_URL, RIVE_MICROPROFILE_VERSION)
project('microprofile')
do
    kind('StaticLib')
    language('C++')
    cppdialect('C++11')
 
    files { microprofile .. "/src/embed.c" }
    includedirs(microprofile)
end

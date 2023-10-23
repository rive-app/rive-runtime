workspace "rive"
configurations {"debug", "release"}

require 'setup_compiler'

project "rive_canvas2d_renderer"
do
    kind 'StaticLib'
    language "C++"
    cppdialect "C++17"
    floatingpoint "Fast" -- Enable FMAs
    targetdir "wasm_%{cfg.buildcfg}"
    objdir "obj/wasm_%{cfg.buildcfg}"
    includedirs {"../../runtime/include"}
    flags { "FatalWarnings" }
    files {"../canvas2d_renderer/*.cpp"}
    defines {"EMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0"}
    linkoptions {"-fno-rtti"}
    buildoptions {"-pthread"}

    filter "configurations:debug"
    do
        defines {"DEBUG"}
        symbols "On"
    end

    filter "configurations:release"
    do
        defines {"RELEASE"}
        defines {"NDEBUG"}
        optimize "On"
    end
end

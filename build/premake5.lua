workspace "rive"
    configurations {"debug", "release"}

project "rive"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    targetdir "%{cfg.system}/bin/%{cfg.buildcfg}"
    objdir "%{cfg.system}/obj/%{cfg.buildcfg}"
    includedirs {"../include"}

    files {"../src/**.cpp"}

    buildoptions {"-Wall", "-fno-exceptions", "-fno-rtti", "-Werror=format"}

    filter "system:windows"
        defines {"_USE_MATH_DEFINES"}

    filter "system:ios"
        buildoptions {"-arch armv7 -arch arm64 -arch x64 -isysroot " .. (os.getenv("IOS_SYSROOT") or "")}

    filter "configurations:debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:release"
        defines {"RELEASE"}
        defines {"NDEBUG"}
        optimize "On"

-- Clean Function --
newaction {
    trigger = "clean",
    description = "clean the build",
    execute = function()
        print("clean the build...")
        os.rmdir("./bin")
        os.rmdir("./obj")
        os.remove("Makefile")
        -- no wildcards in os.remove, so use shell
        os.execute("rm *.make")
        print("build cleaned")
    end
}

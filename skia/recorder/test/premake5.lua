-- require "lfs"
-- Clean Function --
newaction {
    trigger = "clean",
    description = "clean the build",
    execute = function()
        print("clean the build...")
        os.rmdir("build")
        os.remove("Makefile")
        -- no wildcards in os.remove, so use shell
        os.execute("rm *.make")
        print("build cleaned")
    end
}

workspace "rive_tests"
configurations {"debug"}

project("tests")
kind "ConsoleApp"
language "C++"
cppdialect "C++17"
targetdir "build/bin/%{cfg.buildcfg}"
objdir "build/obj/%{cfg.buildcfg}"

buildoptions {"-Wall", "-fno-rtti"}

includedirs {
    "./include",
    "../include",
    "../../../include",
    "../../renderer/include",
    "../../dependencies/glfw/include",
    "../../dependencies/skia",
    "../../dependencies/skia/include/core",
    "../../dependencies/skia/include/effects",
    "../../dependencies/skia/include/gpu",
    "../../dependencies/skia/include/config",
    "../../dependencies/FFmpeg",
    "../../dependencies/x264/include",
    "../../dependencies/libzip/lib",
    "/usr/local/include"
}

links {
    'stdc++',
    "AudioToolbox.framework",
    "AudioUnit.framework",
    "avcodec",
    "avformat",
    "avutil",
    "bz2",
    "Cocoa.framework",
    "CoreFoundation.framework",
    "CoreMedia.framework",
    "CoreServices.framework",
    "CoreVideo.framework",
    "glfw3",
    "iconv",
    "IOKit.framework",
    "lzma",
    "m",
    "rive_skia_renderer",
    "rive",
    "Security.framework",
    "skia",
    "swresample",
    "swscale",
    "VideoToolbox.framework",
    "x264", 
    "z",
    "zip"
}

libdirs {
    "../../../build/bin/%{cfg.buildcfg}",
    "../../dependencies/FFmpeg/libavcodec",
    "../../dependencies/FFmpeg/libavformat",
    "../../dependencies/FFmpeg/libavutil",
    "../../dependencies/FFmpeg/libswscale",
    "../../dependencies/FFmpeg/libswresample",
    "../../dependencies/x264/lib",
    "../../dependencies/glfw_build/src",
    "../../dependencies/skia/out/Static",
    "../../dependencies/libzip_build/lib",
    "../../renderer/build/bin/%{cfg.buildcfg}",
    "/usr/local/lib"
}


files {
    "../src/**.cpp", -- the Rive runtime source
    "../src/**.c", -- miniz source
    "./src/**.cpp" -- the tests
}

defines {"TESTING"}

filter "configurations:debug"
defines {"DEBUG"}
symbols "On"

--[[

-- Recursively iterate through all files in a dir
function dirtree(dir)

    assert(dir and dir ~= "", "Provide a directory")
    if string.sub(dir, -1) == "/" then
        dir = string.sub(dir, 1, -2)
    end

    local function yieldtree(dir)
        for entry in lfs.dir(dir) do
            if entry ~= "." and entry ~= ".." then
                entry = dir .. "/" .. entry
                local attr = lfs.attributes(entry)
                coroutine.yield(entry, attr)
                if attr.mode == "directory" then
                    yieldtree(entry)
                end
            end
        end
    end
    return coroutine.wrap(function()
        yieldtree(dir)
    end)
end

-- Get the file extension from a string
function getFileExtension(path)
    return path:match("^.+(%..+)$")
end

-- Get file paths to all files ending in the given file extension in a given dir
-- This will recurse through subdirs
function getFilesByExtension(extension, dir)
    local function yieldfile(dir)
        for filename, attr in dirtree(dir) do
            if attr.mode == "file" and getFileExtension(filename) == extension then
                coroutine.yield(filename)
            end
        end
    end
    return coroutine.wrap(function()
        yieldfile(dir)
    end)
end

-- Build test executable for a cpp file
local function test(filepath)

    local filename = filepath:match("([^/]+)$")
    local projectname = filename:match("^[^%.]+")
    -- print("Filepath: " .. filepath)
    -- print("Filename: " .. filename)
    -- print("Projectname: " .. projectname)

    project(projectname)
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{cfg.buildcfg}"
    
    buildoptions {
        "-Wall", 
        "-fno-exceptions", 
        "-fno-rtti"
    }

    includedirs {
        "./include",
        "../../rive/include"
    }

    files {
        "../../rive/src/**.cpp",
        filepath
    }

    filter "configurations:debug"
        defines { "DEBUG" }
        symbols "On"
end

-- Build all cpp test files in Rive's test directory
for cppFile in getFilesByExtension(".cpp", "../../rive/test/") do
    test(cppFile)
end

-- Build test executable for a cpp file and link to the precompiled rive lib
local function test_precompiled(filepath)

    local filename = filepath:match("([^/]+)$") .. "_linked"
    local projectname = filename:match("^[^%.]+") .. "_linked"
    -- print("Filepath: " .. filepath)
    -- print("Filename: " .. filename)
    -- print("Projectname: " .. projectname)

    project(projectname)
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{cfg.buildcfg}"
    
    buildoptions {
        "-Wall", 
        "-fno-exceptions", 
        "-fno-rtti"
    }

    includedirs {
        "./include",
        "../../rive/include"
    }

    files { filepath }

    links
    {
        "../../rive/build/bin/debug/librive.a"
    }

    filter "configurations:debug"
        defines { "DEBUG" }
        symbols "On"
end

-- Build all cpp test files in Rive's test directory
for cppFile in getFilesByExtension(".cpp", "../../rive/test/") do
    test_precompiled(cppFile)
end

--]]

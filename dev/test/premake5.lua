require "lfs"

--[[
    Utility functions
--]]

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

--[[
    Premake configuration
--]]

-- Clean Function --
newaction {
    trigger     = "clean",
    description = "clean the build",
    execute     = function ()
       print("clean the build...")
       os.rmdir("build")
       os.remove("Makefile")
       -- no wildcards in os.remove, so use shell
       os.execute("rm *.make")
       print("build cleaned")
    end
 }

workspace "rive_tests"
configurations {
    "debug"
}

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
    cppdialect "C++11"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{cfg.buildcfg}"

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
-- for cppFile in getFilesByExtension(".cpp", "../../rive/test/") do
--     test(cppFile)
-- end

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
    cppdialect "C++11"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{cfg.buildcfg}"

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
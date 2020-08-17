-- require "./premake-emscripten/emscripten"


-- local emcc_tools = {
--    cc = "emcc",
--    cxx = "emcc", -- cxx = "em++",
--    ar = "emar"
-- }

-- local clang = premake.tools.clang

-- premake.tools.emcc = {
--    gettoolname = function(cfg, tool)
--       return emcc_tools[tool]
--    end,
--    getdefines = clang.getdefines,
--    getundefines = clang.getundefines,
--    getincludedirs = clang.getincludedirs,
--    getforceincludes = clang.getforceincludes,
--    getlocalflags = clang.getlocalflags,
--    getcxxflags = function(cfg)
--       local flags = clang.getcxxflags(cfg)
--       print('--getcxxflags:')
--       table.foreachi(cfg.flags, function(value)
-- 			print('cfg value ' .. value)
--       end)
      
--       table.insertflat(flags, '-fno-rtti')
--       table.foreachi(flags, function(value)
-- 			print('cxx flag ' .. value)
--       end)
--       return flags
--    end,
--    getcppflags = clang.getcppflags,
--    getcflags = clang.getcflags,
--    getlinks = clang.getlinks,
--    getLibraryDirectories = clang.getLibraryDirectories,
--    getrunpathdirs = clang.getrunpathdirs,
--    getldflags = function(cfg)
--       local flags = clang.getldflags(cfg)
--       table.insertflat(flags, "-s EXPORT_NAME=\"RiveWasmTest\"")
--       return flags
--    end,
--    getmakesettings = clang.getmakesettings,
-- }



-- function emcc.gettoolname(cfg, tool)
--    return emcc_tools[tool];
-- end

-- premake.tools.emcc.gettoolname = gettoolname

-- function premake.tools.emcc.gettoolname(cfg, tool)
--    return emcc_tools[tool]
-- end


workspace "rive"
   configurations { "debug", "release" }

project "rive"
   kind "StaticLib"
   language "C++"
   cppdialect "C++17"
   targetdir "bin/%{cfg.buildcfg}"
   objdir "obj/%{cfg.buildcfg}"
   includedirs { "../include" }

   files { "../src/**.cpp" }

   filter "configurations:debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:release"
      defines { "RELEASE" }
      optimize "On"

-- project "rive-wasm-runtimme"
--    rules { "MyWasmRule" }
--    kind "Utility"

-- rule "MyWasmRule"
--    display "My custom wasm compiler"
--    fileextension ".cpp"

--    buildmessage "Compiling to WASM"
--    buildcommands 'emcc -O3 --bind -o wasm_test.js -s FORCE_FILESYSTEM=0 -s MODULARIZE=1 -s NO_EXIT_RUNTIME=1 -s STRICT=1 -s WARN_UNALIGNED=1 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=1 -s WASM=1 -s EXPORT_NAME="WasmTest" -DSINGLE -DANSI_DECLARATORS -DTRILIBRARY -Wno-c++17-extensions -I../include --no-entry ../src/*/*.cpp'
--    buildcommands 'echo YOOOOOOOOOOOOOOO'


--   buildmessage 'Compiling %(Filename) with MyCustomCC'
--   buildcommands 'MyCustomCC.exe -c "%(FullPath)" -o "%(IntDir)/%(Filename).obj"'
--   buildoutputs '%(IntDir)/%(Filename).obj"'


-- project "rive-by-rules"
--    -- rules { "MyWasmRule" }
--    -- kind "None"
--    filter 'files:**.lua'
--    -- A message to display while this build step is running (optional)
--    buildmessage 'Compiling %{file.relpath}'

--    -- One or more commands to run (required)
--    buildcommands {
--       'luac -o "%{cfg.objdir}/%{file.basename}.out" "%{file.relpath}"'
--    }

--    -- One or more outputs resulting from the build (required)
--    buildoutputs { '%{cfg.objdir}/%{file.basename}.c' }

--    -- One or more additional dependencies for this build command (optional)
--    buildinputs { 'path/to/file1.ext', 'path/to/file2.ext' }


-- project "rive.js"
--    kind "None"
--    toolset "emcc"
--    language "C++"
--    cppdialect "C++17"

--    buildoptions {
--       "--bind",
--       "-s FORCE_FILESYSTEM=0",
--       "-s MODULARIZE=1",
--       "-s NO_EXIT_RUNTIME=1",
--       "-s STRICT=1",
--       "-s WARN_UNALIGNED=1",
--       "-s ALLOW_MEMORY_GROWTH=1",
--       "-s DISABLE_EXCEPTION_CATCHING=1",
--       "-s WASM=1",
--       "-s EXPORT_NAME=\"RiveWasmTest\"",
--       "-DSINGLE",
--       "-DANSI_DECLARATORS",
--       "-DTRILIBRARY",
--       "-Wno-c++17-extensions",
--       "--no-entry"
--    }

--    targetdir "bin/%{cfg.buildcfg}"
--    objdir "obj/%{cfg.buildcfg}"
--    includedirs { "../include" }

--    files { "../src/**.cpp" }

--    postbuildcommands { "echo Running postbuild" }

--    filter "configurations:debug"
--       defines { "DEBUG" }
--       symbols "On"

--    filter "configurations:release"
--       defines { "RELEASE" }
--       optimize "On"
   

-- Clean Function --
newaction {
   trigger     = "clean",
   description = "clean the build",
   execute     = function ()
      print("clean the build...")
      os.rmdir("./bin")
      os.rmdir("./obj")
      os.remove("Makefile")
      -- no wildcards in os.remove, so use shell
      os.execute("rm *.make")
      print("build cleaned")
   end
}
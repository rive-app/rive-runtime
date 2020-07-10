workspace "rive"
   configurations { "debug", "release" }

project "rive"
   kind "StaticLib"
   language "C++"
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
local dependency = require("dependency")
luau = dependency.github("luigi-rosso/luau", "rive_0_2")

require("export-compile-commands")
dofile("rive_build_config.lua")

project("luau_vm")
do
	kind("StaticLib")
	exceptionhandling("On")

	includedirs({
		luau .. "/VM/include",
		luau .. "/Common/include",
	})

	files({ luau .. "/VM/src/**.cpp" })
	optimize("Size")
end

project("luau_compiler")
do
	kind("StaticLib")
	exceptionhandling("On")

	includedirs({
		luau .. "/Compiler/include",
		luau .. "/Ast/include",
		luau .. "/Common/include",
	})

	files({ luau .. "/Compiler/src/**.cpp", luau .. "/Ast/src/**.cpp" })
	optimize("Size")
end

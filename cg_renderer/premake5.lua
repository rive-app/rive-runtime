dofile('rive_build_config.lua')

dependencies = os.getenv('DEPENDENCIES')

project('rive_cg_renderer')
do
    kind('StaticLib')
    includedirs({ 'include', '../include' })

    libdirs({ '../../build/%{cfg.system}/bin/' .. RIVE_BUILD_CONFIG })

    files({ 'src/**.cpp' })

    flags({ 'FatalCompileWarnings' })

    filter('system:windows')
    do
        architecture('x64')
        defines({ '_USE_MATH_DEFINES' })
    end

    filter({ 'system:macosx', 'options:variant=runtime' })
    do
        buildoptions({ '-fembed-bitcode -arch arm64 -arch x86_64' })
    end

    if os.host() == 'macosx' then
        iphoneos_sysroot = os.outputof('xcrun --sdk iphoneos --show-sdk-path')
        iphonesimulator_sysroot = os.outputof('xcrun --sdk iphonesimulator --show-sdk-path')

        filter({ 'system:ios', 'options:variant=system' })
        do
            buildoptions({
                '-mios-version-min=13.0 -fembed-bitcode -arch arm64 -isysroot ' .. iphoneos_sysroot,
            })
        end

        filter({ 'system:ios', 'options:variant=emulator' })
        do
            buildoptions({
                '--target=arm64-apple-ios13.0.0-simulator -mios-version-min=13.0 -arch x86_64 -arch arm64 -isysroot '
                    .. iphonesimulator_sysroot,
            })
        end
    end
end

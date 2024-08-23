local dependency = require('dependency')
yoga = dependency.github('rive-app/yoga', 'rive_changes_v2_0_1')

workspace('rive')
configurations({ 'debug', 'release' })

project('rive_yoga')
do
    kind('StaticLib')
    language('C++')
    cppdialect('C++11')
    targetdir('%{cfg.system}/cache/bin/%{cfg.buildcfg}/')
    objdir('%{cfg.system}/cache/obj/%{cfg.buildcfg}/')
    warnings('Off')

    defines({ 'YOGA_EXPORT=' })

    includedirs({ yoga })

    files({
        yoga .. '/yoga/Utils.cpp',
        yoga .. '/yoga/YGConfig.cpp',
        yoga .. '/yoga/YGLayout.cpp',
        yoga .. '/yoga/YGEnums.cpp',
        yoga .. '/yoga/YGNodePrint.cpp',
        yoga .. '/yoga/YGNode.cpp',
        yoga .. '/yoga/YGValue.cpp',
        yoga .. '/yoga/YGStyle.cpp',
        yoga .. '/yoga/Yoga.cpp',
        yoga .. '/yoga/event/event.cpp',
        yoga .. '/yoga/log.cpp',
    })

    buildoptions({ '-Wall', '-pedantic' })

    linkoptions({ '-r' })

    filter('system:emscripten')
    do
        buildoptions({ '-pthread' })
    end

    filter('configurations:debug')
    do
        defines({ 'DEBUG' })
        symbols('On')
    end

    filter('toolset:clang')
    do
        flags({ 'FatalWarnings' })
        buildoptions({
            '-Werror=format',
            '-Wimplicit-int-conversion',
            '-Werror=vla',
        })
    end

    filter('configurations:release')
    do
        buildoptions({ '-Oz' })
        defines({ 'RELEASE', 'NDEBUG' })
        optimize('On')
    end

    filter({ 'system:macosx', 'options:variant=runtime' })
    do
        buildoptions({
            '-Wimplicit-float-conversion -fembed-bitcode -arch arm64 -arch x86_64 -isysroot'
                .. (os.getenv('MACOS_SYSROOT') or ''),
        })
    end

    filter({ 'system:macosx', 'configurations:release' })
    do
        buildoptions({ '-flto=full' })
    end

    filter({ 'system:ios' })
    do
        buildoptions({ '-flto=full' })
    end

    filter('system:windows')
    do
        architecture('x64')
        defines({ '_USE_MATH_DEFINES' })
    end

    filter({ 'system:ios', 'options:variant=system' })
    do
        buildoptions({
            '-mios-version-min=13.0 -fembed-bitcode -arch arm64 -isysroot '
                .. (os.getenv('IOS_SYSROOT') or ''),
        })
    end

    filter({ 'system:ios', 'options:variant=emulator' })
    do
        buildoptions({
            '--target=arm64-apple-ios13.0.0-simulator',
            '-mios-version-min=13.0 -arch arm64 -arch x86_64 -isysroot '
                .. (os.getenv('IOS_SYSROOT') or ''),
        })
        targetdir('%{cfg.system}_sim/cache/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}_sim/cache/obj/%{cfg.buildcfg}')
    end

    filter({ 'system:android', 'configurations:release' })
    do
        buildoptions({ '-flto=full' })
    end

    -- Is there a way to pass 'arch' as a variable here?
    filter({ 'system:android', 'options:arch=x86' })
    do
        targetdir('%{cfg.system}/cache/x86/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}/cache/x86/obj/%{cfg.buildcfg}')
    end

    filter({ 'system:android', 'options:arch=x64' })
    do
        targetdir('%{cfg.system}/cache/x64/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}/cache/x64/obj/%{cfg.buildcfg}')
    end

    filter({ 'system:android', 'options:arch=arm' })
    do
        targetdir('%{cfg.system}/cache/arm/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}/cache/arm/obj/%{cfg.buildcfg}')
    end

    filter({ 'system:android', 'options:arch=arm64' })
    do
        targetdir('%{cfg.system}/cache/arm64/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}/cache/arm64/obj/%{cfg.buildcfg}')
    end
end

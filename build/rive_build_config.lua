if _ACTION == 'ninja' then
    require('ninja')
end

if _ACTION == 'export-compile-commands' then
    require('export-compile-commands')
end

workspace('rive')

newoption({
    trigger = 'config',
    description = 'one-and-only config (for multiple configs, target a new --out directory)',
    allowed = { { 'debug' }, { 'release' }, { nil } },
    default = nil,
})
newoption({ trigger = 'release', description = 'shortcut for \'--config=release\'' })
if _OPTIONS['release'] then
    if _OPTIONS['config'] then
        error('use either \'--release\' or \'--config=release/debug\' (not both)')
    end
    _OPTIONS['config'] = 'release'
    _OPTIONS['release'] = nil
elseif not _OPTIONS['config'] then
    _OPTIONS['config'] = 'debug'
end
RIVE_BUILD_CONFIG = _OPTIONS['config']

newoption({
    trigger = 'out',
    description = 'Directory to generate build files',
    default = nil,
})
RIVE_BUILD_OUT = _WORKING_DIR .. '/' .. (_OPTIONS['out'] or ('out/' .. RIVE_BUILD_CONFIG))

newoption({
    trigger = 'toolset',
    value = 'type',
    description = 'Choose which toolchain to build with',
    allowed = {
        { 'clang', 'Build with Clang' },
        { 'msc', 'Build with the Microsoft C/C++ compiler' },
    },
    default = 'clang',
})

newoption({
    trigger = 'arch',
    value = 'ABI',
    description = 'The ABI with the right toolchain for this build, generally with Android',
    allowed = {
        { 'host' },
        { 'x86' },
        { 'x64' },
        { 'arm' },
        { 'arm64' },
        { 'universal', '"fat" library on apple platforms' },
        { 'wasm', 'emscripten targeting web assembly' },
        { 'js', 'emscripten targeting javascript' },
    },
    default = 'host',
})

-- Premake stopped supporting "--os=android". Add our own flag for android.
newoption({
    trigger = 'for_android',
    description = 'compile for android (supersedes --os)',
})

newoption({
    trigger = 'android_api',
    description = 'Target Android API version number',
    default = '21',
})

newoption({
    trigger = 'variant',
    value = 'type',
    description = 'Choose a particular variant to build',
    allowed = {
        { 'system', 'Builds the static library for the provided system' },
        { 'emulator', 'Builds for a simulator for the provided system' },
        { 'xros', 'Builds for Apple Vision Pro' },
        { 'xrsimulator', 'Builds for Apple Vision Pro simulator' },
        { 'appletvos', 'Builds for Apple TV' },
        { 'appletvsimulator', 'Builds for Apple TV simulator' },
        { 'maccatalyst', 'Builds for Mac Catalyst' },
        {
            'runtime',
            'Build the static library specifically targeting our runtimes',
        },
    },
    default = 'system',
})

newoption({
    trigger = 'with-rtti',
    description = 'don\'t disable rtti (nonstandard for Rive)',
})

newoption({
    trigger = 'with-pic',
    description = 'enable position independent code',
})

newoption({
    trigger = 'with-exceptions',
    description = 'don\'t disable exceptions (nonstandard for Rive)',
})

newoption({
    trigger = 'no-wasm-simd',
    description = 'disable simd for wasm builds',
})

newoption({
    trigger = 'wasm_single',
    description = 'Embed wasm directly into the js, instead of side-loading it.',
})

newoption({
    trigger = 'no-lto',
    description = 'Don\'t build with link time optimizations.',
})

newoption({
    trigger = 'for_unreal',
    description = 'compile for unreal engine',
})

newoption({
    trigger = 'with-asan',
    description = 'enable AddressSanitizer',
})

newoption({
    trigger = 'with_ubsan',
    description = 'enable UndefinedBehaviorSanitizer',
})

newoption({ trigger = 'with_optick', description = 'use optick profiler' })
if _OPTIONS['with_optick'] then
    defines({ 'RIVE_OPTICK' })
    RIVE_OPTICK_URL = 'bombomby/optick'
    RIVE_OPTICK_VERSION = '1.4.0.0'
end

newoption({
    trigger = 'with_rive_path_query',
    description = 'Deferred render paths retain a queryable RawPath twin on '
        .. 'request, for hosts that hit test and measure recorded paths.',
})
if _OPTIONS['with_rive_path_query'] then
    defines({ 'WITH_RIVE_PATH_QUERY' })
end

newoption({ trigger = 'with_microprofile', description = 'use microprofile profiler' })
if _OPTIONS['with_microprofile'] then
    defines({ 'RIVE_MICROPROFILE' })
    RIVE_MICROPROFILE_URL = 'aliasbinman/microprofile'
    RIVE_MICROPROFILE_VERSION = 'rivebuild'
end

newoption({
    trigger = 'prof_level_max',
    value = '0..4',
    description = 'Compile-time max tier for RIVE_PROF_SCOPE_L / GPUNAME_L / SCOPENAME_L. Markers with level > N drop at preprocess time. 0 = frame markers only (cheapest), 4 = everything. Default 0.',
})
if _OPTIONS['prof_level_max'] then
    defines({ 'RIVE_PROF_LEVEL_MAX=' .. _OPTIONS['prof_level_max'] })
end

location(RIVE_BUILD_OUT)
targetdir(RIVE_BUILD_OUT)
objdir(RIVE_BUILD_OUT .. '/obj')
toolset(_OPTIONS['toolset'] or 'clang')
language('C++')
cppdialect('C++17')
configurations({ 'default' })
filter({ 'options:not with-rtti' })
rtti('Off')
filter({ 'options:with-rtti' })
rtti('On')
filter({ 'options:not with-exceptions' })
exceptionhandling('Off')
filter({ 'options:with-exceptions' })
exceptionhandling('On')

filter({ 'options:with-asan' })
do
    sanitize({ 'Address' })

    -- Edit & continue and incremental link are not compatible with ASAN in MSVC.
    editandcontinue('Off')
    flags({ 'NoIncrementalLink' })
end

filter({ 'options:with_ubsan', 'system:not windows' })
do
    buildoptions({
        '-fsanitize=undefined',
        '-fno-sanitize-recover=all'
    })
    linkoptions({
        '-fsanitize=undefined',
        '-fno-sanitize-recover=all'
    })
end

filter({ 'options:with-pic' })
do
    pic('on')
    buildoptions({ '-fPIC' })
    linkoptions({ '-fPIC' })
end

filter('options:config=debug')
do
    defines({ 'DEBUG' })
    symbols('On')
end

filter('options:config=release')
do
    defines({ 'RELEASE' })
    defines({ 'NDEBUG' })
    optimize('On')
end

-- Generate PDBs in Release builds on Windows for Sentry crash symbolication.
filter({ 'options:config=release', 'system:windows' })
do
    symbols('On')
end

filter({ 'options:config=release', 'options:not no-lto', 'system:not macosx', 'system:not ios' })
do
    if linktimeoptimization then
        linktimeoptimization('On')
    else
        -- Deprecated way of turning on LTO, for older versions of premake.
        flags({ 'LinkTimeOptimization' })
    end
end

filter({ 'options:config=release', 'options:not no-lto', 'system:macosx or ios' })
do
    -- The 'linktimeoptimization' command attempts to use llvm-ar, which doesn't always exist on macos.
    buildoptions({ '-flto=full' })
    linkoptions({ '-flto=full' })
end

newoption({
    trigger = 'windows_runtime',
    description = 'Choose whether to use staticruntime on/off/default',
    allowed = {
        { 'default', 'Use default runtime' },
        { 'static', 'Use static runtime' },
        { 'dynamic', 'Use dynamic runtime' },
        { 'dynamic_debug', 'Use dynamic runtime force debug' },
        { 'dynamic_release', 'Use dynamic runtime force release' },
    },
    default = 'default',
})

newoption({
    trigger = 'toolsversion',
    value = 'msvc_toolsversion',
    description = 'specify the version of the compiler tool. On windows thats the msvc version which affects both clang and msvc outputs.',
    default = 'latest',
})

-- This is just to match our old windows config. Rive Native specifically sets
-- static/dynamic and maybe we should do the same elsewhere.
filter({ 'system:windows', 'options:windows_runtime=default', 'options:not for_unreal' })
do
    staticruntime('on') -- Match Skia's /MT flag for link compatibility
    runtime('Release')
end

filter({ 'system:windows', 'options:windows_runtime=static', 'options:not for_unreal' })
do
    staticruntime('on') -- Match Skia's /MT flag for link compatibility
end

filter({ 'system:windows', 'options:windows_runtime=dynamic', 'options:not for_unreal' })
do
    staticruntime('off')
end

filter({
    'system:windows',
    'options:not windows_runtime=default',
    'options:config=debug',
    'options:not for_unreal',
})
do
    runtime('Debug')
end

filter({
    'system:windows',
    'options:not windows_runtime=default',
    'options:config=release',
    'options:not for_unreal',
})
do
    runtime('Release')
end

filter({ 'system:windows', 'options:windows_runtime=dynamic_debug', 'options:not for_unreal' })
do
    staticruntime('off')
    runtime('Debug')
end

filter({ 'system:windows', 'options:windows_runtime=dynamic_release', 'options:not for_unreal' })
do
    staticruntime('off')
    runtime('Release')
end

filter('system:windows')
do
    architecture('x64')
    defines({ '_USE_MATH_DEFINES', 'NOMINMAX' })
end

filter({ 'system:windows', 'options:for_unreal' })
do
    staticruntime('off')
    runtime('Release')
end

-- Opt in to Incredibuild-friendly MSVC settings by setting
-- $RIVE_USE_INCREDIBUILD. (See also the Incredibuild handling in
-- build_rive.sh.)
if os.getenv('RIVE_USE_INCREDIBUILD') then
    -- Debug only: release enables LTO (/LTCG) above, which is incompatible with
    -- /INCREMENTAL -- the linker warns (LNK4075) and drops one of them.
    -- Incremental linking is a debug-iteration feature anyway, so scope it (and
    -- the rest of these tweaks) to debug, where LTO is already disabled.
    filter({ 'system:windows', 'options:not for_unreal', 'options:config=debug' })
    do
        if type(incrementallink) == 'function' then
            incrementallink('On')
        else
            -- Older Premake automatically passes /INCREMENTAL to Visual Studio Debug configs.
        end
        if linktimeoptimization then
            linktimeoptimization('Off') -- Disables /LTCG.
        else
            removeflags({ 'LinkTimeOptimization' })
        end
        symbols('On') -- Turns on MSVC debug symbol generation.
        editandcontinue('Off') -- Disables /ZI (Edit & Continue) and forces /Zi (Program Database).
        -- Embeds the debug symbols directly into each individual .obj file
        -- instead of a single shared PDB (/Z7), which can create contention on
        -- a massively parallel build. (The final monolithic PDB is then
        -- generated normally during the link step).
        debugformat "c7"
    end
end

-- Unreal requires c++20 under most circumstances. However, some platforms require 17. So make it a seperate flag
newoption({ trigger = 'cpp20', description = 'use c++ 20 standard' })
filter({'options:cpp20'})
do
    cppdialect('C++20')
end

-- Unreal "prefers" certain msvc versions so we try to match it from the python build script by using "toolsversion" option
-- this defaults to "latest" which will just do the default behavior anyway. So no need for a check
toolsversion(_OPTIONS['toolsversion'])

filter({ 'system:windows', 'options:toolset=clang' })
do
    buildoptions({
        '-Wno-c++98-compat',
        '-Wno-c++20-compat',
        '-Wno-c++98-compat-pedantic',
        '-Wno-c99-extensions',
        '-Wno-ctad-maybe-unsupported',
        '-Wno-deprecated-copy-with-user-provided-dtor',
        '-Wno-deprecated-declarations',
        '-Wno-documentation',
        '-Wno-documentation-pedantic',
        '-Wno-documentation-unknown-command',
        '-Wno-double-promotion',
        '-Wno-exit-time-destructors',
        '-Wno-float-equal',
        '-Wno-global-constructors',
        '-Wno-implicit-float-conversion',
        '-Wno-newline-eof',
        '-Wno-old-style-cast',
        '-Wno-reserved-identifier',
        '-Wno-shadow',
        '-Wno-sign-compare',
        '-Wno-sign-conversion',
        '-Wno-unused-macros',
        '-Wno-unused-parameter',
        '-Wno-four-char-constants',
        '-Wno-unreachable-code',
        '-Wno-switch-enum',
        '-Wno-missing-field-initializers',
        '-Wno-unsafe-buffer-usage',
    })
end

filter({ 'system:windows', 'options:toolset=msc' })
do
    -- MSVC doesn't accept designated initializers in C++17.
    cppdialect('c++latest')
    defines({
        '_CRT_SECURE_NO_WARNINGS',
        '_SILENCE_CXX20_IS_POD_DEPRECATION_WARNING',
        '_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS',
    })

    -- We currently suppress several warnings for the MSVC build, some serious. Once this build
    -- is fully integrated into GitHub actions, we will definitely want to address these.
    disablewarnings({
        '4061', -- enumerator 'identifier' in switch of enum 'enumeration' is not explicitly
        -- handled by a case label
        '4100', -- 'identifier': unreferenced formal parameter
        '4201', -- nonstandard extension used: nameless struct/union
        '4244', -- 'conversion_type': conversion from 'type1' to 'type2', possible loss of data
        '4245', -- 'conversion_type': conversion from 'type1' to 'type2', signed/unsigned
        --  mismatch
        '4267', -- 'variable': conversion from 'size_t' to 'type', possible loss of data
        '4355', -- 'this': used in base member initializer list
        '4365', -- 'expression': conversion from 'type1' to 'type2', signed/unsigned mismatch
        '4388', -- 'expression': signed/unsigned mismatch
        '4389', -- 'operator': signed/unsigned mismatch
        '4458', -- declaration of 'identifier' hides class member
        '4514', -- 'function': unreferenced inline function has been removed
        '4583', -- 'Catch::clara::detail::ResultValueBase<T>::m_value': destructor is not
        -- implicitly called
        '4623', -- 'Catch::AssertionInfo': default constructor was implicitly defined as deleted
        '4625', -- 'derived class': copy constructor was implicitly defined as deleted because a
        -- base class copy constructor is inaccessible or deleted
        '4626', -- 'derived class': assignment operator was implicitly defined as deleted
        --  because a base class assignment operator is inaccessible or deleted
        '4820', -- 'bytes' bytes padding added after construct 'member_name'
        '4868', -- (catch.hpp) compiler may not enforce left-to-right evaluation order in braced
        -- initializer list
        '5026', -- 'type': move constructor was implicitly defined as deleted
        '5027', -- 'type': move assignment operator was implicitly defined as deleted
        '5039', -- (catch.hpp) 'AddVectoredExceptionHandler': pointer or reference to
        -- potentially throwing function passed to 'extern "C"' function under -EHc.
        -- Undefined behavior may occur if this function throws an exception.
        '5045', -- Compiler will insert Spectre mitigation for memory load if /Qspectre switch
        -- specified
        '5204', -- 'Catch::Matchers::Impl::MatcherMethod<T>': class has virtual functions, but
        -- its trivial destructor is not virtual; instances of objects derived from this
        -- class may not be destructed correctly
        '5219', -- implicit conversion from 'type-1' to 'type-2', possible loss of data
        '5262', -- MSVC\14.34.31933\include\atomic(917,9): implicit fall-through occurs here;
        -- are you missing a break statement?
        '5264', -- 'rive::math::PI': 'const' variable is not used
        '4647', -- behavior change: __is_pod(rive::Vec2D) has different value in previous versions
    })
end

filter({ 'action:vs2022' })
do
    flags({ 'MultiProcessorCompile' })
end

filter({})

-- os.target() does not seem to be getting updated when system( ) is changed (for instance, when
-- building on Windows, it stays 'windows' even after we do system('android') which means if we
-- use it directly, we will include more things than are strictly necessary.
--
-- Instead, make an alias that we can use instead that we set manually based on calls to system()
-- Additionally, premake deprecated `--os=android` which is why the custom system() call is 
-- needed in the first place (otherwise the build script would just be passing it in)
rive_target_os = os.target()

-- Native Windows targets only. Android and wasm happen to build through ninja
-- on a Windows host too, but they retarget to the android_ndk/emsdk toolsets
-- below and would choke on these flags.
if os.host() == 'windows'
    and _ACTION == 'ninja'
    and not _OPTIONS['for_android']
    and _OPTIONS['arch'] ~= 'wasm'
    and _OPTIONS['arch'] ~= 'js'
then
    if _OPTIONS['toolset'] ~= 'clang' then
        error('Unsupported toolset ' .. _OPTIONS['toolset'] .. '. Only clang is supported on windows/ninja')
    end
    -- The vstudio actions compile through clang-cl, but ninja invokes the clang
    -- toolset's binaries directly, and premake's clang toolset names the GNU
    -- archiver 'ar', which doesn't exist on Windows. Rename just that binary to
    -- the llvm-ar that ships with Visual Studio.
    --
    -- Patch gettoolname in place rather than cloning into a new toolset the way
    -- android_ndk and emsdk do below: those target a different system, but this
    -- is still the clang toolset, and renaming it would stop every
    -- 'toolset:clang' filter in the tree from matching (libwebp's -msse4.1
    -- flags, for two).
    local win_clang_tools = { ar = 'llvm-ar' }
    local base_gettoolname = premake.tools.clang.gettoolname
    function premake.tools.clang.gettoolname(cfg, tool)
        return win_clang_tools[tool] or base_gettoolname(cfg, tool)
    end

    local WINDOWS_TARGET_ARCHS = {
        host = 'x86_64', -- the system:windows filter above pins architecture('x64') regardless
        x64 = 'x86_64',
        x86 = 'i686',
        arm64 = 'aarch64',
        arm = 'armv7',
    }
    local target_arch = WINDOWS_TARGET_ARCHS[_OPTIONS['arch']]
    if not target_arch then
        error('Unsupported arch ' .. _OPTIONS['arch'] .. ' for windows/ninja')
    end
    local target = '--target=' .. target_arch .. '-pc-windows-msvc'
    buildoptions({ target })

    -- clang++ links through MSVC's link.exe by default, which can't read the
    -- GNU thin archives some of our prebuilt dependencies ship (Dawn's
    -- webgpu_dawn.lib -> LNK1107). lld-link reads them, and is already what the
    -- vstudio ClangCL builds link with.
    linkoptions({ target, '-fuse-ld=lld' })
end

-- Don't use filter() here because we don't want to generate the "android_ndk" toolset if not
-- building for android.
if _OPTIONS['for_android'] then
    system('android')
    rive_target_os = 'android'

    pic('on') -- Position-independent code is required for NDK libraries.

    -- Detect the NDK.
    local EXPECTED_NDK_VERSION = 'r27c'
    local NDK_LONG_VERSION_STRING = "27.2.12479018"
    ndk = os.getenv('NDK_PATH') or os.getenv('ANDROID_NDK') or '<undefined>'
    local ndk_version = '<undetected>'
    local ndk_long_version = '<undetected>'
    local f = io.open(ndk .. '/source.properties', 'r')
    if f then
        for line in f:lines() do
            local match = line:match('^Pkg.ReleaseName = (.+)$')
            if match then
                ndk_version = match
                break
            end
            match = line:match('^Pkg.Revision = (.+)$')
            if match then
                ndk_long_version = match
                break
            end
        end
        f:close()
    end
    if ndk_version ~= EXPECTED_NDK_VERSION and ndk_long_version ~= NDK_LONG_VERSION_STRING then
        print()
        print('** Rive requires Android NDK version ' .. EXPECTED_NDK_VERSION .. ' **')
        print()
        print('To install via Android Studio:')
        print('  - Settings > SDK Manager > SDK Tools')
        print('  - Check "Show Package Details" at the bottom')
        print('  - Select '..NDK_LONG_VERSION_STRING..' under "NDK (Side by side)"')
        print('  - Note the value of "Android SDK Location"')
        print()
        print('Then set the ANDROID_NDK environment variable:')
        print('  - export ANDROID_NDK="<Android SDK Location>/ndk/'..NDK_LONG_VERSION_STRING..'"')
        print()
        error(
            'Unsupported Android NDK\n  ndk: '
                .. ndk
                .. '\n  version: '
                .. ndk_version
                .. '\n  expected: '
                .. EXPECTED_NDK_VERSION
        )
    end

    local ndk_toolchain = ndk .. '/toolchains/llvm/prebuilt'
    if os.host() == 'windows' then
        ndk_toolchain = ndk_toolchain .. '/windows-x86_64'
    elseif os.host() == 'macosx' then
        ndk_toolchain = ndk_toolchain .. '/darwin-x86_64'
    else
        ndk_toolchain = ndk_toolchain .. '/linux-x86_64'
    end

    local android_base_targets = {
        arm64 = 'aarch64-linux-android',
        arm = 'armv7a-linux-androideabi',
        x64 = 'x86_64-linux-android',
        x86 = 'i686-linux-android',
    }
    local android_target = android_base_targets[_OPTIONS['arch']] .. _OPTIONS['android_api']

    -- clone the clang toolset into a custom one called "android_ndk".
    premake.tools.android_ndk = {}
    for k, v in pairs(premake.tools.clang) do
        premake.tools.android_ndk[k] = v
    end

    -- update the android_ndk toolset to use the appropriate binaries.
    local android_ndk_tools = { ar = ndk_toolchain .. '/bin/llvm-ar' }
    if os.host() == 'windows' then
        -- Invoke clang directly instead of going through the .cmd wrappers,
        -- which only add '--target=' and add a process layer between the build
        -- system and the compiler.
        android_ndk_tools.cc = ndk_toolchain .. '/bin/clang.exe --target=' .. android_target
        android_ndk_tools.cxx = ndk_toolchain .. '/bin/clang++.exe --target=' .. android_target
    else
        android_ndk_tools.cc = ndk_toolchain .. '/bin/' .. android_target .. '-clang'
        android_ndk_tools.cxx = ndk_toolchain .. '/bin/' .. android_target .. '-clang++'
    end
    function premake.tools.android_ndk.gettoolname(cfg, tool)
        return android_ndk_tools[tool]
    end

    -- suppress "** Warning: Unsupported toolset 'android_ndk'".
    local premake_valid_tools = premake.action._list[_ACTION].valid_tools
    if premake_valid_tools ~= nil then
        premake_valid_tools['cc'][#premake_valid_tools['cc'] + 1] = 'android_ndk'
    end

    toolset('android_ndk')

    buildoptions({
        '--sysroot=' .. ndk_toolchain .. '/sysroot',
        '-fdata-sections',
        '-ffunction-sections',
        '-funwind-tables',
        '-fstack-protector-strong',
        '-no-canonical-prefixes',
    })

    linkoptions({
        '--sysroot=' .. ndk_toolchain .. '/sysroot',
        '-fdata-sections',
        '-ffunction-sections',
        '-funwind-tables',
        '-fstack-protector-strong',
        '-no-canonical-prefixes',
        '-Wl,--fatal-warnings',
        '-Wl,--gc-sections',
        '-Wl,--no-rosegment',
        '-Wl,--no-undefined',
        '-static-libstdc++',
    })

    filter({})
end

filter('system:linux', 'options:arch=x64')
do
    architecture('x64')
end

filter('system:linux', 'options:arch=arm')
do
    architecture('arm')
end

filter('system:linux', 'options:arch=arm64')
do
    architecture('arm64')
end

filter({})

filter('system:macosx')
do
    defines({ 'RIVE_MACOSX' })
end

filter('system:windows')
do
    defines({ 'RIVE_WINDOWS' })
end

filter({})

if _OPTIONS['os'] == 'ios' then
    if _OPTIONS['variant'] == 'system' then
        iphoneos_sysroot = os.outputof('xcrun --sdk iphoneos --show-sdk-path')
        if iphoneos_sysroot == nil then
            error(
                'Unable to locate iPhoneOS SDK. Please ensure Xcode and its iOS component are installed. Additionally, ensure Xcode Command Line Tools are pointing to that Xcode location with `xcode-select -p`.'
            )
        end

        defines({ 'RIVE_IOS' })
        buildoptions({
            '--target=arm64-apple-ios13.0.0',
            '-mios-version-min=13.0.0',
            '-isysroot ' .. iphoneos_sysroot,
        })
    elseif _OPTIONS['variant'] == 'emulator' then
        iphonesimulator_sysroot = os.outputof('xcrun --sdk iphonesimulator --show-sdk-path')
        if iphonesimulator_sysroot == nil then
            error(
                'Unable to locate iPhone simulator SDK. Please ensure Xcode and its iOS component are installed.  Additionally, ensure Xcode Command Line Tools are pointing to that Xcode location with `xcode-select -p`.'
            )
        end

        defines({ 'RIVE_IOS_SIMULATOR' })
        buildoptions({
            '--target=arm64-apple-ios13.0.0-simulator',
            '-mios-version-min=13.0.0',
            '-isysroot ' .. iphonesimulator_sysroot,
        })
    elseif _OPTIONS['variant'] == 'xros' then
        xros_sysroot = os.outputof('xcrun --sdk xros --show-sdk-path')
        if xros_sysroot == nil then
            error(
                'Unable to locate xros sdk. Please ensure Xcode Command Line Tools are installed, as well as the visionOS SDK.'
            )
        end

        defines({ 'RIVE_XROS' })
        buildoptions({
            '--target=arm64-apple-xros1.0',
            '-isysroot ' .. xros_sysroot,
        })
    elseif _OPTIONS['variant'] == 'xrsimulator' then
        xrsimulator_sysroot = os.outputof('xcrun --sdk xrsimulator --show-sdk-path')
        if xrsimulator_sysroot == nil then
            error(
                'Unable to locate xrsimulator sdk. Please ensure Xcode Command Line Tools are installed, as well as the visionOS SDK.'
            )
        end

        defines({ 'RIVE_XROS_SIMULATOR' })
        buildoptions({
            '--target=arm64-apple-xros1.0-simulator',
            '-isysroot ' .. xrsimulator_sysroot,
        })
    elseif _OPTIONS['variant'] == 'appletvos' then
        appletvos_sysroot = os.outputof('xcrun --sdk appletvos --show-sdk-path')
        if appletvos_sysroot == nil then
            error(
                'Unable to locate appletvos sdk. Please ensure Xcode Command Line Tools are installed, as well as the tvOS SDK.'
            )
        end

        defines({ 'RIVE_APPLETVOS' })
        buildoptions({
            '--target=arm64-apple-tvos',
            '-mappletvos-version-min=16.0',
            '-isysroot ' .. appletvos_sysroot,
        })
    elseif _OPTIONS['variant'] == 'appletvsimulator' then
        appletvsimulator_sysroot = os.outputof('xcrun --sdk appletvsimulator --show-sdk-path')
        if appletvsimulator_sysroot == nil then
            error(
                'Unable to locate appletvsimulator sdk. Please ensure Xcode Command Line Tools are installed, as well as the tvOS SDK.'
            )
        end

        defines({ 'RIVE_APPLETVOS_SIMULATOR' })
        buildoptions({
            '--target=arm64-apple-tvos-simulator',
            '-mappletvsimulator-version-min=16.0',
            '-isysroot ' .. appletvsimulator_sysroot,
        })
    end

    filter({})
end

if os.host() == 'macosx' then
    filter('system:ios')
    do
        buildoptions({ '-fembed-bitcode ' })
    end

    filter('system:macosx or system:ios')
    do
        buildoptions({ '-fobjc-arc' })
    end

    filter({ 'system:macosx', 'options:not variant=maccatalyst' })
    do
        buildoptions({
            '-mmacosx-version-min=11.0',
        })
        -- Pass the deployment target at LINK time too. Without this, the linker
        -- stamps LC_BUILD_VERSION.minos from the build host's SDK, so a dylib
        -- built against a newer SDK can advertise a higher min OS version and
        -- fail to load on older macOS versions.
        --
        -- Note: buildoptions affects compilation only (e.g., availability macros)
        -- and does not influence the link step that stamps the final dylib.
        linkoptions({
            '-mmacosx-version-min=11.0',
        })
    end

    filter({ 'system:macosx', 'options:arch=host', 'action:xcode4' })
    do
        -- xcode tries to specify a target...
        buildoptions({
            '--target=' .. os.outputof('uname -m') .. '-apple-macos11.0',
        })
    end

    filter({ 'system:macosx', 'options:arch=arm64 or arch=universal' })
    do
        buildoptions({ '-arch arm64' })
        linkoptions({ '-arch arm64' })
    end

    filter({ 'system:macosx', 'options:arch=x64 or arch=universal' })
    do
        buildoptions({ '-arch x86_64' })
        linkoptions({ '-arch x86_64' })
    end

    filter({
        'system:ios',
        'options:variant=system',
        'options:arch=arm64 or arch=universal',
    })
    do
        buildoptions({ '-arch arm64' })
    end

    filter({
        'system:ios',
        'options:variant=emulator or variant=xrsimulator or variant=appletvsimulator',
        'options:arch=x64 or arch=universal',
    })
    do
        buildoptions({ '-arch x86_64' })
    end

    filter({
        'system:ios',
        'options:variant=emulator or variant=xrsimulator or variant=appletvsimulator',
        'options:arch=arm64 or arch=universal',
    })
    do
        buildoptions({ '-arch arm64' })
    end

    filter({
        'system:macosx',
        'options:variant=maccatalyst',
        'options:arch=arm64',
    })
    do
        buildoptions({ '-target arm64-apple-ios14.0-macabi' })
    end

    filter({
        'system:macosx',
        'options:variant=maccatalyst',
        'options:arch=x64',
    })
    do
        buildoptions({ '-target x86_64-apple-ios14.0-macabi' })
    end

    filter({})
end

if _OPTIONS['arch'] == 'wasm' or _OPTIONS['arch'] == 'js' then
    -- make a new system called "emscripten" so we don't try including host-os-specific files in the
    -- web build.
    premake.api.addAllowed('system', 'emscripten')

    -- clone the clang toolset into a custom one called "emsdk".
    premake.tools.emsdk = {}
    for k, v in pairs(premake.tools.clang) do
        premake.tools.emsdk[k] = v
    end

    -- update the emsdk toolset to use the appropriate binaries.
    local emsdk_tools = {
        cc = 'emcc',
        cxx = 'em++',
        ar = 'emar',
    }
    if os.host() == 'windows' then
        -- $PATH doesn't propogate to CreateProcess() on Windows. Resolve the
        -- paths and execute them explicitly as shell scripts.
        local emsdk_dir = os.getenv('EMSDK'):gsub("/", "\\")
        local emsdk_tools_dir = emsdk_dir .. "\\upstream\\emscripten\\"
        for key, value in pairs(emsdk_tools) do
            emsdk_tools[key] = "python3 " .. emsdk_tools_dir .. value .. ".py"
        end
    end
    function premake.tools.emsdk.gettoolname(cfg, tool)
        return emsdk_tools[tool]
    end

    -- suppress "** Warning: Unsupported toolset 'emsdk'".
    local premake_valid_tools = premake.action._list[_ACTION].valid_tools
    if premake_valid_tools ~= nil then
        premake_valid_tools['cc'][#premake_valid_tools['cc'] + 1] = 'emsdk'
    end

    system('emscripten')
    rive_target_os = 'emscripten'

    toolset('emsdk')

    linkoptions({ '-sALLOW_MEMORY_GROWTH=1', '-sDYNAMIC_EXECUTION=0' })

    filter('options:arch=wasm')
    do
        linkoptions({ '-sWASM=1' })
    end

    filter({ 'options:arch=wasm', 'options:not no-wasm-simd' })
    do
        buildoptions({ '-msimd128' })
    end

    filter({ 'options:arch=wasm', 'options:no-wasm-simd' })
    do
        -- 120200 (Safari 12.2) is the oldest emsdk >=4.0 supports; was 120000.
        linkoptions({ '-s MIN_SAFARI_VERSION=120200' })
    end

    filter('options:arch=js')
    do
        linkoptions({ '-sWASM=0' })
    end

    filter('options:wasm_single')
    do
        linkoptions({ '-sSINGLE_FILE=1' })
    end

    filter('options:config=release')
    do
        buildoptions({ '-Oz' })
    end

    filter({})
end

filter({})

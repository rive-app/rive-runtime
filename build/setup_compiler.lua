-- https://github.com/TurkeyMan/premake-emscripten.git adds "emscripten" as a valid system, but
-- premake5 still doesn't accept "--os=emscripten" from the command line. To work around this we add
-- a custom "--wasm" flag that sets system to "emscripten" for us.
newoption({
    trigger = 'emsdk',
    value = 'type',
    description = 'Build with emscripten',
    allowed = {
        { 'none', 'don\'t use emscripten' },
        { 'wasm', 'build WASM with emscripten' },
        { 'js', 'build Javascript with emscripten' },
    },
    default = 'none',
})
if _OPTIONS['emsdk'] ~= 'none' then
    -- Target emscripten via https://github.com/TurkeyMan/premake-emscripten.git
    -- Premake doesn't properly load the _preload.lua for this module, so we load it here manually.
    -- BUG: https://github.com/premake/premake-core/issues/1235
    dofile('premake-emscripten/_preload.lua')
    dofile('premake-emscripten/emscripten.lua')
    system('emscripten')
    toolset('emcc')
end

filter({ 'system:emscripten' })
do
    linkoptions({ '-sALLOW_MEMORY_GROWTH' })
end

filter({ 'system:emscripten', 'options:emsdk=wasm' })
do
    buildoptions({ '-msimd128' })
    linkoptions({ '-sWASM=1' })
end

filter({ 'system:emscripten', 'options:emsdk=js' })
do
    linkoptions({ '-sWASM=0' })
end

filter('system:not emscripten')
do
    toolset(_OPTIONS['toolset'] or 'clang')
end

filter('system:windows')
do
    staticruntime('on') -- Match Skia's /MT flag for link compatibility
    runtime('Release') -- Use /MT even in debug (/MTd is incompatible with Skia)
end

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

filter({})

newoption({
    trigger = 'with-rtti',
    description = 'don\'t disable rtti (nonstandard for Rive)',
})
newoption({
    trigger = 'with-exceptions',
    description = 'don\'t disable exceptions (nonstandard for Rive)',
})
filter({ 'options:not with-rtti' })
rtti('Off')
filter({ 'options:with-rtti' })
rtti('On')
filter({ 'options:not with-exceptions' })
exceptionhandling('Off')
filter({ 'options:with-exceptions' })
exceptionhandling('On')
filter({})

-- Don't use filter() here because we don't want to generate the "android_ndk" toolset if not
-- building for android.
if _OPTIONS['os'] == 'android' then
    local ndk = os.getenv('NDK_PATH')
    if not ndk or ndk == '' then
        error('export $NDK_PATH')
    end

    local ndk_toolchain = ndk .. '/toolchains/llvm/prebuilt'
    if os.host() == 'windows' then
        ndk_toolchain = ndk_toolchain .. '/windows-x86_64'
    elseif os.host() == 'macosx' then
        ndk_toolchain = ndk_toolchain .. '/darwin-x86_64'
    else
        ndk_toolchain = ndk_toolchain .. '/linux-x86_64'
    end

    -- clone the clang toolset into a custom one called "android_ndk".
    premake.tools.android_ndk = {}
    for k, v in pairs(premake.tools.clang) do
        premake.tools.android_ndk[k] = v
    end

    -- update the android_ndk toolset to use the appropriate binaries.
    local android_ndk_tools = {
        cc = ndk_toolchain .. '/bin/clang',
        cxx = ndk_toolchain .. '/bin/clang++',
        ar = ndk_toolchain .. '/bin/llvm-ar',
    }
    function premake.tools.android_ndk.gettoolname(cfg, tool)
        return android_ndk_tools[tool]
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

    pic('on') -- Position-independent code is required for NDK libraries.
    filter({ 'options:arch=x86', 'options:not for_unreal' })
    do
        architecture('x86')
        buildoptions({ '--target=i686-none-linux-android21' })
        linkoptions({ '--target=i686-none-linux-android21' })
    end

    filter({ 'options:arch=x86', 'options:for_unreal' })
    do
        architecture('x86')
        buildoptions({ '--target=i686-none-linux-android31' })
        linkoptions({ '--target=i686-none-linux-android31' })
    end

    filter({ 'options:arch=x64', 'options:not for_unreal' })
    do
        architecture('x64')
        buildoptions({ '--target=x86_64-none-linux-android21' })
        linkoptions({ '--target=x86_64-none-linux-android21' })
    end

    filter({ 'options:arch=x64', 'options:for_unreal' })
    do
        architecture('x64')
        buildoptions({ '--target=x86_64-none-linux-androi31' })
        linkoptions({ '--target=x86_64-none-linux-android31' })
    end

    filter({ 'options:arch=arm', 'options:not for_unreal' })
    do
        architecture('arm')
        buildoptions({ '--target=armv7a-none-linux-android21' })
        linkoptions({ '--target=armv7a-none-linux-android21' })
    end

    filter({ 'options:arch=arm', 'options:for_unreal' })
    do
        architecture('arm')
        buildoptions({ '--target=armv7a-none-linux-android31' })
        linkoptions({ '--target=armv7a-none-linux-android31' })
    end

    filter({ 'options:arch=arm64', 'options:not for_unreal' })
    do
        architecture('arm64')
        buildoptions({ '--target=aarch64-none-linux-android21' })
        linkoptions({ '--target=aarch64-none-linux-android21' })
    end

    filter({ 'options:arch=arm64', 'options:for_unreal' })
    do
        architecture('arm64')
        buildoptions({ '--target=aarch64-none-linux-android31' })
        linkoptions({ '--target=aarch64-none-linux-android31' })
    end
    filter({})
end

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
    allowed = { { 'x86' }, { 'x64' }, { 'arm' }, { 'arm64' } },
})

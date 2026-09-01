-- WAMR for the wasm scripting backend. Flags proven by the luau_wasm EH
-- spike: sw-bounds artifacts must never unwind through hardware traps
-- (the longjmp skips the sjlj glue's native frames) and libc-wasi must be
-- compiled in (without it emscripten-built module ctors spin; see
-- spike_eh/README.md). Bounds checks default sw; wasm_hw_bounds below.
local dependency = require('dependency')
-- The patch set carries the rive fast-interp fixes (linear_mem_size
-- refreshes around growth, gated OOB diagnostics); a clone without them is
-- subtly wrong under memory growth.
local wamr = dependency.github('bytecodealliance/wasm-micro-runtime', 'WAMR-2.4.5', {
    patches = path.join(path.getdirectory(_SCRIPT), 'wamr_patches'),
})
-- Header-only simd library the fast interpreter's v128 handlers compile
-- against; upstream fetches the same tag through CMake FetchContent.
local simde = dependency.github('simd-everywhere/simde', 'v0.8.2')

-- Struct layouts in wamr's internal headers follow these; any rive TU that
-- includes them (the tier transplant) must compile with the identical set
-- or instance layouts silently diverge.
local wamrConfigDefines = {
    'WASM_ENABLE_INTERP=1',
    'WASM_ENABLE_FAST_INTERP=1',
    'WASM_ENABLE_AOT=1',
    'WASM_ENABLE_JIT=0',
    'WASM_ENABLE_LIBC_BUILTIN=1',
    'WASM_ENABLE_LIBC_WASI=1',
    'WASM_ENABLE_MODULE_INST_CONTEXT=1',
    'WASM_ENABLE_BULK_MEMORY=1',
    -- Simde-backed v128 in the fast interpreter; scalar modules
    -- translate the same as before, simd modules stop being refused.
    'WASM_ENABLE_SIMD=1',
    'WASM_ENABLE_SIMDE=1',
    'WASM_DISABLE_STACK_HW_BOUND_CHECK=1',
    'BH_MALLOC=wasm_runtime_malloc',
    'BH_FREE=wasm_runtime_free',
}
-- Opt-in hw bounds serve AOT artifacts compiled --bounds-checks=0 only:
-- patch 0009 keeps the interpreter sw-checked and patch 0010 keeps sw
-- artifacts unwinding by checked returns, so the Luau lane never takes
-- a hardware trap. Native stack checks stay sw everywhere (artifacts
-- must carry --stack-bounds-checks=1).
if not _OPTIONS['wasm_hw_bounds'] then
    table.insert(wamrConfigDefines, 'WASM_DISABLE_HW_BOUND_CHECK=1')
end
if os.target() == 'windows' then
    -- We link wamr statically; under clang's msvc mode wasm_export.h and
    -- wasm_c_api.h otherwise declare every API dllimport.
    table.insert(wamrConfigDefines, 'WASM_RUNTIME_API_EXTERN=')
    table.insert(wamrConfigDefines, 'WASM_API_EXTERN=')
end

-- os.target() stays the host under --for_android; the option is the
-- truth for the platform dir.
local forAndroid = _OPTIONS['for_android'] ~= nil
local platformDir = forAndroid and 'android'
    or os.target() == 'linux' and 'linux'
    or os.target() == 'windows' and 'windows'
    or 'darwin'

project('wamr')
do
    kind('StaticLib')
    language('C')
    warnings('Off')
    -- Vendored dep nobody debugs into: an unoptimized interpreter is 6x
    -- slower, so every config builds it optimized. NDEBUG matches the CMake
    -- Release baseline; without it the interpreter's computed-goto dispatch
    -- tail-merges into a single prediction-killing branch.
    optimize('Speed')
    defines({ 'NDEBUG' })
    -- Replicated per-handler dispatch is what makes the fast interpreter
    -- fast; default heuristics tail-merge the computed gotos into a single
    -- indirect branch (+27% on box2d). Guarded by bench/check_dispatch_sites.
    -- LTO would run codegen at link where these -mllvm knobs cannot reach,
    -- so this library opts out and keeps its dispatch replication.
    buildoptions({ '-fno-lto' })
    -- The tail-dup knobs need llvm 19; older clangs build correct but
    -- slower dispatch, still caught by the dispatch-site guard where it runs.
    -- The probe runs host clang; cross toolchains (the NDK's) may reject
    -- the knobs the host accepts, so cross builds keep default dispatch.
    local devNull = os.ishost('windows') and 'NUL' or '/dev/null'
    local _, tailDupProbe = os.outputof(
        'clang -fsyntax-only -x c ' .. devNull ..
            ' -mllvm -tail-dup-pred-size=5000' ..
            ' -mllvm -tail-dup-succ-size=5000 2>&1'
    )
    if tailDupProbe == 0 and _OPTIONS['for_android'] == nil then
        buildoptions({
            '-mllvm -tail-dup-pred-size=5000',
            '-mllvm -tail-dup-succ-size=5000',
        })
    end
    defines(wamrConfigDefines)
    filter({ 'system:macosx' })
    defines({ 'BH_PLATFORM_DARWIN' })
    filter({ 'system:linux' })
    defines({ 'BH_PLATFORM_LINUX' })
    filter({ 'system:android' })
    defines({ 'BH_PLATFORM_ANDROID' })
    filter({ 'system:windows' })
    defines({
        'BH_PLATFORM_WINDOWS',
        'HAVE_STRUCT_TIMESPEC',
        '_WINSOCK_DEPRECATED_NO_WARNINGS',
    })
    filter({})
    -- Per-lane arch beats host arch: universal builds compile both lanes
    -- on one machine and each must get its own invoke shim and relocs.
    local archOption = _OPTIONS['arch']
    local machine = os.outputof('uname -m')
    local isArm64
    -- 'host' is the option's default, not an explicit lane.
    if archOption ~= nil and archOption ~= '' and archOption ~= 'host' then
        isArm64 = archOption == 'arm64' or archOption == 'aarch64'
    else
        isArm64 = machine == 'arm64' or machine == 'aarch64'
    end
    local isArm32 = forAndroid and archOption == 'arm'
    -- The quoted string define goes through buildoptions pre-escaped so both
    -- the gmake and ninja generators deliver the quotes to the compiler.
    if isArm32 then
        -- NDK armv7 compiles thumb2 with NEON/VFP by default. Upstream's
        -- thumb reloc does pointer arithmetic clang 18 makes a hard error.
        -- The THUMBV7 string matters: the aot loader matches it against
        -- wamrc's thumbv7 artifacts (bare THUMB defaults to thumbv4t).
        defines({ 'BUILD_TARGET_THUMB_VFP' })
        buildoptions({
            '-DBUILD_TARGET=\\"THUMBV7\\"',
            '-Wno-int-conversion',
        })
    elseif isArm64 then
        defines({ 'BUILD_TARGET_AARCH64' })
        buildoptions({ '-DBUILD_TARGET=\\"AARCH64\\"' })
    else
        defines({ 'BUILD_TARGET_X86_64' })
        buildoptions({ '-DBUILD_TARGET=\\"X86_64\\"' })
    end
    -- em64 is WAMR's x86-64 SysV invoke shim. With WASM_ENABLE_SIMD the
    -- invoke marshaling widens float slots to v128, so the shim must be the
    -- _simd variant or int registers load from the wrong offsets. arm32 has
    -- no simd invoke variant; its marshaling never widens. Windows takes
    -- the mingw shim: same Win64 ABI as MSVC targets, and GAS syntax that
    -- clang's integrated assembler handles without ml64.
    local invokeNative = isArm32 and 'invokeNative_thumb_vfp.s'
        or isArm64 and 'invokeNative_aarch64_simd.s'
        or os.target() == 'windows' and 'invokeNative_mingw_x64_simd.s'
        or 'invokeNative_em64_simd.s'
    local aotReloc = isArm32 and 'aot_reloc_thumb.c'
        or isArm64 and 'aot_reloc_aarch64.c'
        or 'aot_reloc_x86_64.c'
    includedirs({
        wamr .. '/core/iwasm/include',
        wamr .. '/core/iwasm/common',
        wamr .. '/core/iwasm/aot',
        wamr .. '/core/iwasm/interpreter',
        wamr .. '/core/iwasm/libraries/libc-wasi/sandboxed-system-primitives/include',
        wamr .. '/core/iwasm/libraries/libc-wasi/sandboxed-system-primitives/src',
        wamr .. '/core/shared/include',
        wamr .. '/core/shared/platform/include',
        -- platform_internal.h comes from the per-platform dir.
        wamr .. '/core/shared/platform/' .. platformDir,
        wamr .. '/core/shared/platform/common/libc-util',
        wamr .. '/core/shared/mem-alloc',
        -- wasm_runtime_common.h pulls the wasi primitives when libc-wasi is
        -- on, which our config always is.
        wamr .. '/core/iwasm/libraries/libc-wasi/sandboxed-system-primitives/src',
        wamr .. '/core/iwasm/libraries/libc-wasi/sandboxed-system-primitives/include',
        wamr .. '/core/shared/utils',
        wamr .. '/core/shared/utils/uncommon',
        simde,
    })
    files({
        wamr .. '/core/iwasm/common/*.c',
        wamr .. '/core/iwasm/common/arch/' .. invokeNative,
        wamr .. '/core/iwasm/interpreter/wasm_loader.c',
        wamr .. '/core/iwasm/interpreter/wasm_runtime.c',
        wamr .. '/core/iwasm/interpreter/wasm_interp_fast.c',
        wamr .. '/core/iwasm/aot/*.c',
        wamr .. '/core/iwasm/aot/arch/' .. aotReloc,
        wamr .. '/core/iwasm/libraries/libc-builtin/*.c',
        wamr .. '/core/iwasm/libraries/libc-wasi/**.c',
        wamr .. '/core/shared/platform/' .. platformDir .. '/*.c',
        -- Windows replaces the posix layer wholesale; win_atomic is C++.
        os.target() == 'windows'
                and (wamr .. '/core/shared/platform/windows/*.cpp')
            or (wamr .. '/core/shared/platform/common/posix/*.c'),
        wamr .. '/core/shared/platform/common/memory/*.c',
        wamr .. '/core/shared/platform/common/libc-util/*.c',
        wamr .. '/core/shared/mem-alloc/*.c',
        wamr .. '/core/shared/mem-alloc/ems/*.c',
        wamr .. '/core/shared/utils/*.c',
        wamr .. '/core/shared/utils/uncommon/*.c',
    })
    if os.target() == 'windows' then
        -- Linux perf-map support; leans on pid_t/getpid.
        removefiles({ wamr .. '/core/iwasm/aot/aot_perf_map.c' })
    end
end

return {
    wamr = wamr,
    configDefines = wamrConfigDefines,
    internalIncludes = {
        wamr .. '/core/iwasm/interpreter',
        wamr .. '/core/iwasm/aot',
        wamr .. '/core/iwasm/common',
        wamr .. '/core/shared/include',
        wamr .. '/core/shared/platform/include',
        -- platform_internal.h comes from the per-platform dir.
        wamr .. '/core/shared/platform/' .. platformDir,
        wamr .. '/core/shared/mem-alloc',
        -- wasm_runtime_common.h pulls the wasi primitives when libc-wasi is
        -- on, which our config always is.
        wamr .. '/core/iwasm/libraries/libc-wasi/sandboxed-system-primitives/src',
        wamr .. '/core/iwasm/libraries/libc-wasi/sandboxed-system-primitives/include',
        wamr .. '/core/shared/utils',
    },
}

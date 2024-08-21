require('setup_compiler')
local dependency = require('dependency')
harfbuzz = dependency.github('rive-app/harfbuzz', 'rive_8.3.0')

workspace('rive')
configurations({ 'debug', 'release' })

project('rive_harfbuzz')
do
    kind('StaticLib')
    language('C++')
    cppdialect('C++17')
    targetdir('%{cfg.system}/cache/bin/%{cfg.buildcfg}/')
    objdir('%{cfg.system}/cache/obj/%{cfg.buildcfg}/')

    includedirs({ '../', harfbuzz .. '/src' })

    files({
        harfbuzz .. '/src/hb-aat-layout-ankr-table.hh',
        harfbuzz .. '/src/hb-aat-layout-bsln-table.hh',
        harfbuzz .. '/src/hb-aat-layout-common.hh',
        harfbuzz .. '/src/hb-aat-layout-feat-table.hh',
        harfbuzz .. '/src/hb-aat-layout-just-table.hh',
        harfbuzz .. '/src/hb-aat-layout-kerx-table.hh',
        harfbuzz .. '/src/hb-aat-layout-morx-table.hh',
        harfbuzz .. '/src/hb-aat-layout-opbd-table.hh',
        harfbuzz .. '/src/hb-aat-layout-trak-table.hh',
        harfbuzz .. '/src/hb-aat-layout.cc',
        harfbuzz .. '/src/hb-aat-layout.hh',
        harfbuzz .. '/src/hb-aat-ltag-table.hh',
        harfbuzz .. '/src/hb-aat-map.cc',
        harfbuzz .. '/src/hb-aat-map.hh',
        harfbuzz .. '/src/hb-aat.h',
        harfbuzz .. '/src/hb-algs.hh',
        harfbuzz .. '/src/hb-array.hh',
        harfbuzz .. '/src/hb-atomic.hh',
        harfbuzz .. '/src/hb-bimap.hh',
        harfbuzz .. '/src/hb-bit-page.hh',
        harfbuzz .. '/src/hb-bit-set-invertible.hh',
        harfbuzz .. '/src/hb-bit-set.hh',
        harfbuzz .. '/src/hb-blob.cc',
        harfbuzz .. '/src/hb-blob.hh',
        harfbuzz .. '/src/hb-buffer-deserialize-json.hh',
        harfbuzz .. '/src/hb-buffer-deserialize-text.hh',
        harfbuzz .. '/src/hb-buffer-serialize.cc',
        harfbuzz .. '/src/hb-buffer-verify.cc',
        harfbuzz .. '/src/hb-buffer.cc',
        harfbuzz .. '/src/hb-buffer.hh',
        harfbuzz .. '/src/hb-cache.hh',
        harfbuzz .. '/src/hb-cff-interp-common.hh',
        harfbuzz .. '/src/hb-cff-interp-cs-common.hh',
        harfbuzz .. '/src/hb-cff-interp-dict-common.hh',
        harfbuzz .. '/src/hb-cff1-interp-cs.hh',
        harfbuzz .. '/src/hb-cff2-interp-cs.hh',
        harfbuzz .. '/src/hb-common.cc',
        harfbuzz .. '/src/hb-config.hh',
        harfbuzz .. '/src/hb-debug.hh',
        harfbuzz .. '/src/hb-dispatch.hh',
        harfbuzz .. '/src/hb-draw.cc',
        harfbuzz .. '/src/hb-draw.h',
        harfbuzz .. '/src/hb-draw.hh',
        harfbuzz .. '/src/hb-face.cc',
        harfbuzz .. '/src/hb-face.hh',
        harfbuzz .. '/src/hb-font.cc',
        harfbuzz .. '/src/hb-font.hh',
        harfbuzz .. '/src/hb-iter.hh',
        harfbuzz .. '/src/hb-kern.hh',
        harfbuzz .. '/src/hb-machinery.hh',
        harfbuzz .. '/src/hb-map.cc',
        harfbuzz .. '/src/hb-map.hh',
        harfbuzz .. '/src/hb-meta.hh',
        harfbuzz .. '/src/hb-ms-feature-ranges.hh',
        harfbuzz .. '/src/hb-mutex.hh',
        harfbuzz .. '/src/hb-null.hh',
        harfbuzz .. '/src/hb-number-parser.hh',
        harfbuzz .. '/src/hb-number.cc',
        harfbuzz .. '/src/hb-number.hh',
        harfbuzz .. '/src/hb-object.hh',
        harfbuzz .. '/src/hb-open-file.hh',
        harfbuzz .. '/src/hb-open-type.hh',
        harfbuzz .. '/src/hb-ot-cff-common.hh',
        harfbuzz .. '/src/hb-ot-cff1-std-str.hh',
        harfbuzz .. '/src/hb-ot-cff1-table.cc',
        harfbuzz .. '/src/hb-ot-cff1-table.hh',
        harfbuzz .. '/src/hb-ot-cff2-table.cc',
        harfbuzz .. '/src/hb-ot-cff2-table.hh',
        harfbuzz .. '/src/hb-ot-cmap-table.hh',
        harfbuzz .. '/src/hb-ot-color-cbdt-table.hh',
        harfbuzz .. '/src/hb-ot-color-colr-table.hh',
        harfbuzz .. '/src/hb-ot-color-colrv1-closure.hh',
        harfbuzz .. '/src/hb-ot-color-cpal-table.hh',
        harfbuzz .. '/src/hb-ot-color-sbix-table.hh',
        harfbuzz .. '/src/hb-ot-color-svg-table.hh',
        harfbuzz .. '/src/hb-ot-color.cc',
        harfbuzz .. '/src/hb-ot-color.h',
        harfbuzz .. '/src/hb-ot-deprecated.h',
        harfbuzz .. '/src/hb-ot-face-table-list.hh',
        harfbuzz .. '/src/hb-ot-face.cc',
        harfbuzz .. '/src/hb-ot-face.hh',
        harfbuzz .. '/src/hb-ot-font.cc',
        harfbuzz .. '/src/hb-ot-gasp-table.hh',
        harfbuzz .. '/src/hb-ot-glyf-table.hh',
        harfbuzz .. '/src/hb-ot-hdmx-table.hh',
        harfbuzz .. '/src/hb-ot-head-table.hh',
        harfbuzz .. '/src/hb-ot-hhea-table.hh',
        harfbuzz .. '/src/hb-ot-hmtx-table.hh',
        harfbuzz .. '/src/hb-ot-kern-table.hh',
        harfbuzz .. '/src/hb-ot-layout-base-table.hh',
        harfbuzz .. '/src/hb-ot-layout-common.hh',
        harfbuzz .. '/src/hb-ot-layout-gdef-table.hh',
        harfbuzz .. '/src/hb-ot-layout-gpos-table.hh',
        harfbuzz .. '/src/hb-ot-layout-gsub-table.hh',
        harfbuzz .. '/src/hb-ot-layout-gsubgpos.hh',
        harfbuzz .. '/src/hb-ot-layout-jstf-table.hh',
        harfbuzz .. '/src/hb-ot-layout.cc',
        harfbuzz .. '/src/hb-ot-layout.hh',
        harfbuzz .. '/src/hb-ot-map.cc',
        harfbuzz .. '/src/hb-ot-map.hh',
        harfbuzz .. '/src/hb-ot-math-table.hh',
        harfbuzz .. '/src/hb-ot-math.cc',
        harfbuzz .. '/src/hb-ot-maxp-table.hh',
        harfbuzz .. '/src/hb-ot-meta-table.hh',
        harfbuzz .. '/src/hb-ot-meta.cc',
        harfbuzz .. '/src/hb-ot-meta.h',
        harfbuzz .. '/src/hb-ot-metrics.cc',
        harfbuzz .. '/src/hb-ot-metrics.hh',
        harfbuzz .. '/src/hb-ot-name-language-static.hh',
        harfbuzz .. '/src/hb-ot-name-language.hh',
        harfbuzz .. '/src/hb-ot-name-table.hh',
        harfbuzz .. '/src/hb-ot-name.cc',
        harfbuzz .. '/src/hb-ot-name.h',
        harfbuzz .. '/src/hb-ot-os2-table.hh',
        harfbuzz .. '/src/hb-ot-os2-unicode-ranges.hh',
        harfbuzz .. '/src/hb-ot-post-macroman.hh',
        harfbuzz .. '/src/hb-ot-post-table-v2subset.hh',
        harfbuzz .. '/src/hb-ot-post-table.hh',
        harfbuzz .. '/src/hb-ot-shaper-arabic-fallback.hh',
        harfbuzz .. '/src/hb-ot-shaper-arabic-joining-list.hh',
        harfbuzz .. '/src/hb-ot-shaper-arabic-pua.hh',
        harfbuzz .. '/src/hb-ot-shaper-arabic-table.hh',
        harfbuzz .. '/src/hb-ot-shaper-arabic-win1256.hh',
        harfbuzz .. '/src/hb-ot-shaper-arabic.cc',
        harfbuzz .. '/src/hb-ot-shaper-arabic.hh',
        harfbuzz .. '/src/hb-ot-shaper-default.cc',
        harfbuzz .. '/src/hb-ot-shaper-hangul.cc',
        harfbuzz .. '/src/hb-ot-shaper-hebrew.cc',
        harfbuzz .. '/src/hb-ot-shaper-indic-table.cc',
        harfbuzz .. '/src/hb-ot-shaper-indic.cc',
        harfbuzz .. '/src/hb-ot-shaper-indic.hh',
        harfbuzz .. '/src/hb-ot-shaper-khmer.cc',
        harfbuzz .. '/src/hb-ot-shaper-myanmar.cc',
        harfbuzz .. '/src/hb-ot-shaper-syllabic.cc',
        harfbuzz .. '/src/hb-ot-shaper-syllabic.hh',
        harfbuzz .. '/src/hb-ot-shaper-thai.cc',
        harfbuzz .. '/src/hb-ot-shaper-use-table.hh',
        harfbuzz .. '/src/hb-ot-shaper-use.cc',
        harfbuzz .. '/src/hb-ot-shaper-vowel-constraints.cc',
        harfbuzz .. '/src/hb-ot-shaper-vowel-constraints.hh',
        harfbuzz .. '/src/hb-ot-shaper.hh',
        harfbuzz .. '/src/hb-ot-shaper-indic-machine.hh',
        harfbuzz .. '/src/hb-ot-shaper-khmer-machine.hh',
        harfbuzz .. '/src/hb-ot-shaper-myanmar-machine.hh',
        harfbuzz .. '/src/hb-ot-shaper-use-machine.hh',
        harfbuzz .. '/src/hb-ot-shape-fallback.cc',
        harfbuzz .. '/src/hb-ot-shape-fallback.hh',
        harfbuzz .. '/src/hb-ot-shape-normalize.cc',
        harfbuzz .. '/src/hb-ot-shape-normalize.hh',
        harfbuzz .. '/src/hb-ot-shape.cc',
        harfbuzz .. '/src/hb-ot-shape.hh',
        harfbuzz .. '/src/hb-ot-stat-table.hh',
        harfbuzz .. '/src/hb-ot-tag-table.hh',
        harfbuzz .. '/src/hb-ot-tag.cc',
        harfbuzz .. '/src/hb-ot-var-avar-table.hh',
        harfbuzz .. '/src/hb-ot-var-common.hh',
        harfbuzz .. '/src/hb-ot-var-fvar-table.hh',
        harfbuzz .. '/src/hb-ot-var-gvar-table.hh',
        harfbuzz .. '/src/hb-ot-var-hvar-table.hh',
        harfbuzz .. '/src/hb-ot-var-mvar-table.hh',
        harfbuzz .. '/src/hb-ot-var.cc',
        harfbuzz .. '/src/hb-ot-vorg-table.hh',
        harfbuzz .. '/src/hb-pool.hh',
        harfbuzz .. '/src/hb-priority-queue.hh',
        harfbuzz .. '/src/hb-repacker.hh',
        harfbuzz .. '/src/hb-sanitize.hh',
        harfbuzz .. '/src/hb-serialize.hh',
        harfbuzz .. '/src/hb-set-digest.hh',
        harfbuzz .. '/src/hb-set.cc',
        harfbuzz .. '/src/hb-set.hh',
        harfbuzz .. '/src/hb-shape-plan.cc',
        harfbuzz .. '/src/hb-shape-plan.hh',
        harfbuzz .. '/src/hb-shape.cc',
        harfbuzz .. '/src/hb-shaper-impl.hh',
        harfbuzz .. '/src/hb-shaper-list.hh',
        harfbuzz .. '/src/hb-shaper.cc',
        harfbuzz .. '/src/hb-shaper.hh',
        harfbuzz .. '/src/hb-static.cc',
        harfbuzz .. '/src/hb-string-array.hh',
        harfbuzz .. '/src/hb-subset-cff-common.cc',
        harfbuzz .. '/src/hb-subset-cff-common.hh',
        harfbuzz .. '/src/hb-subset-cff1.cc',
        harfbuzz .. '/src/hb-subset-cff1.hh',
        harfbuzz .. '/src/hb-subset-cff2.cc',
        harfbuzz .. '/src/hb-subset-cff2.hh',
        harfbuzz .. '/src/hb-subset-input.cc',
        harfbuzz .. '/src/hb-subset-input.hh',
        harfbuzz .. '/src/hb-subset-plan.cc',
        harfbuzz .. '/src/hb-subset-plan.hh',
        harfbuzz .. '/src/hb-subset-repacker.cc',
        harfbuzz .. '/src/hb-subset-repacker.h',
        harfbuzz .. '/src/hb-subset.cc',
        harfbuzz .. '/src/hb-subset.hh',
        harfbuzz .. '/src/hb-ucd-table.hh',
        harfbuzz .. '/src/hb-ucd.cc',
        harfbuzz .. '/src/hb-unicode-emoji-table.hh',
        harfbuzz .. '/src/hb-unicode.cc',
        harfbuzz .. '/src/hb-unicode.hh',
        harfbuzz .. '/src/hb-utf.hh',
        harfbuzz .. '/src/hb-vector.hh',
        harfbuzz .. '/src/hb.hh',
        harfbuzz .. '/src/graph/gsubgpos-context.cc',
        harfbuzz .. '/src/hb-paint.cc',
        harfbuzz .. '/src/hb-paint-extents.cc',
        harfbuzz .. '/src/hb-outline.cc',
    })

    warnings('Off')

    defines({
        'HAVE_OT',
        'HB_NO_FALLBACK_SHAPE',
        'HB_NO_WIN1256',
        'HB_NO_EXTERN_HELPERS',
        'HB_DISABLE_DEPRECATED',
        'HB_NO_COLOR',
        'HB_NO_BITMAP',
        'HB_NO_BUFFER_SERIALIZE',
        'HB_NO_SETLOCALE',
        'HB_NO_STYLE',
        'HB_NO_VERTICAL',
        'HB_NO_LAYOUT_COLLECT_GLYPHS',
        'HB_NO_LAYOUT_RARELY_USED',
        'HB_NO_LAYOUT_UNUSED',
        'HB_NO_OT_FONT_GLYPH_NAMES',
    })

    filter('system:emscripten')
    do
        buildoptions({ '-pthread' })
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
    filter('toolset:msc')
    do
        buildoptions({
            '/bigobj',
        })
    end

    filter('configurations:debug')
    do
        defines({ 'DEBUG' })
        symbols('On')
    end

    filter('configurations:release')
    do
        defines({ 'RELEASE' })
        defines({ 'NDEBUG' })
        optimize('On')
    end

    filter({ 'system:macosx', 'options:variant=runtime' })
    do
        buildoptions({
            '-Wimplicit-float-conversion -fembed-bitcode -arch arm64 -arch x86_64 -isysroot '
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

    filter('system:macosx or system:ios')
    do
        defines({ 'HAVE_CORETEXT' })
        files({ harfbuzz .. '/src/hb-coretext.cc' })
    end
end

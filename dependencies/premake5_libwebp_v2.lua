dofile('rive_build_config.lua')

local dependency = require('dependency')
libwebp = dependency.github('webmproject/libwebp', 'v1.4.0')

project('libwebp')
do
    kind('StaticLib')
    optimize('Speed') -- Always optimize image encoding/decoding, even in debug builds.

    includedirs({ libwebp })

    -- Leaving some notes here for future perf improvements. Define these when
    -- we can determine we're on a compatible platform/perf gain is worth it.
    --
    -- Some extra details about each of these:
    -- https://github.com/webmproject/libwebp/blob/main/cmake/config.h.in
    defines({
        -- 'WEBP_USE_NEON=1',
        -- 'WEBP_HAVE_NEON_RTCD=1', -- runtime detection of NEON extensions
        -- 'WEBP_HAVE_SSE41=1',
        -- 'WEBP_USE_THREAD=1'
    })

    files({
        -- common dsp
        libwebp .. '/src/dsp/alpha_processing.c',
        libwebp .. '/src/dsp/cpu.c',
        libwebp .. '/src/dsp/dec.c',
        libwebp .. '/src/dsp/dec_clip_tables.c',
        libwebp .. '/src/dsp/filters.c',
        libwebp .. '/src/dsp/lossless.c',
        libwebp .. '/src/dsp/rescaler.c',
        libwebp .. '/src/dsp/upsampling.c',
        libwebp .. '/src/dsp/yuv.c',

        -- encoder dsp
        libwebp .. '/src/dsp/cost.c',
        libwebp .. '/src/dsp/enc.c',
        libwebp .. '/src/dsp/lossless_enc.c',
        libwebp .. '/src/dsp/ssim.c',

        -- decoder
        libwebp .. '/src/dec/alpha_dec.c',
        libwebp .. '/src/dec/buffer_dec.c',
        libwebp .. '/src/dec/frame_dec.c',
        libwebp .. '/src/dec/idec_dec.c',
        libwebp .. '/src/dec/io_dec.c',
        libwebp .. '/src/dec/quant_dec.c',
        libwebp .. '/src/dec/tree_dec.c',
        libwebp .. '/src/dec/vp8_dec.c',
        libwebp .. '/src/dec/vp8l_dec.c',
        libwebp .. '/src/dec/webp_dec.c',

        -- libwebpdspdecode_sse41_la_SOURCES =
        libwebp .. '/src/dsp/alpha_processing_sse41.c',
        libwebp .. '/src/dsp/dec_sse41.c',
        libwebp .. '/src/dsp/lossless_sse41.c',
        libwebp .. '/src/dsp/upsampling_sse41.c',
        libwebp .. '/src/dsp/yuv_sse41.c',

        -- libwebpdspdecode_sse2_la_SOURCES =
        libwebp .. '/src/dsp/alpha_processing_sse2.c',
        libwebp .. '/src/dsp/common_sse2.h',
        libwebp .. '/src/dsp/dec_sse2.c',
        libwebp .. '/src/dsp/filters_sse2.c',
        libwebp .. '/src/dsp/lossless_sse2.c',
        libwebp .. '/src/dsp/rescaler_sse2.c',
        libwebp .. '/src/dsp/upsampling_sse2.c',
        libwebp .. '/src/dsp/yuv_sse2.c',

        -- neon sources
        -- TODO: define WEBP_HAVE_NEON when we're on a platform that supports it.
        libwebp .. '/src/dsp/alpha_processing_neon.c',
        libwebp .. '/src/dsp/dec_neon.c',
        libwebp .. '/src/dsp/filters_neon.c',
        libwebp .. '/src/dsp/lossless_neon.c',
        libwebp .. '/src/dsp/neon.h',
        libwebp .. '/src/dsp/rescaler_neon.c',
        libwebp .. '/src/dsp/upsampling_neon.c',
        libwebp .. '/src/dsp/yuv_neon.c',

        -- libwebpdspdecode_msa_la_SOURCES =
        libwebp .. '/src/dsp/dec_msa.c',
        libwebp .. '/src/dsp/filters_msa.c',
        libwebp .. '/src/dsp/lossless_msa.c',
        libwebp .. '/src/dsp/msa_macro.h',
        libwebp .. '/src/dsp/rescaler_msa.c',
        libwebp .. '/src/dsp/upsampling_msa.c',

        -- libwebpdspdecode_mips32_la_SOURCES =
        libwebp .. '/src/dsp/dec_mips32.c',
        libwebp .. '/src/dsp/mips_macro.h',
        libwebp .. '/src/dsp/rescaler_mips32.c',
        libwebp .. '/src/dsp/yuv_mips32.c',

        -- libwebpdspdecode_mips_dsp_r2_la_SOURCES =
        libwebp .. '/src/dsp/alpha_processing_mips_dsp_r2.c',
        libwebp .. '/src/dsp/dec_mips_dsp_r2.c',
        libwebp .. '/src/dsp/filters_mips_dsp_r2.c',
        libwebp .. '/src/dsp/lossless_mips_dsp_r2.c',
        libwebp .. '/src/dsp/mips_macro.h',
        libwebp .. '/src/dsp/rescaler_mips_dsp_r2.c',
        libwebp .. '/src/dsp/upsampling_mips_dsp_r2.c',
        libwebp .. '/src/dsp/yuv_mips_dsp_r2.c',

        -- libwebpdsp_sse2_la_SOURCES =
        libwebp .. '/src/dsp/cost_sse2.c',
        libwebp .. '/src/dsp/enc_sse2.c',
        libwebp .. '/src/dsp/lossless_enc_sse2.c',
        libwebp .. '/src/dsp/ssim_sse2.c',

        -- libwebpdsp_sse41_la_SOURCES =
        libwebp .. '/src/dsp/enc_sse41.c',
        libwebp .. '/src/dsp/lossless_enc_sse41.c',

        -- libwebpdsp_neon_la_SOURCES =
        libwebp .. '/src/dsp/cost_neon.c',
        libwebp .. '/src/dsp/enc_neon.c',
        libwebp .. '/src/dsp/lossless_enc_neon.c',

        -- libwebpdsp_msa_la_SOURCES =
        libwebp .. '/src/dsp/enc_msa.c',
        libwebp .. '/src/dsp/lossless_enc_msa.c',

        -- libwebpdsp_mips32_la_SOURCES =
        libwebp .. '/src/dsp/cost_mips32.c',
        libwebp .. '/src/dsp/enc_mips32.c',
        libwebp .. '/src/dsp/lossless_enc_mips32.c',

        -- libwebpdsp_mips_dsp_r2_la_SOURCES =
        libwebp .. '/src/dsp/cost_mips_dsp_r2.c',
        libwebp .. '/src/dsp/enc_mips_dsp_r2.c',
        libwebp .. '/src/dsp/lossless_enc_mips_dsp_r2.c',

        -- COMMON_SOURCES =
        libwebp .. '/src/utils/bit_reader_utils.c',
        libwebp .. '/src/utils/bit_reader_utils.h',
        libwebp .. '/src/utils/color_cache_utils.c',
        libwebp .. '/src/utils/filters_utils.c',
        libwebp .. '/src/utils/huffman_utils.c',
        libwebp .. '/src/utils/palette.c',
        libwebp .. '/src/utils/quant_levels_dec_utils.c',
        libwebp .. '/src/utils/rescaler_utils.c',
        libwebp .. '/src/utils/random_utils.c',
        libwebp .. '/src/utils/thread_utils.c',
        libwebp .. '/src/utils/utils.c',

        -- ENC_SOURCES =
        libwebp .. '/src/utils/bit_writer_utils.c',
        libwebp .. '/src/utils/huffman_encode_utils.c',
        libwebp .. '/src/utils/quant_levels_utils.c',

        -- libwebpdemux_la_SOURCES =
        libwebp .. '/src/demux/anim_decode.c',
        libwebp .. '/src/demux/demux.c',
    })

    filter({ 'system:windows', 'toolset:clang' })
    do
        -- https://github.com/webmproject/libwebp/blob/233e86b91f4e0af7833d50013e3b978f825f73f5/src/dsp/cpu.h#L57
        -- webp automaticall enables these for windows so we need to compile
        -- with the correct settings or we get an error.
        buildoptions({ '-mssse3', '-msse4.1' })
    end
end

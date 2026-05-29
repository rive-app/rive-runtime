#ifdef WITH_RIVE_SCRIPTING
#include "lualib.h"

#include <cstring>
#include <cstdint>

#if defined(__aarch64__) || defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
#define RIVE_BUF_NEON_F16 1
#endif
#endif

#if defined(__F16C__)
#include <immintrin.h>
#define RIVE_BUF_F16C 1
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

#define isoutofbounds(offset, len, accessize)                                  \
    (uint64_t(unsigned(offset)) + (accessize) > uint64_t(len))

// --------------------------------------------------------------------------
// Half-float (IEEE 754 half-precision) — scalar fallback
// --------------------------------------------------------------------------

static float halfToFloat(uint16_t h)
{
    uint32_t sign = (uint32_t)(h >> 15) << 31;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;

    if (exp == 0)
    {
        if (mant == 0)
        {
            uint32_t bits = sign;
            float f;
            memcpy(&f, &bits, 4);
            return f;
        }
        exp = 1;
        while ((mant & 0x400) == 0)
        {
            mant <<= 1;
            exp--;
        }
        mant &= 0x3FF;
        uint32_t bits = sign | ((exp + 127 - 15) << 23) | (mant << 13);
        float f;
        memcpy(&f, &bits, 4);
        return f;
    }
    if (exp == 31)
    {
        uint32_t bits = sign | 0x7F800000 | (mant << 13);
        float f;
        memcpy(&f, &bits, 4);
        return f;
    }
    uint32_t bits = sign | ((exp + 127 - 15) << 23) | (mant << 13);
    float f;
    memcpy(&f, &bits, 4);
    return f;
}

static uint16_t floatToHalf(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, 4);

    uint32_t sign = (bits >> 16) & 0x8000;
    int32_t exp = ((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t mant = bits & 0x7FFFFF;

    if (exp <= 0)
    {
        if (exp < -10)
            return (uint16_t)sign;
        mant |= 0x800000;
        uint32_t shift = (uint32_t)(1 - exp);
        uint32_t round = mant & ((1u << (shift + 13)) - 1);
        mant >>= (shift + 13);
        if (round > (1u << (shift + 12)) ||
            (round == (1u << (shift + 12)) && (mant & 1)))
            mant++;
        return (uint16_t)(sign | mant);
    }
    if (exp == 0xFF - 127 + 15)
    {
        if (mant == 0)
            return (uint16_t)(sign | 0x7C00);
        return (uint16_t)(sign | 0x7C00 | (mant >> 13));
    }
    uint32_t round = mant & 0x1FFF;
    mant >>= 13;
    if (round > 0x1000 || (round == 0x1000 && (mant & 1)))
    {
        mant++;
        if (mant >= 0x400)
        {
            mant = 0;
            exp++;
        }
    }
    if (exp >= 31)
        return (uint16_t)(sign | 0x7C00);
    return (uint16_t)(sign | ((uint32_t)exp << 10) | mant);
}

// --------------------------------------------------------------------------
// SIMD bulk f32↔f16 conversion
// --------------------------------------------------------------------------

static void convertF32ToF16Bulk(const float* src, uint16_t* dst, int count)
{
    int i = 0;
#if RIVE_BUF_NEON_F16
    for (; i + 3 < count; i += 4)
    {
        float32x4_t v = vld1q_f32(src + i);
        float16x4_t h = vcvt_f16_f32(v);
        vst1_u16(dst + i, vreinterpret_u16_f16(h));
    }
#elif RIVE_BUF_F16C
    for (; i + 3 < count; i += 4)
    {
        __m128 v = _mm_loadu_ps(src + i);
        __m128i h = _mm_cvtps_ph(v, _MM_FROUND_TO_NEAREST_INT);
        _mm_storel_epi64((__m128i*)(dst + i), h);
    }
#endif
    for (; i < count; i++)
        dst[i] = floatToHalf(src[i]);
}

static void convertF16ToF32Bulk(const uint16_t* src, float* dst, int count)
{
    int i = 0;
#if RIVE_BUF_NEON_F16
    for (; i + 3 < count; i += 4)
    {
        uint16x4_t h = vld1_u16(src + i);
        float32x4_t v = vcvt_f32_f16(vreinterpret_f16_u16(h));
        vst1q_f32(dst + i, v);
    }
#elif RIVE_BUF_F16C
    for (; i + 3 < count; i += 4)
    {
        __m128i h = _mm_loadl_epi64((const __m128i*)(src + i));
        __m128 v = _mm_cvtph_ps(h);
        _mm_storeu_ps(dst + i, v);
    }
#endif
    for (; i < count; i++)
        dst[i] = halfToFloat(src[i]);
}

// --------------------------------------------------------------------------
// buffer.readf16 / buffer.writef16
// --------------------------------------------------------------------------

static int buffer_readf16(lua_State* L)
{
    size_t len = 0;
    void* buf = luaL_checkbuffer(L, 1, &len);
    int offset = luaL_checkinteger(L, 2);

    if (isoutofbounds(offset, len, 2))
        luaL_error(L, "buffer access out of bounds");

    uint16_t half;
    memcpy(&half, (char*)buf + offset, 2);
    lua_pushnumber(L, (double)halfToFloat(half));
    return 1;
}

static int buffer_writef16(lua_State* L)
{
    size_t len = 0;
    void* buf = luaL_checkbuffer(L, 1, &len);
    int offset = luaL_checkinteger(L, 2);
    double value = luaL_checknumber(L, 3);

    if (isoutofbounds(offset, len, 2))
        luaL_error(L, "buffer access out of bounds");

    uint16_t half = floatToHalf((float)value);
    memcpy((char*)buf + offset, &half, 2);
    return 0;
}

// --------------------------------------------------------------------------
// buffer.stridedcopy
// --------------------------------------------------------------------------

static int buffer_stridedcopy(lua_State* L)
{
    size_t dstLen = 0, srcLen = 0;
    void* dst = luaL_checkbuffer(L, 1, &dstLen);
    int dstOffset = luaL_checkinteger(L, 2);
    int dstStride = luaL_checkinteger(L, 3);
    void* src = luaL_checkbuffer(L, 4, &srcLen);
    int srcOffset = luaL_checkinteger(L, 5);
    int srcStride = luaL_checkinteger(L, 6);
    int elementSize = luaL_checkinteger(L, 7);
    int count = luaL_checkinteger(L, 8);

    if (elementSize < 0)
        luaL_error(L, "elementSize must be non-negative");
    if (count < 0)
        luaL_error(L, "count must be non-negative");
    if (count == 0)
        return 0;
    if (srcStride < elementSize || dstStride < elementSize)
        luaL_error(L, "stride must be >= elementSize");

    int64_t srcEnd =
        (int64_t)srcOffset + (int64_t)(count - 1) * srcStride + elementSize;
    if (srcOffset < 0 || srcEnd > (int64_t)srcLen)
        luaL_error(L, "buffer access out of bounds");

    int64_t dstEnd =
        (int64_t)dstOffset + (int64_t)(count - 1) * dstStride + elementSize;
    if (dstOffset < 0 || dstEnd > (int64_t)dstLen)
        luaL_error(L, "buffer access out of bounds");

    char* d = (char*)dst + dstOffset;
    const char* s = (const char*)src + srcOffset;

    for (int i = 0; i < count; i++)
    {
        memcpy(d + (int64_t)i * dstStride,
               s + (int64_t)i * srcStride,
               (size_t)elementSize);
    }

    return 0;
}

// --------------------------------------------------------------------------
// buffer.convert — bulk format conversion with optional stride + SIMD
// --------------------------------------------------------------------------

enum BufFormat
{
    BufFormat_f16,
    BufFormat_f32,
    BufFormat_u8,
    BufFormat_u8norm,
    BufFormat_i8norm,
    BufFormat_u16,
    BufFormat_u16norm,
    BufFormat_i16norm,
    BufFormat_u32,
};

static BufFormat parseFormat(lua_State* L, int arg)
{
    const char* s = luaL_checkstring(L, arg);
    if (strcmp(s, "f16") == 0)
        return BufFormat_f16;
    if (strcmp(s, "f32") == 0)
        return BufFormat_f32;
    if (strcmp(s, "u8") == 0)
        return BufFormat_u8;
    if (strcmp(s, "u8norm") == 0)
        return BufFormat_u8norm;
    if (strcmp(s, "i8norm") == 0)
        return BufFormat_i8norm;
    if (strcmp(s, "u16") == 0)
        return BufFormat_u16;
    if (strcmp(s, "u16norm") == 0)
        return BufFormat_u16norm;
    if (strcmp(s, "i16norm") == 0)
        return BufFormat_i16norm;
    if (strcmp(s, "u32") == 0)
        return BufFormat_u32;
    luaL_error(L, "unknown buffer format '%s'", s);
    return BufFormat_f32;
}

static int formatByteSize(BufFormat fmt)
{
    switch (fmt)
    {
        case BufFormat_f16:
        case BufFormat_u16:
        case BufFormat_u16norm:
        case BufFormat_i16norm:
            return 2;
        case BufFormat_f32:
        case BufFormat_u32:
            return 4;
        case BufFormat_u8:
        case BufFormat_u8norm:
        case BufFormat_i8norm:
            return 1;
    }
    return 0;
}

static double readElement(const char* ptr, BufFormat fmt)
{
    switch (fmt)
    {
        case BufFormat_f16:
        {
            uint16_t h;
            memcpy(&h, ptr, 2);
            return (double)halfToFloat(h);
        }
        case BufFormat_f32:
        {
            float f;
            memcpy(&f, ptr, 4);
            return (double)f;
        }
        case BufFormat_u8:
            return (double)(uint8_t)*ptr;
        case BufFormat_u8norm:
            return (double)(uint8_t)*ptr / 255.0;
        case BufFormat_i8norm:
        {
            int8_t v;
            memcpy(&v, ptr, 1);
            double d = (double)v / 127.0;
            return d < -1.0 ? -1.0 : d;
        }
        case BufFormat_u16:
        {
            uint16_t v;
            memcpy(&v, ptr, 2);
            return (double)v;
        }
        case BufFormat_u16norm:
        {
            uint16_t v;
            memcpy(&v, ptr, 2);
            return (double)v / 65535.0;
        }
        case BufFormat_i16norm:
        {
            int16_t v;
            memcpy(&v, ptr, 2);
            double d = (double)v / 32767.0;
            return d < -1.0 ? -1.0 : d;
        }
        case BufFormat_u32:
        {
            uint32_t v;
            memcpy(&v, ptr, 4);
            return (double)v;
        }
    }
    return 0.0;
}

static void writeElement(char* ptr, BufFormat fmt, double value)
{
    switch (fmt)
    {
        case BufFormat_f16:
        {
            uint16_t h = floatToHalf((float)value);
            memcpy(ptr, &h, 2);
            break;
        }
        case BufFormat_f32:
        {
            float f = (float)value;
            memcpy(ptr, &f, 4);
            break;
        }
        case BufFormat_u8:
        {
            *ptr = (char)(uint8_t)(unsigned int)value;
            break;
        }
        case BufFormat_u8norm:
        {
            double c = value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
            *ptr = (char)(uint8_t)(c * 255.0 + 0.5);
            break;
        }
        case BufFormat_i8norm:
        {
            double c = value < -1.0 ? -1.0 : (value > 1.0 ? 1.0 : value);
            int8_t v = (int8_t)(c * 127.0 + (c >= 0 ? 0.5 : -0.5));
            memcpy(ptr, &v, 1);
            break;
        }
        case BufFormat_u16:
        {
            uint16_t v = (uint16_t)(unsigned int)value;
            memcpy(ptr, &v, 2);
            break;
        }
        case BufFormat_u16norm:
        {
            double c = value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
            uint16_t v = (uint16_t)(c * 65535.0 + 0.5);
            memcpy(ptr, &v, 2);
            break;
        }
        case BufFormat_i16norm:
        {
            double c = value < -1.0 ? -1.0 : (value > 1.0 ? 1.0 : value);
            int16_t v = (int16_t)(c * 32767.0 + (c >= 0 ? 0.5 : -0.5));
            memcpy(ptr, &v, 2);
            break;
        }
        case BufFormat_u32:
        {
            uint32_t v = (uint32_t)value;
            memcpy(ptr, &v, 4);
            break;
        }
    }
}

// buffer.convert(dst, dstOff, dstFmt, src, srcOff, srcFmt, count,
//                components?, dstStride?, srcStride?)
static int buffer_convert(lua_State* L)
{
    size_t dstLen = 0, srcLen = 0;
    void* dst = luaL_checkbuffer(L, 1, &dstLen);
    int dstOffset = luaL_checkinteger(L, 2);
    BufFormat dstFmt = parseFormat(L, 3);
    void* src = luaL_checkbuffer(L, 4, &srcLen);
    int srcOffset = luaL_checkinteger(L, 5);
    BufFormat srcFmt = parseFormat(L, 6);
    int count = luaL_checkinteger(L, 7);
    int components = luaL_optinteger(L, 8, 1);

    int srcElemSize = formatByteSize(srcFmt);
    int dstElemSize = formatByteSize(dstFmt);

    int dstStride = luaL_optinteger(L, 9, components * dstElemSize);
    int srcStride = luaL_optinteger(L, 10, components * srcElemSize);

    if (count < 0)
        luaL_error(L, "count must be non-negative");
    if (components < 1)
        luaL_error(L, "components must be at least 1");
    if (count == 0)
        return 0;
    if (srcStride < 0 || dstStride < 0)
        luaL_error(L, "stride must be non-negative");

    int64_t srcSpan = (int64_t)components * srcElemSize;
    int64_t dstSpan = (int64_t)components * dstElemSize;

    if (srcStride > 0 && srcStride < srcSpan)
        luaL_error(L, "srcStride must be >= components * element size");
    if (dstStride > 0 && dstStride < dstSpan)
        luaL_error(L, "dstStride must be >= components * element size");

    // Bounds check: last element at offset + (count-1)*stride + span
    {
        int64_t srcEnd =
            (int64_t)srcOffset + (int64_t)(count - 1) * srcStride + srcSpan;
        if (srcOffset < 0 || srcEnd > (int64_t)srcLen)
            luaL_error(L, "buffer access out of bounds");

        int64_t dstEnd =
            (int64_t)dstOffset + (int64_t)(count - 1) * dstStride + dstSpan;
        if (dstOffset < 0 || dstEnd > (int64_t)dstLen)
            luaL_error(L, "buffer access out of bounds");
    }

    const char* s = (const char*)src + srcOffset;
    char* d = (char*)dst + dstOffset;

    bool packed = (srcStride == (int)srcSpan && dstStride == (int)dstSpan);
    int totalScalars = count * components;

    // Fast path: same format, packed → memcpy
    if (srcFmt == dstFmt && packed)
    {
        memcpy(d, s, (size_t)totalScalars * srcElemSize);
        return 0;
    }

    // SIMD fast path: packed f32 → f16
    if (srcFmt == BufFormat_f32 && dstFmt == BufFormat_f16 && packed)
    {
        convertF32ToF16Bulk((const float*)s, (uint16_t*)d, totalScalars);
        return 0;
    }

    // SIMD fast path: packed f16 → f32
    if (srcFmt == BufFormat_f16 && dstFmt == BufFormat_f32 && packed)
    {
        convertF16ToF32Bulk((const uint16_t*)s, (float*)d, totalScalars);
        return 0;
    }

    // General path: per-element with stride support
    for (int i = 0; i < count; i++)
    {
        const char* sp = s + (int64_t)i * srcStride;
        char* dp = d + (int64_t)i * dstStride;
        for (int c = 0; c < components; c++)
        {
            double val = readElement(sp + c * srcElemSize, srcFmt);
            writeElement(dp + c * dstElemSize, dstFmt, val);
        }
    }
    return 0;
}

// --------------------------------------------------------------------------
// Registration
// --------------------------------------------------------------------------

extern "C" int luaopen_rive_buffer_ext(lua_State* L)
{
    lua_getglobal(L, "buffer");

    lua_pushcfunction(L, buffer_readf16, "buffer.readf16");
    lua_setfield(L, -2, "readf16");

    lua_pushcfunction(L, buffer_writef16, "buffer.writef16");
    lua_setfield(L, -2, "writef16");

    lua_pushcfunction(L, buffer_stridedcopy, "buffer.stridedcopy");
    lua_setfield(L, -2, "stridedcopy");

    lua_pushcfunction(L, buffer_convert, "buffer.convert");
    lua_setfield(L, -2, "convert");

    lua_pop(L, 1);
    return 0;
}

#endif // WITH_RIVE_SCRIPTING

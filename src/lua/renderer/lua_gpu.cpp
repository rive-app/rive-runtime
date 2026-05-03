#ifdef WITH_RIVE_SCRIPTING
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
#include "lualib.h"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_context_impl.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/assets/shader_asset.hpp"
#include "rive/shapes/paint/color.hpp"

#include <algorithm>
#include <cstring>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <vector>

using namespace rive;
using namespace rive::ore;

// ============================================================================
// String-to-enum helpers
// ============================================================================

static BufferUsage lua_tobufferusage(lua_State* L, int idx)
{
    const char* s = luaL_checkstring(L, idx);
    if (strcmp(s, "vertex") == 0)
        return BufferUsage::vertex;
    if (strcmp(s, "index") == 0)
        return BufferUsage::index;
    if (strcmp(s, "uniform") == 0)
        return BufferUsage::uniform;
    luaL_error(L, "invalid BufferUsage: %s", s);
    return BufferUsage::vertex;
}

static TextureFormat lua_totextureformat(lua_State* L, const char* s)
{
    if (strcmp(s, "r8unorm") == 0)
        return TextureFormat::r8unorm;
    if (strcmp(s, "rg8unorm") == 0)
        return TextureFormat::rg8unorm;
    if (strcmp(s, "rgba8unorm") == 0)
        return TextureFormat::rgba8unorm;
    if (strcmp(s, "bgra8unorm") == 0)
        return TextureFormat::bgra8unorm;
    if (strcmp(s, "rgba16float") == 0)
        return TextureFormat::rgba16float;
    if (strcmp(s, "rg16float") == 0)
        return TextureFormat::rg16float;
    if (strcmp(s, "r16float") == 0)
        return TextureFormat::r16float;
    if (strcmp(s, "rgba32float") == 0)
        return TextureFormat::rgba32float;
    if (strcmp(s, "rgb10a2unorm") == 0)
        return TextureFormat::rgb10a2unorm;
    if (strcmp(s, "rg11b10ufloat") == 0)
        return TextureFormat::r11g11b10float;
    if (strcmp(s, "depth16unorm") == 0)
        return TextureFormat::depth16unorm;
    if (strcmp(s, "depth24plus-stencil8") == 0)
        return TextureFormat::depth24plusStencil8;
    if (strcmp(s, "depth32float") == 0)
        return TextureFormat::depth32float;
    if (strcmp(s, "depth32float-stencil8") == 0)
        return TextureFormat::depth32floatStencil8;
    if (strcmp(s, "bc1-rgba-unorm") == 0)
        return TextureFormat::bc1unorm;
    if (strcmp(s, "bc3-rgba-unorm") == 0)
        return TextureFormat::bc3unorm;
    if (strcmp(s, "bc7-rgba-unorm") == 0)
        return TextureFormat::bc7unorm;
    if (strcmp(s, "etc2-rgb8unorm") == 0)
        return TextureFormat::etc2rgb8;
    if (strcmp(s, "etc2-rgba8unorm") == 0)
        return TextureFormat::etc2rgba8;
    if (strcmp(s, "astc-4x4-unorm") == 0)
        return TextureFormat::astc4x4;
    if (strcmp(s, "astc-6x6-unorm") == 0)
        return TextureFormat::astc6x6;
    if (strcmp(s, "astc-8x8-unorm") == 0)
        return TextureFormat::astc8x8;
    luaL_error(L, "invalid TextureFormat: %s", s);
    return TextureFormat::rgba8unorm;
}

static const char* lua_totextureformatstring(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::r8unorm:
            return "r8unorm";
        case TextureFormat::rg8unorm:
            return "rg8unorm";
        case TextureFormat::rgba8unorm:
            return "rgba8unorm";
        case TextureFormat::bgra8unorm:
            return "bgra8unorm";
        case TextureFormat::rgba16float:
            return "rgba16float";
        case TextureFormat::rg16float:
            return "rg16float";
        case TextureFormat::r16float:
            return "r16float";
        case TextureFormat::rgba32float:
            return "rgba32float";
        case TextureFormat::rgb10a2unorm:
            return "rgb10a2unorm";
        case TextureFormat::r11g11b10float:
            return "rg11b10ufloat";
        case TextureFormat::depth16unorm:
            return "depth16unorm";
        case TextureFormat::depth24plusStencil8:
            return "depth24plus-stencil8";
        case TextureFormat::depth32float:
            return "depth32float";
        case TextureFormat::depth32floatStencil8:
            return "depth32float-stencil8";
        default:
            return "rgba8unorm";
    }
}

static TextureType lua_totexturetype(lua_State* L, const char* s)
{
    if (strcmp(s, "2d") == 0)
        return TextureType::texture2D;
    if (strcmp(s, "cube") == 0)
        return TextureType::cube;
    if (strcmp(s, "3d") == 0)
        return TextureType::texture3D;
    if (strcmp(s, "2d-array") == 0)
        return TextureType::array2D;
    luaL_error(L, "invalid TextureType: %s", s);
    return TextureType::texture2D;
}

static CompareFunction lua_tocompare(lua_State* L, const char* s)
{
    if (strcmp(s, "never") == 0)
        return CompareFunction::never;
    if (strcmp(s, "less") == 0)
        return CompareFunction::less;
    if (strcmp(s, "equal") == 0)
        return CompareFunction::equal;
    if (strcmp(s, "less-equal") == 0)
        return CompareFunction::lessEqual;
    if (strcmp(s, "greater") == 0)
        return CompareFunction::greater;
    if (strcmp(s, "not-equal") == 0)
        return CompareFunction::notEqual;
    if (strcmp(s, "greater-equal") == 0)
        return CompareFunction::greaterEqual;
    if (strcmp(s, "always") == 0)
        return CompareFunction::always;
    luaL_error(L, "invalid CompareFunction: %s", s);
    return CompareFunction::none;
}

static Filter lua_tofilter(lua_State* L, const char* s)
{
    if (strcmp(s, "nearest") == 0)
        return Filter::nearest;
    if (strcmp(s, "linear") == 0)
        return Filter::linear;
    luaL_error(L, "invalid Filter: %s", s);
    return Filter::nearest;
}

static WrapMode lua_towrapmode(lua_State* L, const char* s)
{
    if (strcmp(s, "repeat") == 0)
        return WrapMode::repeat;
    if (strcmp(s, "mirror-repeat") == 0)
        return WrapMode::mirrorRepeat;
    if (strcmp(s, "clamp-to-edge") == 0)
        return WrapMode::clampToEdge;
    luaL_error(L, "invalid WrapMode: %s", s);
    return WrapMode::clampToEdge;
}

static VertexFormat lua_tovertexformat(lua_State* L, const char* s)
{
    if (strcmp(s, "float32") == 0)
        return VertexFormat::float1;
    if (strcmp(s, "float32x2") == 0)
        return VertexFormat::float2;
    if (strcmp(s, "float32x3") == 0)
        return VertexFormat::float3;
    if (strcmp(s, "float32x4") == 0)
        return VertexFormat::float4;
    if (strcmp(s, "uint8x4") == 0)
        return VertexFormat::uint8x4;
    if (strcmp(s, "unorm8x4") == 0)
        return VertexFormat::unorm8x4;
    if (strcmp(s, "snorm8x4") == 0)
        return VertexFormat::snorm8x4;
    if (strcmp(s, "float16x2") == 0)
        return VertexFormat::float16x2;
    if (strcmp(s, "float16x4") == 0)
        return VertexFormat::float16x4;
    luaL_error(L, "invalid VertexFormat: %s", s);
    return VertexFormat::float4;
}

static CullMode lua_tocullmode(lua_State* L, const char* s)
{
    if (strcmp(s, "none") == 0)
        return CullMode::none;
    if (strcmp(s, "front") == 0)
        return CullMode::front;
    if (strcmp(s, "back") == 0)
        return CullMode::back;
    luaL_error(L, "invalid CullMode: %s", s);
    return CullMode::none;
}

static PrimitiveTopology lua_totopology(lua_State* L, const char* s)
{
    if (strcmp(s, "triangle-list") == 0)
        return PrimitiveTopology::triangleList;
    if (strcmp(s, "triangle-strip") == 0)
        return PrimitiveTopology::triangleStrip;
    if (strcmp(s, "line-list") == 0)
        return PrimitiveTopology::lineList;
    if (strcmp(s, "line-strip") == 0)
        return PrimitiveTopology::lineStrip;
    if (strcmp(s, "point-list") == 0)
        return PrimitiveTopology::pointList;
    luaL_error(L, "invalid PrimitiveTopology: %s", s);
    return PrimitiveTopology::triangleList;
}

static BlendFactor lua_toblendfactor(lua_State* L, const char* s)
{
    if (strcmp(s, "zero") == 0)
        return BlendFactor::zero;
    if (strcmp(s, "one") == 0)
        return BlendFactor::one;
    if (strcmp(s, "src") == 0)
        return BlendFactor::srcColor;
    if (strcmp(s, "one-minus-src") == 0)
        return BlendFactor::oneMinusSrcColor;
    if (strcmp(s, "src-alpha") == 0)
        return BlendFactor::srcAlpha;
    if (strcmp(s, "one-minus-src-alpha") == 0)
        return BlendFactor::oneMinusSrcAlpha;
    if (strcmp(s, "dst") == 0)
        return BlendFactor::dstColor;
    if (strcmp(s, "one-minus-dst") == 0)
        return BlendFactor::oneMinusDstColor;
    if (strcmp(s, "dst-alpha") == 0)
        return BlendFactor::dstAlpha;
    if (strcmp(s, "one-minus-dst-alpha") == 0)
        return BlendFactor::oneMinusDstAlpha;
    if (strcmp(s, "src-alpha-saturated") == 0)
        return BlendFactor::srcAlphaSaturated;
    if (strcmp(s, "constant") == 0)
        return BlendFactor::blendColor;
    if (strcmp(s, "one-minus-constant") == 0)
        return BlendFactor::oneMinusBlendColor;
    luaL_error(L, "invalid BlendFactor: %s", s);
    return BlendFactor::one;
}

static BlendOp lua_toblendop(lua_State* L, const char* s)
{
    if (strcmp(s, "add") == 0)
        return BlendOp::add;
    if (strcmp(s, "subtract") == 0)
        return BlendOp::subtract;
    if (strcmp(s, "reverse-subtract") == 0)
        return BlendOp::reverseSubtract;
    if (strcmp(s, "min") == 0)
        return BlendOp::min;
    if (strcmp(s, "max") == 0)
        return BlendOp::max;
    luaL_error(L, "invalid BlendOp: %s", s);
    return BlendOp::add;
}

static FaceWinding lua_towinding(lua_State* L, const char* s)
{
    if (strcmp(s, "cw") == 0)
        return FaceWinding::clockwise;
    if (strcmp(s, "ccw") == 0)
        return FaceWinding::counterClockwise;
    luaL_error(L, "invalid FaceWinding: %s", s);
    return FaceWinding::counterClockwise;
}

static StencilOp lua_tostencilop(lua_State* L, const char* s)
{
    if (strcmp(s, "keep") == 0)
        return StencilOp::keep;
    if (strcmp(s, "zero") == 0)
        return StencilOp::zero;
    if (strcmp(s, "replace") == 0)
        return StencilOp::replace;
    if (strcmp(s, "increment-clamp") == 0)
        return StencilOp::incrementClamp;
    if (strcmp(s, "decrement-clamp") == 0)
        return StencilOp::decrementClamp;
    if (strcmp(s, "invert") == 0)
        return StencilOp::invert;
    if (strcmp(s, "increment-wrap") == 0)
        return StencilOp::incrementWrap;
    if (strcmp(s, "decrement-wrap") == 0)
        return StencilOp::decrementWrap;
    luaL_error(L, "invalid StencilOp: %s", s);
    return StencilOp::keep;
}

// Parse a color writemask string. Each of "r"/"g"/"b"/"a" present in the
// string enables that channel. Recognized special values: "" or "none"
// disables all; "all" / "rgba" enables all (also the default if the
// field is absent). Order doesn't matter.
static ColorWriteMask lua_towritemask(lua_State* L, const char* s)
{
    if (s == nullptr || strcmp(s, "all") == 0 || strcmp(s, "rgba") == 0)
        return ColorWriteMask::all;
    if (s[0] == '\0' || strcmp(s, "none") == 0)
        return ColorWriteMask::none;
    ColorWriteMask out = ColorWriteMask::none;
    for (const char* p = s; *p; ++p)
    {
        switch (*p)
        {
            case 'r':
            case 'R':
                out = out | ColorWriteMask::red;
                break;
            case 'g':
            case 'G':
                out = out | ColorWriteMask::green;
                break;
            case 'b':
            case 'B':
                out = out | ColorWriteMask::blue;
                break;
            case 'a':
            case 'A':
                out = out | ColorWriteMask::alpha;
                break;
            default:
                luaL_error(L,
                           "invalid ColorWriteMask: '%s' "
                           "(expected r/g/b/a chars or 'all'/'none')",
                           s);
        }
    }
    return out;
}

// Helper to get a string field from a table at idx, returns nullptr if
// the field doesn't exist.
static const char* lua_getoptionalstringfield(lua_State* L,
                                              int idx,
                                              const char* field)
{
    lua_getfield(L, idx, field);
    const char* s = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
    lua_pop(L, 1);
    return s;
}

// Parse a stencil-face state table:
//   { compare="...", failOp="...", depthFailOp="...", passOp="..." }
// Defined after `lua_getoptionalstringfield` because it uses that helper —
// Android NDK clang's `-Werror` rejects forward use of a `static` function
// that hasn't been declared yet.
static void lua_tostencilface(lua_State* L, int idx, StencilFaceState* face)
{
    if (!lua_istable(L, idx))
        return;
    const char* s = lua_getoptionalstringfield(L, idx, "compare");
    if (s)
        face->compare = lua_tocompare(L, s);
    s = lua_getoptionalstringfield(L, idx, "failOp");
    if (s)
        face->failOp = lua_tostencilop(L, s);
    s = lua_getoptionalstringfield(L, idx, "depthFailOp");
    if (s)
        face->depthFailOp = lua_tostencilop(L, s);
    s = lua_getoptionalstringfield(L, idx, "passOp");
    if (s)
        face->passOp = lua_tostencilop(L, s);
}

static double lua_getoptionalnumberfield(lua_State* L,
                                         int idx,
                                         const char* field,
                                         double def)
{
    lua_getfield(L, idx, field);
    double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : def;
    lua_pop(L, 1);
    return v;
}

static bool lua_getoptionalboolfield(lua_State* L,
                                     int idx,
                                     const char* field,
                                     bool def)
{
    lua_getfield(L, idx, field);
    bool v = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : def;
    lua_pop(L, 1);
    return v;
}

// ============================================================================
// Debug tracing
// ============================================================================

// ============================================================================
// Shader (ore::ShaderModule)
// ============================================================================

// Retrieves the ore GPU context for the current VM from its ScriptingContext.
static Context* getOreContext(lua_State* L)
{
    return static_cast<Context*>(
        static_cast<ScriptingContext*>(lua_getthreaddata(L))->oreContext());
}

/// Returns the ShaderTarget byte that matches the active platform/backend.
/// Mirrors the platform selection logic in wgslCompileForBackend().
static uint8_t currentShaderTarget()
{
#if defined(__APPLE__)
    return 2; // MSL
#elif defined(_WIN32)
    return 3; // HLSL SM5, compiled to DXBC at runtime (D3D11/D3D12)
#else
    return 1; // GLSL ES3 (Android / Linux OpenGL / Web via wagyu)
#endif
}

/// Try to create ShaderModules from a raw RSTB blob.
/// For GLSL targets, the blob may contain two sources separated by a null
/// byte (vertex then fragment). Populates both module and fragmentModule on
/// the ScriptedShader. Returns true on success.
/// Copy texture-sampler pairs from a ShaderAsset into both
/// vertex and fragment ShaderModules of a ScriptedShader.
static void propagatePairs(const ShaderAsset& asset, ScriptedShader* out)
{
    auto pairs = asset.textureSamplerPairs();
    if (pairs.empty())
        return;
    std::vector<ShaderModule::TextureSamplerPair> vec;
    vec.reserve(pairs.size());
    for (size_t i = 0; i < pairs.size(); i++)
    {
        vec.push_back({pairs[i].texGroup,
                       pairs[i].texBinding,
                       pairs[i].sampGroup,
                       pairs[i].sampBinding});
    }
    if (out->module)
        out->module->m_textureSamplerPairs = vec;
    if (out->fragmentModule)
        out->fragmentModule->m_textureSamplerPairs = vec;
}

static bool makeShaderFromRstb(Context* oreCtx,
                               const uint8_t* data,
                               uint32_t len,
                               ScriptedShader* out)
{
    if (oreCtx == nullptr || data == nullptr || len == 0 || out == nullptr)
        return false;

    // ShaderAsset::decode expects a SignedContentHeader envelope
    // (shared with ScriptAsset: `[flags:1][sig:64?][inner]`). The workspace
    // hands us raw RSTB bytes, so prepend an unsigned envelope (flags=0,
    // no signature) to produce the canonical input format.
    ShaderAsset asset;
    SimpleArray<uint8_t> bytes(static_cast<size_t>(len) + 1);
    bytes[0] = 0x00; // flags: unsigned, version 0
    memcpy(bytes.data() + 1, data, len);
    if (!asset.decode(bytes, nullptr))
        return false;

    uint8_t target = currentShaderTarget();
    auto blob = asset.findShader(0, target);
    if (blob.empty())
        return false;

    const char* blobData = reinterpret_cast<const char*>(blob.data());
    uint32_t blobSize = static_cast<uint32_t>(blob.size());

    // Binding-map sidecar (mandatory) + GL fixup sidecars (only present
    // for the GLSL source target). RFC §14.4: every shipped shader carries
    // a sidecar paired with its source variant.
    auto bindingMapTargetFor = [](uint8_t t) -> uint8_t {
        switch (t)
        {
            case 0:
                return 16;
            case 1:
                return 11;
            case 2:
                return 10;
            case 3:
                return 12;
            case 5:
                return 13;
            default:
                return 255;
        }
    };
    uint8_t bmTarget = bindingMapTargetFor(target);
    auto bindingMapBlob = (bmTarget == 255) ? Span<const uint8_t>{}
                                            : asset.findShader(0, bmTarget);
    const uint8_t* bindingMapBytes =
        bindingMapBlob.empty() ? nullptr : bindingMapBlob.data();
    uint32_t bindingMapSize = static_cast<uint32_t>(bindingMapBlob.size());
    auto vsGLFixupBlob =
        (target == 1) ? asset.findShader(0, 14) : Span<const uint8_t>{};
    auto fsGLFixupBlob =
        (target == 1) ? asset.findShader(0, 15) : Span<const uint8_t>{};
    const uint8_t* vsGLFixupBytes =
        vsGLFixupBlob.empty() ? nullptr : vsGLFixupBlob.data();
    uint32_t vsGLFixupSize = static_cast<uint32_t>(vsGLFixupBlob.size());
    const uint8_t* fsGLFixupBytes =
        fsGLFixupBlob.empty() ? nullptr : fsGLFixupBlob.data();
    uint32_t fsGLFixupSize = static_cast<uint32_t>(fsGLFixupBlob.size());

    // HLSL SM5 via SPIRV-Cross (D3D11/D3D12 on Windows).
    // Blob format: "vsEntry\0fsEntry\0vsHLSL\0fsHLSL"
    // Compiled to DXBC at runtime via D3DCompile in ensureD3DShaders().
    if (target == 3)
    {
        // Parse: vsEntry\0
        const char* vsEntry = blobData;
        const char* vsEnd =
            static_cast<const char*>(memchr(vsEntry, '\0', blobSize));
        if (!vsEnd)
            return false;
        // Parse: fsEntry\0
        const char* fsEntry = vsEnd + 1;
        uint32_t remaining =
            blobSize - static_cast<uint32_t>(fsEntry - blobData);
        const char* fsEnd =
            static_cast<const char*>(memchr(fsEntry, '\0', remaining));
        if (!fsEnd)
            return false;
        // Parse: vsHLSL\0
        const char* vsHLSL = fsEnd + 1;
        remaining = blobSize - static_cast<uint32_t>(vsHLSL - blobData);
        const char* vsHLSLEnd =
            static_cast<const char*>(memchr(vsHLSL, '\0', remaining));
        if (!vsHLSLEnd)
            return false;
        // Parse: fsHLSL (rest of blob)
        const char* fsHLSL = vsHLSLEnd + 1;
        uint32_t fsHLSLSize =
            blobSize - static_cast<uint32_t>(fsHLSL - blobData);

        // Create vertex module with HLSL source for runtime D3DCompile.
        {
            ShaderModuleDesc vtxDesc;
            vtxDesc.stage = ShaderStage::vertex;
            vtxDesc.hlslSource = vsHLSL;
            vtxDesc.hlslSourceSize = static_cast<uint32_t>(vsHLSLEnd - vsHLSL);
            vtxDesc.hlslEntryPoint = vsEntry;
            vtxDesc.bindingMapBytes = bindingMapBytes;
            vtxDesc.bindingMapSize = bindingMapSize;
            out->module = oreCtx->makeShaderModule(vtxDesc);
        }
        // Create fragment module with HLSL source for runtime D3DCompile.
        {
            ShaderModuleDesc fragDesc;
            fragDesc.stage = ShaderStage::fragment;
            fragDesc.hlslSource = fsHLSL;
            fragDesc.hlslSourceSize = fsHLSLSize;
            fragDesc.hlslEntryPoint = fsEntry;
            fragDesc.bindingMapBytes = bindingMapBytes;
            fragDesc.bindingMapSize = bindingMapSize;
            out->fragmentModule = oreCtx->makeShaderModule(fragDesc);
        }
        if (out->module)
            propagatePairs(asset, out);
        return out->module != nullptr;
    }

    // Check for a null separator inside the blob — indicates split
    // vertex/fragment GLSL (one GLSL source per entry point).
    const char* sep =
        static_cast<const char*>(memchr(blobData, '\0', blobSize - 1));

    if (sep != nullptr)
    {
        // Split blob: vertex before separator, fragment after.
        uint32_t vtxSize = static_cast<uint32_t>(sep - blobData);
        const char* fragData = sep + 1;
        uint32_t fragSize = blobSize - vtxSize - 1;

        ShaderModuleDesc vtxDesc;
        vtxDesc.code = blobData;
        vtxDesc.codeSize = vtxSize;
        vtxDesc.stage = ShaderStage::vertex;
        vtxDesc.bindingMapBytes = bindingMapBytes;
        vtxDesc.bindingMapSize = bindingMapSize;
        vtxDesc.glFixupBytes = vsGLFixupBytes;
        vtxDesc.glFixupSize = vsGLFixupSize;
        out->module = oreCtx->makeShaderModule(vtxDesc);

        ShaderModuleDesc fragDesc;
        fragDesc.code = fragData;
        fragDesc.codeSize = fragSize;
        fragDesc.stage = ShaderStage::fragment;
        fragDesc.bindingMapBytes = bindingMapBytes;
        fragDesc.bindingMapSize = bindingMapSize;
        fragDesc.glFixupBytes = fsGLFixupBytes;
        fragDesc.glFixupSize = fsGLFixupSize;
        out->fragmentModule = oreCtx->makeShaderModule(fragDesc);

        if (out->module && out->fragmentModule)
            propagatePairs(asset, out);
        return out->module != nullptr && out->fragmentModule != nullptr;
    }

    // Single combined blob (Metal, WGSL, etc.)
    ShaderModuleDesc desc;
    desc.code = blobData;
    desc.codeSize = blobSize;
    desc.bindingMapBytes = bindingMapBytes;
    desc.bindingMapSize = bindingMapSize;
    out->module = oreCtx->makeShaderModule(desc);
    if (out->module)
        propagatePairs(asset, out);
    return out->module != nullptr;
}

/// Public wrapper: build a ScriptedShader from raw RSTB bytes. Used by the
/// runtime `loadShader` fallback path for legacy .riv files where WGSL
/// shaders were packed into ScriptAsset containers (pre-ShaderAsset).
bool lua_gpu_make_shader_from_rstb(ScriptedShader* out,
                                   ScriptingContext* context,
                                   const uint8_t* data,
                                   uint32_t len)
{
    Context* oreCtx = static_cast<Context*>(
        context != nullptr ? context->oreContext() : nullptr);
    return makeShaderFromRstb(oreCtx, data, len, out);
}

/// Look up a shader by name: first check the per-VM RSTB blobs on the
/// ScriptingContext (editor path, compiled during requestVM), then try the
/// ShaderAsset from file->assets() (runtime .riv path).
/// Populates both vertex and fragment modules on the ScriptedShader.
/// Returns true on success.
bool lua_gpu_load_shader_by_name(ScriptedShader* out,
                                 ScriptingContext* context,
                                 const char* name,
                                 ShaderAsset* fileAsset)
{
    Context* oreCtx = static_cast<Context*>(
        context != nullptr ? context->oreContext() : nullptr);

#ifdef WITH_RIVE_TOOLS
    // Editor path: RSTB blobs compiled from WGSL during requestVM, stored
    // per-VM on the ScriptingContext.
    if (context != nullptr)
    {
        const auto* rstb = context->findShaderRstb(name);
        if (rstb != nullptr)
        {
            return makeShaderFromRstb(oreCtx,
                                      rstb->data(),
                                      static_cast<uint32_t>(rstb->size()),
                                      out);
        }
    }
#endif
    // Runtime path: ShaderAsset from the .riv file's asset list.
    // Use the same RSTB decoding as the editor path so that GLSL
    // vertex/fragment splitting and texture-sampler pair propagation
    // are handled correctly on all backends.
    if (fileAsset != nullptr && oreCtx != nullptr)
    {
        uint8_t target = currentShaderTarget();
        auto blob = fileAsset->findShader(0, target);
        if (!blob.empty())
        {
            const char* blobData = reinterpret_cast<const char*>(blob.data());
            uint32_t blobSize = static_cast<uint32_t>(blob.size());

            // Sidecar lookup (mirrors makeShaderFromRstb above). The
            // sidecar is mandatory; absence here would assert in
            // `makeShaderModule::applyBindingMapFromDesc`.
            auto bmTarget = [](uint8_t t) -> uint8_t {
                switch (t)
                {
                    case 0:
                        return 16;
                    case 1:
                        return 11;
                    case 2:
                        return 10;
                    case 3:
                        return 12;
                    case 5:
                        return 13;
                    default:
                        return 255;
                }
            }(target);
            auto bmBlob = (bmTarget == 255)
                              ? Span<const uint8_t>{}
                              : fileAsset->findShader(0, bmTarget);
            const uint8_t* bindingMapBytes =
                bmBlob.empty() ? nullptr : bmBlob.data();
            uint32_t bindingMapSize = static_cast<uint32_t>(bmBlob.size());
            auto vsFixupBlob = (target == 1) ? fileAsset->findShader(0, 14)
                                             : Span<const uint8_t>{};
            auto fsFixupBlob = (target == 1) ? fileAsset->findShader(0, 15)
                                             : Span<const uint8_t>{};
            const uint8_t* vsFixupBytes =
                vsFixupBlob.empty() ? nullptr : vsFixupBlob.data();
            uint32_t vsFixupSize = static_cast<uint32_t>(vsFixupBlob.size());
            const uint8_t* fsFixupBytes =
                fsFixupBlob.empty() ? nullptr : fsFixupBlob.data();
            uint32_t fsFixupSize = static_cast<uint32_t>(fsFixupBlob.size());

            // HLSL SM5 (target 3): split vsEntry/fsEntry/vsHLSL/fsHLSL.
            if (target == 3)
            {
                const char* vsEntry = blobData;
                const char* vsEnd =
                    static_cast<const char*>(memchr(vsEntry, '\0', blobSize));
                if (!vsEnd)
                    return false;
                const char* fsEntry = vsEnd + 1;
                uint32_t remaining =
                    blobSize - static_cast<uint32_t>(fsEntry - blobData);
                const char* fsEnd =
                    static_cast<const char*>(memchr(fsEntry, '\0', remaining));
                if (!fsEnd)
                    return false;
                const char* vsHLSL = fsEnd + 1;
                remaining = blobSize - static_cast<uint32_t>(vsHLSL - blobData);
                const char* vsHLSLEnd =
                    static_cast<const char*>(memchr(vsHLSL, '\0', remaining));
                if (!vsHLSLEnd)
                    return false;
                const char* fsHLSL = vsHLSLEnd + 1;
                uint32_t fsHLSLSize =
                    blobSize - static_cast<uint32_t>(fsHLSL - blobData);

                ShaderModuleDesc vtxDesc;
                vtxDesc.stage = ShaderStage::vertex;
                vtxDesc.hlslSource = vsHLSL;
                vtxDesc.hlslSourceSize =
                    static_cast<uint32_t>(vsHLSLEnd - vsHLSL);
                vtxDesc.hlslEntryPoint = vsEntry;
                vtxDesc.bindingMapBytes = bindingMapBytes;
                vtxDesc.bindingMapSize = bindingMapSize;
                out->module = oreCtx->makeShaderModule(vtxDesc);

                ShaderModuleDesc fragDesc;
                fragDesc.stage = ShaderStage::fragment;
                fragDesc.hlslSource = fsHLSL;
                fragDesc.hlslSourceSize = fsHLSLSize;
                fragDesc.hlslEntryPoint = fsEntry;
                fragDesc.bindingMapBytes = bindingMapBytes;
                fragDesc.bindingMapSize = bindingMapSize;
                out->fragmentModule = oreCtx->makeShaderModule(fragDesc);
            }
            // GLSL ES3 (target 1): split vertex\0fragment.
            else if (target == 1 && blobSize > 1)
            {
                const char* sep = static_cast<const char*>(
                    memchr(blobData, '\0', blobSize - 1));
                if (sep)
                {
                    uint32_t vtxSize = static_cast<uint32_t>(sep - blobData);
                    const char* fragData = sep + 1;
                    uint32_t fragSize = blobSize - vtxSize - 1;

                    ShaderModuleDesc vtxDesc;
                    vtxDesc.code = blobData;
                    vtxDesc.codeSize = vtxSize;
                    vtxDesc.stage = ShaderStage::vertex;
                    vtxDesc.bindingMapBytes = bindingMapBytes;
                    vtxDesc.bindingMapSize = bindingMapSize;
                    vtxDesc.glFixupBytes = vsFixupBytes;
                    vtxDesc.glFixupSize = vsFixupSize;
                    out->module = oreCtx->makeShaderModule(vtxDesc);

                    ShaderModuleDesc fragDesc;
                    fragDesc.code = fragData;
                    fragDesc.codeSize = fragSize;
                    fragDesc.stage = ShaderStage::fragment;
                    fragDesc.bindingMapBytes = bindingMapBytes;
                    fragDesc.bindingMapSize = bindingMapSize;
                    fragDesc.glFixupBytes = fsFixupBytes;
                    fragDesc.glFixupSize = fsFixupSize;
                    out->fragmentModule = oreCtx->makeShaderModule(fragDesc);
                }
                else
                {
                    // No separator — single module (unusual).
                    ShaderModuleDesc desc;
                    desc.code = blobData;
                    desc.codeSize = blobSize;
                    desc.bindingMapBytes = bindingMapBytes;
                    desc.bindingMapSize = bindingMapSize;
                    out->module = oreCtx->makeShaderModule(desc);
                }
            }
            // Single combined blob (MSL, WGSL, SPIR-V).
            else
            {
                ShaderModuleDesc desc;
                desc.code = blobData;
                desc.codeSize = blobSize;
                desc.bindingMapBytes = bindingMapBytes;
                desc.bindingMapSize = bindingMapSize;
                out->module = oreCtx->makeShaderModule(desc);
            }

            // Propagate texture-sampler pairs from the RSTB.
            if (out->module)
            {
                auto pairs = fileAsset->textureSamplerPairs();
                if (!pairs.empty())
                {
                    std::vector<ShaderModule::TextureSamplerPair> vec;
                    vec.reserve(pairs.size());
                    for (size_t i = 0; i < pairs.size(); i++)
                    {
                        vec.push_back({pairs[i].texGroup,
                                       pairs[i].texBinding,
                                       pairs[i].sampGroup,
                                       pairs[i].sampBinding});
                    }
                    out->module->m_textureSamplerPairs = vec;
                    if (out->fragmentModule)
                        out->fragmentModule->m_textureSamplerPairs = vec;
                }
            }

            return out->module != nullptr;
        }
    }

    return false;
}

int lua_gpu_push_shader_by_name(lua_State* L, const char* name)
{
    auto* context = static_cast<ScriptingContext*>(lua_getthreaddata(L));
    auto* scripted = lua_newrive<ScriptedShader>(L);
    if (!lua_gpu_load_shader_by_name(scripted, context, name, nullptr))
    {
        lua_pop(L, 1);
        return 0;
    }
    return 1;
}

// Optional WGSL→native compiler, set by the host via
// luaopen_rive_gpu_set_wgsl_compiler(). When set, shader_construct() routes
// WGSL source through this function before passing to makeShaderModule().
// Returns true and fills |outSource| on success; returns false and fills
// |outError| on failure. Not set in Flutter runtime builds (shaders arrive
// as pre-compiled backend source from ShaderAsset).
#ifdef WITH_RIVE_TOOLS
static bool (*g_wgslCompilerFn)(const char* wgsl,
                                uint32_t len,
                                std::string* outSource,
                                std::string* outError) = nullptr;

void luaopen_rive_gpu_set_wgsl_compiler(
    bool (*fn)(const char*, uint32_t, std::string*, std::string*))
{
    g_wgslCompilerFn = fn;
}
#endif

static int shader_construct(lua_State* L)
{
    // Argument 1 is the name of a pre-compiled WGSL shader asset that was
    // bundled with the Rive file (set via the "wgslAssetName" field in the
    // editor).  Raw WGSL source strings are NOT accepted at runtime; shaders
    // must be pre-compiled and embedded as assets.
    const char* name = luaL_checkstring(L, 1);
    if (lua_gpu_push_shader_by_name(L, name) == 0)
    {
        luaL_error(L, "Shader.new: no shader asset named '%s' found", name);
    }
    return 1;
}

// ============================================================================
// GPUBuffer
// ============================================================================

// `GPUBuffer.new(size, usage [, options])` where `options` is an optional
// table of:
//   - `data`       : Lua buffer with initial contents. Required when
//                    `immutable=true`. Length must match `size` exactly so
//                    backends can populate the GPU resource at create time.
//   - `immutable`  : when true, the buffer is GPU-only (no `:write` after
//                    creation). Required for `BufferDesc::immutable`, which
//                    backends with a USAGE_IMMUTABLE concept (D3D11) honor
//                    for static vertex/index buffers.
static int gpubuffer_construct(lua_State* L)
{
    if (getOreContext(L) == nullptr)
    {
        luaL_error(L, "GPU context not initialized");
    }

    uint32_t size = static_cast<uint32_t>(luaL_checkunsigned(L, 1));
    BufferUsage usage = lua_tobufferusage(L, 2);

    BufferDesc desc;
    desc.size = size;
    desc.usage = usage;

    // Optional `options` table with initial data + immutable flag.
    if (lua_istable(L, 3))
    {
        int optsIdx = 3;
        desc.immutable =
            lua_getoptionalboolfield(L, optsIdx, "immutable", false);

        lua_getfield(L, optsIdx, "data");
        if (!lua_isnil(L, -1))
        {
            size_t dataLen = 0;
            const void* dataPtr = lua_tobuffer(L, -1, &dataLen);
            if (dataPtr == nullptr)
            {
                luaL_error(L,
                           "GPUBuffer.new: 'data' option must be a Luau "
                           "buffer");
            }
            if (dataLen != size)
            {
                luaL_error(L,
                           "GPUBuffer.new: data length (%zu) must equal "
                           "size (%u)",
                           dataLen,
                           size);
            }
            desc.data = dataPtr;
        }
        lua_pop(L, 1);

        if (desc.immutable && desc.data == nullptr)
        {
            luaL_error(L,
                       "GPUBuffer.new: immutable=true requires 'data' "
                       "option (the GPU buffer is GPU-only after creation)");
        }
    }

    auto* ctx = getOreContext(L);
    ctx->clearLastError();
    auto buffer = ctx->makeBuffer(desc);
    if (!buffer)
    {
        if (!ctx->lastError().empty())
            luaL_error(L, "GPUBuffer.new: %s", ctx->lastError().c_str());
        luaL_error(L, "GPUBuffer.new: failed to create buffer");
    }

    auto* scripted = lua_newrive<ScriptedGPUBuffer>(L);
    scripted->buffer = std::move(buffer);
    scripted->immutable = desc.immutable;
    return 1;
}

static int gpubuffer_write(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUBuffer>(L, 1);
    if (self->immutable)
    {
        luaL_error(L,
                   "GPUBuffer:write: buffer was created with "
                   "immutable=true; its contents are fixed at construction");
    }
    // Luau buffer type
    size_t len;
    const void* data = lua_tobuffer(L, 2, &len);
    if (data == nullptr)
    {
        luaL_typeerror(L, 2, "buffer");
    }
    uint32_t offset =
        lua_isnumber(L, 3) ? static_cast<uint32_t>(lua_tonumber(L, 3)) : 0;

    if (offset + len > self->buffer->size())
    {
        luaL_error(L,
                   "GPUBuffer:write: offset(%u) + size(%u) = %u exceeds "
                   "buffer size(%u)",
                   offset,
                   (uint32_t)len,
                   (uint32_t)(offset + len),
                   self->buffer->size());
    }

    self->buffer->update(data, static_cast<uint32_t>(len), offset);
    return 0;
}

static int gpubuffer_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::write:
                return gpubuffer_write(L);
        }
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedGPUBuffer::luaName);
    return 0;
}

static int gpubuffer_index(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUBuffer>(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "size") == 0)
    {
        lua_pushnumber(L, self->buffer->size());
        return 1;
    }
    luaL_error(L, "%s is not a valid property of GPUBuffer", key);
    return 0;
}

// ============================================================================
// GPUTexture
// ============================================================================

// Validate a user-supplied sampleCount against the device's actual limit.
// Errors with a clear message rather than letting Metal/Vulkan assert-fail.
static void lua_checksamplecount(lua_State* L, uint32_t sampleCount)
{
    if (sampleCount <= 1)
        return;
    // Must be a power of two.
    if ((sampleCount & (sampleCount - 1)) != 0)
        luaL_error(L,
                   "sampleCount must be a power of two (got %u)",
                   sampleCount);
    auto* ctx = getOreContext(L);
    if (ctx)
    {
        uint32_t maxSamples = ctx->features().maxSamples;
        if (sampleCount > maxSamples)
            luaL_error(L,
                       "sampleCount %u exceeds device maximum of %u — "
                       "query context:features().maxSamples before creating "
                       "MSAA textures",
                       sampleCount,
                       maxSamples);
    }
}

static int gputexture_construct(lua_State* L)
{
    if (getOreContext(L) == nullptr)
    {
        luaL_error(L, "GPU context not initialized");
    }

    luaL_checktype(L, 1, LUA_TTABLE);
    int descIdx = 1;

    TextureDesc desc;
    desc.width = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "width", 0));
    desc.height = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "height", 0));
    if (desc.width == 0 || desc.height == 0)
    {
        luaL_error(L, "GPUTexture requires width and height");
    }

    const char* fmt = lua_getoptionalstringfield(L, descIdx, "format");
    if (fmt)
        desc.format = lua_totextureformat(L, fmt);

    const char* typ = lua_getoptionalstringfield(L, descIdx, "type");
    if (typ)
        desc.type = lua_totexturetype(L, typ);

    desc.renderTarget =
        lua_getoptionalboolfield(L, descIdx, "renderTarget", false);
    desc.sampleCount = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "sampleCount", 1));
    lua_checksamplecount(L, desc.sampleCount);
    desc.numMipmaps = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "mipmaps", 1));
    desc.depthOrArrayLayers = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "layers", 1));

    auto* ctx = getOreContext(L);
    ctx->clearLastError();
    auto texture = ctx->makeTexture(desc);
    if (!texture)
    {
        if (!ctx->lastError().empty())
            luaL_error(L, "GPUTexture.new: %s", ctx->lastError().c_str());
        luaL_error(L, "GPUTexture.new: failed to create texture");
    }

    auto* scripted = lua_newrive<ScriptedGPUTexture>(L);
    scripted->texture = std::move(texture);
    return 1;
}

static int gputexture_view(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUTexture>(L, 1);

    TextureViewDesc viewDesc;
    viewDesc.texture = self->texture.get();
    viewDesc.mipCount = self->texture->numMipmaps();
    viewDesc.layerCount = self->texture->depthOrArrayLayers();

    // Map TextureType -> TextureViewDimension
    switch (self->texture->type())
    {
        case TextureType::texture2D:
            viewDesc.dimension = TextureViewDimension::texture2D;
            break;
        case TextureType::cube:
            viewDesc.dimension = TextureViewDimension::cube;
            break;
        case TextureType::texture3D:
            viewDesc.dimension = TextureViewDimension::texture3D;
            break;
        case TextureType::array2D:
            viewDesc.dimension = TextureViewDimension::array2D;
            break;
    }

    if (lua_istable(L, 2))
    {
        const char* dim = lua_getoptionalstringfield(L, 2, "dimension");
        if (dim)
        {
            if (strcmp(dim, "2d") == 0)
                viewDesc.dimension = TextureViewDimension::texture2D;
            else if (strcmp(dim, "cube") == 0)
                viewDesc.dimension = TextureViewDimension::cube;
            else if (strcmp(dim, "3d") == 0)
                viewDesc.dimension = TextureViewDimension::texture3D;
            else if (strcmp(dim, "2d-array") == 0)
                viewDesc.dimension = TextureViewDimension::array2D;
        }
        viewDesc.baseMipLevel = static_cast<uint32_t>(
            lua_getoptionalnumberfield(L, 2, "baseMipLevel", 0));
        viewDesc.mipCount = static_cast<uint32_t>(
            lua_getoptionalnumberfield(L, 2, "mipCount", viewDesc.mipCount));
        viewDesc.baseLayer = static_cast<uint32_t>(
            lua_getoptionalnumberfield(L, 2, "baseLayer", 0));
        viewDesc.layerCount = static_cast<uint32_t>(
            lua_getoptionalnumberfield(L,
                                       2,
                                       "layerCount",
                                       viewDesc.layerCount));
    }

    auto* ctx = getOreContext(L);
    ctx->clearLastError();
    auto tv = ctx->makeTextureView(viewDesc);
    if (!tv)
    {
        if (!ctx->lastError().empty())
            luaL_error(L, "GPUTexture:view: %s", ctx->lastError().c_str());
        luaL_error(L, "GPUTexture:view: failed to create texture view");
    }

    auto* scripted = lua_newrive<ScriptedGPUTextureView>(L);
    scripted->view = std::move(tv);
    return 1;
}

static int gputexture_upload(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUTexture>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    int descIdx = 2;

    lua_getfield(L, descIdx, "data");
    size_t len;
    const void* data = lua_tobuffer(L, -1, &len);
    if (!data)
    {
        luaL_error(L, "upload requires 'data' field of type buffer");
    }
    lua_pop(L, 1);

    TextureDataDesc uploadDesc;
    uploadDesc.data = data;
    uploadDesc.width = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L,
                                   descIdx,
                                   "width",
                                   self->texture->width()));
    uploadDesc.height = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L,
                                   descIdx,
                                   "height",
                                   self->texture->height()));
    uploadDesc.depth = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "depth", 1));
    uploadDesc.x =
        static_cast<uint32_t>(lua_getoptionalnumberfield(L, descIdx, "x", 0));
    uploadDesc.y =
        static_cast<uint32_t>(lua_getoptionalnumberfield(L, descIdx, "y", 0));
    uploadDesc.z =
        static_cast<uint32_t>(lua_getoptionalnumberfield(L, descIdx, "z", 0));
    uploadDesc.mipLevel = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "mipLevel", 0));
    uploadDesc.layer = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "layer", 0));
    uploadDesc.bytesPerRow = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "bytesPerRow", 0));
    uploadDesc.rowsPerImage = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "rowsPerImage", 0));

    // Validate region/level/layer against the texture's actual dimensions —
    // out-of-range values trip backend asserts (Metal API Validation, D3D12
    // GPU hangs, Vulkan validation). Per the
    // `feedback_lua_gpu_misuse_validation` rule, surface as luaL_error.
    if (uploadDesc.mipLevel >= self->texture->numMipmaps())
    {
        luaL_error(L,
                   "upload: mipLevel %u out of range [0, %u)",
                   uploadDesc.mipLevel,
                   self->texture->numMipmaps());
    }
    if (uploadDesc.layer >= self->texture->depthOrArrayLayers())
    {
        luaL_error(L,
                   "upload: layer %u out of range [0, %u)",
                   uploadDesc.layer,
                   self->texture->depthOrArrayLayers());
    }
    // Mip-level dimensions: floor-div-by-2 per level, min 1.
    uint32_t mipW = std::max(1u, self->texture->width() >> uploadDesc.mipLevel);
    uint32_t mipH =
        std::max(1u, self->texture->height() >> uploadDesc.mipLevel);
    if (uploadDesc.x > mipW || uploadDesc.width > mipW - uploadDesc.x)
    {
        luaL_error(L,
                   "upload: x+width (%u+%u) exceeds mip %u width %u",
                   uploadDesc.x,
                   uploadDesc.width,
                   uploadDesc.mipLevel,
                   mipW);
    }
    if (uploadDesc.y > mipH || uploadDesc.height > mipH - uploadDesc.y)
    {
        luaL_error(L,
                   "upload: y+height (%u+%u) exceeds mip %u height %u",
                   uploadDesc.y,
                   uploadDesc.height,
                   uploadDesc.mipLevel,
                   mipH);
    }

    // Compute a tightly-packed bytesPerRow when the caller omits it.
    if (uploadDesc.bytesPerRow == 0)
    {
        uint32_t bpt = textureFormatBytesPerTexel(self->texture->format());
        if (bpt == 0)
        {
            luaL_error(L,
                       "upload: bytesPerRow must be provided for "
                       "block-compressed formats");
        }
        uploadDesc.bytesPerRow = uploadDesc.width * bpt;
    }

    // Default rowsPerImage to height so Metal's bytesPerImage is correct.
    if (uploadDesc.rowsPerImage == 0)
    {
        uploadDesc.rowsPerImage = uploadDesc.height;
    }

    // Validate the data buffer is large enough to cover the region.
    // GPUs read past the supplied bytes if we don't catch it here; on
    // Metal the validation layer aborts, on others it's a silent OOB.
    const uint64_t requiredBytes =
        static_cast<uint64_t>(uploadDesc.bytesPerRow) *
        uploadDesc.rowsPerImage * std::max(1u, uploadDesc.depth);
    if (len < requiredBytes)
    {
        luaL_error(L,
                   "upload: data buffer is %zu bytes but region requires "
                   "%llu (bytesPerRow=%u * rowsPerImage=%u * depth=%u)",
                   len,
                   static_cast<unsigned long long>(requiredBytes),
                   uploadDesc.bytesPerRow,
                   uploadDesc.rowsPerImage,
                   std::max(1u, uploadDesc.depth));
    }

    self->texture->upload(uploadDesc);
    return 0;
}

static int gputexture_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::view:
                return gputexture_view(L);
            case (int)LuaAtoms::upload:
                return gputexture_upload(L);
        }
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedGPUTexture::luaName);
    return 0;
}

static int gputexture_index(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUTexture>(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "width") == 0)
    {
        lua_pushnumber(L, self->texture->width());
        return 1;
    }
    if (strcmp(key, "height") == 0)
    {
        lua_pushnumber(L, self->texture->height());
        return 1;
    }
    if (strcmp(key, "format") == 0)
    {
        lua_pushstring(L, lua_totextureformatstring(self->texture->format()));
        return 1;
    }
    luaL_error(L, "%s is not a valid property of GPUTexture", key);
    return 0;
}

static int gputextureview_index(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUTextureView>(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "format") == 0)
    {
        if (self->view && self->view->texture())
            lua_pushstring(
                L,
                lua_totextureformatstring(self->view->texture()->format()));
        else
            lua_pushnil(L);
        return 1;
    }
    luaL_error(L, "%s is not a valid property of GPUTextureView", key);
    return 0;
}

// ============================================================================
// GPUSampler
// ============================================================================

static int gpusampler_construct(lua_State* L)
{
    if (getOreContext(L) == nullptr)
    {
        luaL_error(L, "GPU context not initialized");
    }

    SamplerDesc desc;

    if (lua_istable(L, 1))
    {
        int descIdx = 1;
        const char* s;

        s = lua_getoptionalstringfield(L, descIdx, "min");
        if (s)
            desc.minFilter = lua_tofilter(L, s);
        s = lua_getoptionalstringfield(L, descIdx, "mag");
        if (s)
            desc.magFilter = lua_tofilter(L, s);
        s = lua_getoptionalstringfield(L, descIdx, "mipmap");
        if (s)
            desc.mipmapFilter = lua_tofilter(L, s);
        s = lua_getoptionalstringfield(L, descIdx, "wrapU");
        if (s)
            desc.wrapU = lua_towrapmode(L, s);
        s = lua_getoptionalstringfield(L, descIdx, "wrapV");
        if (s)
            desc.wrapV = lua_towrapmode(L, s);
        s = lua_getoptionalstringfield(L, descIdx, "wrapW");
        if (s)
            desc.wrapW = lua_towrapmode(L, s);
        s = lua_getoptionalstringfield(L, descIdx, "compare");
        if (s)
            desc.compare = lua_tocompare(L, s);
        desc.minLod = static_cast<float>(
            lua_getoptionalnumberfield(L, descIdx, "minLod", 0.0));
        desc.maxLod = static_cast<float>(
            lua_getoptionalnumberfield(L, descIdx, "maxLod", 32.0));
        if (desc.minLod > desc.maxLod)
        {
            luaL_error(L,
                       "GPUSampler.new: minLod (%g) > maxLod (%g)",
                       desc.minLod,
                       desc.maxLod);
        }
        desc.maxAnisotropy = static_cast<uint32_t>(
            lua_getoptionalnumberfield(L, descIdx, "maxAnisotropy", 1));
        // WebGPU + every backend caps anisotropy at 16x and accepts only
        // power-of-two values. Reject out-of-range early so a bad sampler
        // doesn't cause a backend-specific create failure later.
        const uint32_t a = desc.maxAnisotropy;
        if (a < 1 || a > 16 || (a & (a - 1)) != 0)
        {
            luaL_error(L,
                       "GPUSampler.new: maxAnisotropy must be a power of "
                       "two in [1, 16] (got %u)",
                       a);
        }
        if (a > 1 && !getOreContext(L)->features().anisotropicFiltering)
        {
            luaL_error(L,
                       "GPUSampler.new: maxAnisotropy=%u requires "
                       "anisotropicFiltering feature, which the active "
                       "backend does not support",
                       a);
        }
    }

    auto* ctx = getOreContext(L);
    ctx->clearLastError();
    auto sampler = ctx->makeSampler(desc);
    if (!sampler)
    {
        if (!ctx->lastError().empty())
            luaL_error(L, "GPUSampler.new: %s", ctx->lastError().c_str());
        luaL_error(L, "GPUSampler.new: failed to create sampler");
    }

    auto* scripted = lua_newrive<ScriptedGPUSampler>(L);
    scripted->sampler = std::move(sampler);
    return 1;
}

// ============================================================================
// GPUBindGroup
// ============================================================================

ScriptedGPUBindGroup::~ScriptedGPUBindGroup()
{
    // If the BindGroup has a context, defer its destruction until after
    // endFrame() so the GPU is done with any command buffers referencing it.
    // The rcp<> is moved into the context's deferred queue, keeping the
    // BindGroup alive. When endFrame() clears the queue, refcount drops
    // to zero and onRefCntReachedZero() does a simple delete.
    if (bindGroup && bindGroup->context())
    {
        bindGroup->context()->deferBindGroupDestroy(std::move(bindGroup));
        return;
    }
    // No context (or null) — drop immediately; rcp destructor handles it.
}

// ============================================================================
// GPUBindGroupLayout.new — explicit BindGroupLayout (Phase E).
// ============================================================================

// Map binding-map types to layout-entry types. These mirror the GM helper
// `makeLayoutFromShader` so Lua-built layouts validate the same way as
// C++-built ones.
static BindingKind bindingKindFromResource(ResourceKind k)
{
    switch (k)
    {
        case ResourceKind::UniformBuffer:
            return BindingKind::uniformBuffer;
        case ResourceKind::StorageBufferRO:
            return BindingKind::storageBufferRO;
        case ResourceKind::StorageBufferRW:
            return BindingKind::storageBufferRW;
        case ResourceKind::SampledTexture:
            return BindingKind::sampledTexture;
        case ResourceKind::StorageTexture:
            return BindingKind::storageTexture;
        case ResourceKind::Sampler:
            return BindingKind::sampler;
        case ResourceKind::ComparisonSampler:
            return BindingKind::comparisonSampler;
    }
    return BindingKind::uniformBuffer;
}

static TextureViewDimension viewDimFromBindingMap(TextureViewDim d)
{
    switch (d)
    {
        case TextureViewDim::Cube:
            return TextureViewDimension::cube;
        case TextureViewDim::CubeArray:
            return TextureViewDimension::cubeArray;
        case TextureViewDim::D3:
            return TextureViewDimension::texture3D;
        case TextureViewDim::D2Array:
            return TextureViewDimension::array2D;
        case TextureViewDim::D1:
        case TextureViewDim::D2:
        case TextureViewDim::Undefined:
            return TextureViewDimension::texture2D;
    }
    return TextureViewDimension::texture2D;
}

static BindGroupLayoutEntry::SampleType sampleTypeFromBindingMap(
    TextureSampleType s)
{
    switch (s)
    {
        case TextureSampleType::UnfilterableFloat:
            return BindGroupLayoutEntry::SampleType::floatUnfilterable;
        case TextureSampleType::Depth:
            return BindGroupLayoutEntry::SampleType::depth;
        case TextureSampleType::Sint:
            return BindGroupLayoutEntry::SampleType::sint;
        case TextureSampleType::Uint:
            return BindGroupLayoutEntry::SampleType::uint;
        case TextureSampleType::Float:
        case TextureSampleType::Undefined:
            return BindGroupLayoutEntry::SampleType::floatFilterable;
    }
    return BindGroupLayoutEntry::SampleType::floatFilterable;
}

// Walk a shader's BindingMap for the given group, populating layout entries
// with kind / visibility / texture metadata / native slots — the same path the
// GM helper takes. Returns the entry count actually filled.
static uint32_t populateEntriesFromShader(BindGroupLayoutEntry* entries,
                                          uint32_t maxEntries,
                                          const ore::ShaderModule* shader,
                                          uint32_t group,
                                          const uint32_t* dynamicUBOBindings,
                                          uint32_t dynamicUBOCount)
{
    if (shader == nullptr)
        return 0;
    auto isDynamic = [&](uint32_t binding) -> bool {
        for (uint32_t i = 0; i < dynamicUBOCount; ++i)
            if (dynamicUBOBindings[i] == binding)
                return true;
        return false;
    };
    const BindingMap& bm = shader->m_bindingMap;
    uint32_t n = 0;
    for (size_t i = 0; i < bm.size() && n < maxEntries; ++i)
    {
        const BindingMap::Entry& e = bm.at(i);
        if (e.group != group)
            continue;
        BindGroupLayoutEntry& out = entries[n++];
        out.binding = e.binding;
        out.kind = bindingKindFromResource(e.kind);
        uint8_t vis = 0;
        if (e.stageMask & BindingMap::kStageVertex)
            vis |= StageVisibility::kVertex;
        if (e.stageMask & BindingMap::kStageFragment)
            vis |= StageVisibility::kFragment;
        if (e.stageMask & BindingMap::kStageCompute)
            vis |= StageVisibility::kCompute;
        out.visibility.mask = vis;
        out.hasDynamicOffset =
            (out.kind == BindingKind::uniformBuffer && isDynamic(e.binding));
        out.textureViewDim = viewDimFromBindingMap(e.textureViewDim);
        out.textureSampleType = sampleTypeFromBindingMap(e.textureSampleType);
        out.textureMultisampled = e.textureMultisampled;
        const uint16_t vs =
            e.backendSlot[static_cast<size_t>(BindingMap::Stage::VS)];
        const uint16_t fs =
            e.backendSlot[static_cast<size_t>(BindingMap::Stage::FS)];
        out.nativeSlotVS = (vs == BindingMap::kAbsent)
                               ? BindGroupLayoutEntry::kNativeSlotAbsent
                               : static_cast<uint32_t>(vs);
        out.nativeSlotFS = (fs == BindingMap::kAbsent)
                               ? BindGroupLayoutEntry::kNativeSlotAbsent
                               : static_cast<uint32_t>(fs);
    }
    return n;
}

static int gpubindgrouplayout_construct(lua_State* L)
{
    Context* oreCtx = getOreContext(L);
    if (oreCtx == nullptr)
        luaL_error(L, "GPU context not initialized");

    luaL_checktype(L, 1, LUA_TTABLE);
    int descIdx = 1;

    BindGroupLayoutDesc desc;
    desc.groupIndex = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "groupIndex", 0));
    if (desc.groupIndex >= ore::kMaxBindGroups)
        luaL_error(L,
                   "GPUBindGroupLayout.new: groupIndex must be in [0, %u)",
                   ore::kMaxBindGroups);

    // Resolve the source shader. Layouts must always derive from a shader
    // module — Lua callers cannot meaningfully fill in `nativeSlotVS/FS`
    // without the binding map, and getting kind/visibility wrong would
    // fail validation anyway.
    lua_getfield(L, descIdx, "shader");
    auto* scripted = lua_torive<ScriptedShader>(L, -1);
    lua_pop(L, 1);
    if (scripted == nullptr || !scripted->module)
        luaL_error(L,
                   "GPUBindGroupLayout.new: 'shader' must be a Shader with "
                   "a loaded module");

    static constexpr int kMaxEntries = 16;
    BindGroupLayoutEntry entries[kMaxEntries]{};

    // Optional `dynamicUBOs`: array of WGSL @binding values whose UBO
    // entries should set hasDynamicOffset.
    uint32_t dynUBOs[kMaxEntries] = {};
    uint32_t dynUBOCount = 0;
    lua_getfield(L, descIdx, "dynamicUBOs");
    if (lua_istable(L, -1))
    {
        int dynTbl = lua_gettop(L);
        int dynN = (int)lua_objlen(L, dynTbl);
        for (int i = 0; i < dynN && dynUBOCount < kMaxEntries; ++i)
        {
            lua_rawgeti(L, dynTbl, i + 1);
            if (lua_isnumber(L, -1))
                dynUBOs[dynUBOCount++] =
                    static_cast<uint32_t>(lua_tonumber(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1); // dynamicUBOs

    // Vertex module carries the merged binding map for both stages —
    // the same module the GM helper walks.
    uint32_t entryCount = populateEntriesFromShader(entries,
                                                    kMaxEntries,
                                                    scripted->vertexMod(),
                                                    desc.groupIndex,
                                                    dynUBOs,
                                                    dynUBOCount);

    desc.entries = entries;
    desc.entryCount = entryCount;

    rcp<BindGroupLayout> layout = oreCtx->makeBindGroupLayout(desc);
    if (!layout)
    {
        const std::string& err = oreCtx->lastError();
        oreCtx->clearLastError();
        luaL_error(L,
                   "GPUBindGroupLayout.new: %s",
                   err.empty() ? "failed" : err.c_str());
    }

    auto* w = lua_newrive<ScriptedGPUBindGroupLayout>(L);
    w->layout = std::move(layout);
    return 1;
}

static int gpubindgroup_construct(lua_State* L)
{
    Context* oreCtx = getOreContext(L);
    if (oreCtx == nullptr)
    {
        luaL_error(L, "GPU context not initialized");
    }

    luaL_checktype(L, 1, LUA_TTABLE);
    int descIdx = 1;

    BindGroupDesc desc;

    // layout (required) — replaces the legacy `pipeline` field. The
    // BindGroup is built against a BindGroupLayout (Phase E); the
    // layout's groupIndex determines which @group(N) the BindGroup
    // binds to.
    lua_getfield(L, descIdx, "layout");
    auto* layoutScripted = lua_torive<ScriptedGPUBindGroupLayout>(L, -1);
    if (layoutScripted == nullptr)
    {
        luaL_error(L,
                   "GPUBindGroup.new: 'layout' field is required and must be "
                   "a GPUBindGroupLayout");
    }
    desc.layout = layoutScripted->layout.get();
    lua_pop(L, 1);

    // Parse UBO entries
    static constexpr int MAX_ENTRIES = 8;
    BindGroupDesc::UBOEntry uboEntries[MAX_ENTRIES];
    uint32_t uboCount = 0;

    lua_getfield(L, descIdx, "ubos");
    if (lua_istable(L, -1))
    {
        int tbl = lua_gettop(L);
        int n = (int)lua_objlen(L, tbl);
        for (int i = 0; i < n && i < MAX_ENTRIES; i++)
        {
            lua_rawgeti(L, tbl, i + 1);
            int entry = lua_gettop(L);

            uboEntries[uboCount].slot = static_cast<uint32_t>(
                lua_getoptionalnumberfield(L, entry, "slot", 0));
            if (uboEntries[uboCount].slot > 7)
            {
                luaL_error(L,
                           "GPUBindGroup.new: UBO slot must be 0-7 (got %u)",
                           uboEntries[uboCount].slot);
            }

            lua_getfield(L, entry, "buffer");
            auto* bufScripted = lua_torive<ScriptedGPUBuffer>(L, -1);
            uboEntries[uboCount].buffer = bufScripted->buffer.get();
            lua_pop(L, 1);

            uboEntries[uboCount].offset = static_cast<uint32_t>(
                lua_getoptionalnumberfield(L, entry, "offset", 0));
            uboEntries[uboCount].size = static_cast<uint32_t>(
                lua_getoptionalnumberfield(L, entry, "size", 0));

            lua_pop(L, 1); // entry table
            uboCount++;
        }
    }
    lua_pop(L, 1);
    desc.ubos = uboEntries;
    desc.uboCount = uboCount;

    // Parse texture entries
    BindGroupDesc::TexEntry texEntries[MAX_ENTRIES];
    uint32_t texCount = 0;

    lua_getfield(L, descIdx, "textures");
    if (lua_istable(L, -1))
    {
        int tbl = lua_gettop(L);
        int n = (int)lua_objlen(L, tbl);
        for (int i = 0; i < n && i < MAX_ENTRIES; i++)
        {
            lua_rawgeti(L, tbl, i + 1);
            int entry = lua_gettop(L);

            texEntries[texCount].slot = static_cast<uint32_t>(
                lua_getoptionalnumberfield(L, entry, "slot", 0));
            if (texEntries[texCount].slot > 7)
            {
                luaL_error(
                    L,
                    "GPUBindGroup.new: texture slot must be 0-7 (got %u)",
                    texEntries[texCount].slot);
            }

            lua_getfield(L, entry, "view");
            auto* tv = lua_torive<ScriptedGPUTextureView>(L, -1);
            texEntries[texCount].view = tv->view.get();
            lua_pop(L, 1);

            lua_pop(L, 1); // entry table
            texCount++;
        }
    }
    lua_pop(L, 1);
    desc.textures = texEntries;
    desc.textureCount = texCount;

    // Parse sampler entries
    BindGroupDesc::SampEntry sampEntries[MAX_ENTRIES];
    uint32_t sampCount = 0;

    lua_getfield(L, descIdx, "samplers");
    if (lua_istable(L, -1))
    {
        int tbl = lua_gettop(L);
        int n = (int)lua_objlen(L, tbl);
        for (int i = 0; i < n && i < MAX_ENTRIES; i++)
        {
            lua_rawgeti(L, tbl, i + 1);
            int entry = lua_gettop(L);

            sampEntries[sampCount].slot = static_cast<uint32_t>(
                lua_getoptionalnumberfield(L, entry, "slot", 0));
            if (sampEntries[sampCount].slot > 7)
            {
                luaL_error(
                    L,
                    "GPUBindGroup.new: sampler slot must be 0-7 (got %u)",
                    sampEntries[sampCount].slot);
            }

            lua_getfield(L, entry, "sampler");
            auto* ss = lua_torive<ScriptedGPUSampler>(L, -1);
            sampEntries[sampCount].sampler = ss->sampler.get();
            lua_pop(L, 1);

            lua_pop(L, 1); // entry table
            sampCount++;
        }
    }
    lua_pop(L, 1);
    desc.samplers = sampEntries;
    desc.samplerCount = sampCount;

    oreCtx->clearLastError();
    auto bindGroup = oreCtx->makeBindGroup(desc);
    if (!bindGroup)
    {
        if (!oreCtx->lastError().empty())
            luaL_error(L, "GPUBindGroup.new: %s", oreCtx->lastError().c_str());
        luaL_error(L, "GPUBindGroup.new: failed to create bind group");
    }

    auto* scripted = lua_newrive<ScriptedGPUBindGroup>(L);
    scripted->bindGroup = std::move(bindGroup);
    return 1;
}

// ============================================================================
// GPUPipeline
// ============================================================================

static int gpupipeline_construct(lua_State* L)
{
    if (getOreContext(L) == nullptr)
    {
        luaL_error(L, "GPU context not initialized");
    }

    luaL_checktype(L, 1, LUA_TTABLE);
    int descIdx = 1;

    PipelineDesc desc;

    // vertex shader (required)
    lua_getfield(L, descIdx, "vertex");
    auto* vs = lua_torive<ScriptedShader>(L, -1);
    desc.vertexModule = vs->vertexMod();
    lua_pop(L, 1);

    // fragment shader. Three cases:
    //   * explicit `fragment` Shader → use its fragment module.
    //   * absent + at least one colorTarget → fall back to vs's fragment
    //     module (combined-shader file with both vs_main and fs_main).
    //   * absent + no colorTargets → depth-only pipeline, no fragment.
    // Case 3 resolution is deferred until after colorTargets is parsed.
    bool explicitFragment = false;
    lua_getfield(L, descIdx, "fragment");
    if (!lua_isnil(L, -1))
    {
        auto* fs = lua_torive<ScriptedShader>(L, -1);
        desc.fragmentModule = fs->fragmentMod();
        explicitFragment = true;
    }
    lua_pop(L, 1);

    // vertexLayout (required)
    lua_getfield(L, descIdx, "vertexLayout");
    luaL_checktype(L, -1, LUA_TTABLE);
    int layoutTableIdx = lua_gettop(L);
    int vbCount = (int)lua_objlen(L, layoutTableIdx);

    // Stack-allocate vertex buffer layouts and attributes
    static constexpr int MAX_VB = 8;
    static constexpr int MAX_ATTRS = 32;
    VertexBufferLayout vbLayouts[MAX_VB];
    VertexAttribute attrs[MAX_ATTRS];
    int totalAttrs = 0;

    for (int vb = 0; vb < vbCount && vb < MAX_VB; vb++)
    {
        lua_rawgeti(L, layoutTableIdx, vb + 1);
        int vbIdx = lua_gettop(L);

        vbLayouts[vb].stride = static_cast<uint32_t>(
            lua_getoptionalnumberfield(L, vbIdx, "stride", 0));

        const char* stepMode = lua_getoptionalstringfield(L, vbIdx, "stepMode");
        if (stepMode && strcmp(stepMode, "instance") == 0)
            vbLayouts[vb].stepMode = VertexStepMode::instance;
        else
            vbLayouts[vb].stepMode = VertexStepMode::vertex;

        lua_getfield(L, vbIdx, "attributes");
        int attrTableIdx = lua_gettop(L);
        int attrCount = (int)lua_objlen(L, attrTableIdx);

        vbLayouts[vb].attributes = &attrs[totalAttrs];
        vbLayouts[vb].attributeCount = attrCount;

        for (int a = 0; a < attrCount && totalAttrs < MAX_ATTRS; a++)
        {
            lua_rawgeti(L, attrTableIdx, a + 1);
            int attrIdx = lua_gettop(L);

            const char* fmt = lua_getoptionalstringfield(L, attrIdx, "format");
            if (fmt)
                attrs[totalAttrs].format = lua_tovertexformat(L, fmt);

            attrs[totalAttrs].shaderSlot = static_cast<uint32_t>(
                lua_getoptionalnumberfield(L, attrIdx, "slot", 0));
            attrs[totalAttrs].offset = static_cast<uint32_t>(
                lua_getoptionalnumberfield(L, attrIdx, "offset", 0));

            totalAttrs++;
            lua_pop(L, 1); // pop attr entry
        }
        lua_pop(L, 1); // pop attributes table
        lua_pop(L, 1); // pop vb entry
    }
    lua_pop(L, 1); // pop vertexLayout table

    // colorTargets (optional). WebGPU-shaped: omitting it means "no color
    // outputs" (depth-only pipeline, e.g. shadow map). We override the C++
    // PipelineDesc default of colorCount=1 so that scripts don't get a
    // phantom color target that mismatches a depth-only render pass.
    desc.colorCount = 0;
    lua_getfield(L, descIdx, "colorTargets");
    if (lua_istable(L, -1))
    {
        int ctTableIdx = lua_gettop(L);
        int ctCount = (int)lua_objlen(L, ctTableIdx);
        constexpr int kMaxColorTargets =
            sizeof(desc.colorTargets) / sizeof(desc.colorTargets[0]);
        if (ctCount > kMaxColorTargets)
        {
            luaL_error(L,
                       "GPUPipeline.new: colorTargets count %d exceeds "
                       "maximum of %d",
                       ctCount,
                       kMaxColorTargets);
        }
        desc.colorCount = ctCount;

        for (int ct = 0; ct < ctCount; ct++)
        {
            lua_rawgeti(L, ctTableIdx, ct + 1);
            int ctIdx = lua_gettop(L);

            const char* fmt = lua_getoptionalstringfield(L, ctIdx, "format");
            if (fmt)
                desc.colorTargets[ct].format = lua_totextureformat(L, fmt);

            // writeMask: optional. String like "rgba" / "rg" / "" / "all".
            // Default (when absent) is `ColorWriteMask::all` from PipelineDesc.
            const char* wm = lua_getoptionalstringfield(L, ctIdx, "writeMask");
            if (wm)
                desc.colorTargets[ct].writeMask = lua_towritemask(L, wm);

            lua_getfield(L, ctIdx, "blend");
            if (lua_istable(L, -1))
            {
                int blendIdx = lua_gettop(L);
                desc.colorTargets[ct].blendEnabled = true;
                const char* s;

                s = lua_getoptionalstringfield(L, blendIdx, "srcColor");
                if (s)
                    desc.colorTargets[ct].blend.srcColor =
                        lua_toblendfactor(L, s);
                s = lua_getoptionalstringfield(L, blendIdx, "dstColor");
                if (s)
                    desc.colorTargets[ct].blend.dstColor =
                        lua_toblendfactor(L, s);
                s = lua_getoptionalstringfield(L, blendIdx, "colorOp");
                if (s)
                    desc.colorTargets[ct].blend.colorOp = lua_toblendop(L, s);
                s = lua_getoptionalstringfield(L, blendIdx, "srcAlpha");
                if (s)
                    desc.colorTargets[ct].blend.srcAlpha =
                        lua_toblendfactor(L, s);
                s = lua_getoptionalstringfield(L, blendIdx, "dstAlpha");
                if (s)
                    desc.colorTargets[ct].blend.dstAlpha =
                        lua_toblendfactor(L, s);
                s = lua_getoptionalstringfield(L, blendIdx, "alphaOp");
                if (s)
                    desc.colorTargets[ct].blend.alphaOp = lua_toblendop(L, s);
            }
            lua_pop(L, 1); // pop blend
            lua_pop(L, 1); // pop color target entry
        }
    }
    lua_pop(L, 1); // pop colorTargets

    // Resolve the deferred fragment-module decision (see "fragment shader"
    // block above). With color outputs but no explicit fragment shader,
    // fall back to the vertex shader's fragment module (combined file).
    if (!explicitFragment && desc.colorCount > 0)
        desc.fragmentModule = vs->fragmentMod();

    // depthStencil (optional)
    lua_getfield(L, descIdx, "depthStencil");
    if (lua_istable(L, -1))
    {
        int dsIdx = lua_gettop(L);
        const char* fmt = lua_getoptionalstringfield(L, dsIdx, "format");
        if (fmt)
            desc.depthStencil.format = lua_totextureformat(L, fmt);
        const char* cmp = lua_getoptionalstringfield(L, dsIdx, "compare");
        if (cmp)
            desc.depthStencil.depthCompare = lua_tocompare(L, cmp);
        desc.depthStencil.depthWriteEnabled =
            lua_getoptionalboolfield(L, dsIdx, "write", false);
        desc.depthStencil.depthBias = static_cast<int32_t>(
            lua_getoptionalnumberfield(L, dsIdx, "depthBias", 0));
        desc.depthStencil.depthBiasSlopeScale = static_cast<float>(
            lua_getoptionalnumberfield(L, dsIdx, "depthBiasSlopeScale", 0));
        desc.depthStencil.depthBiasClamp = static_cast<float>(
            lua_getoptionalnumberfield(L, dsIdx, "depthBiasClamp", 0));
    }
    lua_pop(L, 1);

    // stencilFront / stencilBack (optional, both default to "always pass,
    // keep on every op"). Each is a table of compare + failOp + depthFailOp
    // + passOp strings.
    lua_getfield(L, descIdx, "stencilFront");
    lua_tostencilface(L, lua_gettop(L), &desc.stencilFront);
    lua_pop(L, 1);
    lua_getfield(L, descIdx, "stencilBack");
    lua_tostencilface(L, lua_gettop(L), &desc.stencilBack);
    lua_pop(L, 1);

    // stencilReadMask / stencilWriteMask (optional, default 0xFF).
    desc.stencilReadMask = static_cast<uint8_t>(
        lua_getoptionalnumberfield(L, descIdx, "stencilReadMask", 0xFF));
    desc.stencilWriteMask = static_cast<uint8_t>(
        lua_getoptionalnumberfield(L, descIdx, "stencilWriteMask", 0xFF));

    // bindGroupLayouts (optional). When absent, derive layouts per
    // @group(N) from the shader's binding map (WebGPU's `layout: 'auto'`).
    // Supply explicitly to share one layout across multiple pipelines.
    BindGroupLayout* layoutPtrs[ore::kMaxBindGroups] = {};
    uint32_t layoutCount = 0;
    std::vector<rcp<BindGroupLayout>> autoLayouts;
    lua_getfield(L, descIdx, "bindGroupLayouts");
    bool explicitLayouts = lua_istable(L, -1);
    if (explicitLayouts)
    {
        int blglIdx = lua_gettop(L);
        int n = (int)lua_objlen(L, blglIdx);
        if (n > static_cast<int>(ore::kMaxBindGroups))
        {
            luaL_error(L,
                       "GPUPipeline.new: bindGroupLayouts count %d exceeds "
                       "maximum of %u",
                       n,
                       ore::kMaxBindGroups);
        }
        for (int i = 0; i < n; ++i)
        {
            lua_rawgeti(L, blglIdx, i + 1);
            auto* l = lua_torive<ScriptedGPUBindGroupLayout>(L, -1);
            layoutPtrs[i] = l ? l->layout.get() : nullptr;
            lua_pop(L, 1);
        }
        layoutCount = static_cast<uint32_t>(n);
    }
    lua_pop(L, 1);
    if (!explicitLayouts)
    {
        // Auto path: scan the binding map for unique groups, build a
        // layout per group. Sparse-group shaders are supported (e.g.
        // group 0 + group 2 → autoLayouts[1] is null/empty).
        const BindingMap& bm = vs->vertexMod()->m_bindingMap;
        uint32_t maxGroup = 0;
        bool seen[ore::kMaxBindGroups] = {};
        for (size_t i = 0; i < bm.size(); ++i)
        {
            uint32_t g = bm.at(i).group;
            if (g >= ore::kMaxBindGroups)
                continue;
            seen[g] = true;
            if (g + 1 > maxGroup)
                maxGroup = g + 1;
        }
        autoLayouts.resize(maxGroup);
        static constexpr int kMaxEntries = 16;
        for (uint32_t g = 0; g < maxGroup; ++g)
        {
            if (!seen[g])
                continue;
            BindGroupLayoutEntry entries[kMaxEntries]{};
            uint32_t n = populateEntriesFromShader(entries,
                                                   kMaxEntries,
                                                   vs->vertexMod(),
                                                   g,
                                                   nullptr,
                                                   0);
            BindGroupLayoutDesc lDesc;
            lDesc.groupIndex = g;
            lDesc.entries = entries;
            lDesc.entryCount = n;
            autoLayouts[g] = getOreContext(L)->makeBindGroupLayout(lDesc);
            layoutPtrs[g] = autoLayouts[g].get();
        }
        layoutCount = maxGroup;
    }
    desc.bindGroupLayouts = layoutPtrs;
    desc.bindGroupLayoutCount = layoutCount;

    // cullMode (optional)
    const char* cull = lua_getoptionalstringfield(L, descIdx, "cullMode");
    if (cull)
        desc.cullMode = lua_tocullmode(L, cull);

    // winding (optional, default counterClockwise)
    const char* wind = lua_getoptionalstringfield(L, descIdx, "winding");
    if (wind)
        desc.winding = lua_towinding(L, wind);

    // topology (optional)
    const char* topo = lua_getoptionalstringfield(L, descIdx, "topology");
    if (topo)
        desc.topology = lua_totopology(L, topo);

    // sampleCount (optional, default 1)
    uint32_t pipelineSampleCount = static_cast<uint32_t>(
        lua_getoptionalnumberfield(L, descIdx, "sampleCount", 1));
    desc.sampleCount = pipelineSampleCount;

    // Allocate the ScriptedGPUPipeline first so we can deep-copy the vertex
    // layout data into it. Pipeline shallow-copies PipelineDesc, so the
    // vertex buffer layout pointers must outlive the Pipeline.
    auto* scripted = lua_newrive<ScriptedGPUPipeline>(L);

    // Deep-copy layouts and attributes into a single byte buffer:
    //   [ VertexBufferLayout[vbCount] ][ VertexAttribute[totalAttrs] ]
    size_t layoutBytes = sizeof(VertexBufferLayout) * vbCount;
    size_t attrBytes = sizeof(VertexAttribute) * totalAttrs;
    scripted->ownedVertexLayoutData.resize(layoutBytes + attrBytes);

    auto* ownedLayouts = reinterpret_cast<VertexBufferLayout*>(
        scripted->ownedVertexLayoutData.data());
    auto* ownedAttrs = reinterpret_cast<VertexAttribute*>(
        scripted->ownedVertexLayoutData.data() + layoutBytes);

    memcpy(ownedLayouts, vbLayouts, layoutBytes);
    memcpy(ownedAttrs, attrs, attrBytes);

    // Patch each layout's attributes pointer to point into the owned copy.
    int attrOffset = 0;
    for (int vb = 0; vb < vbCount; vb++)
    {
        ownedLayouts[vb].attributes = ownedAttrs + attrOffset;
        attrOffset += ownedLayouts[vb].attributeCount;
    }

    desc.vertexBuffers = ownedLayouts;
    desc.vertexBufferCount = vbCount;

    std::string pipelineError;
    auto pipeline = getOreContext(L)->makePipeline(desc, &pipelineError);
    if (!pipeline)
    {
        if (pipelineError.empty())
            luaL_error(L, "GPUPipeline.new: failed to create pipeline");
        else
            luaL_error(L, "GPUPipeline.new: %s", pipelineError.c_str());
    }

    scripted->pipeline = std::move(pipeline);
    scripted->sampleCount = pipelineSampleCount;
    scripted->autoBindGroupLayouts = std::move(autoLayouts);
    return 1;
}

// pipeline:getBindGroupLayout(groupIndex) — returns an auto-derived layout
// (WebGPU's pipeline.getBindGroupLayout). Errors when explicit layouts were
// supplied — share the explicit one directly instead.
static int gpupipeline_getbindgrouplayout(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUPipeline>(L, 1);
    uint32_t group = static_cast<uint32_t>(luaL_checkunsigned(L, 2));
    if (self->autoBindGroupLayouts.empty())
        luaL_error(L,
                   "getBindGroupLayout: pipeline was built with explicit "
                   "bindGroupLayouts; reuse the layout you supplied");
    if (group >= self->autoBindGroupLayouts.size() ||
        !self->autoBindGroupLayouts[group])
        luaL_error(L,
                   "getBindGroupLayout: group %u not present in shader",
                   group);
    auto* w = lua_newrive<ScriptedGPUBindGroupLayout>(L);
    w->layout = self->autoBindGroupLayouts[group];
    return 1;
}

static int gpupipeline_namecall(lua_State* L)
{
    int atom;
    const char* method = lua_namecallatom(L, &atom);
    if (method == nullptr)
        luaL_error(L, "GPUPipeline: no method specified");
    if (strcmp(method, "getBindGroupLayout") == 0)
        return gpupipeline_getbindgrouplayout(L);
    luaL_error(L, "GPUPipeline: unknown method '%s'", method);
    return 0;
}

// ============================================================================
// GPURenderPass
// ============================================================================

static void validate_render_pass(lua_State* L, ScriptedGPURenderPass* self)
{
    if (self->m_finished || self->pass == nullptr)
    {
        luaL_error(L, "render pass expired — call finish() only once");
    }
}

static void validate_pipeline_set(lua_State* L, ScriptedGPURenderPass* self)
{
    if (!self->m_pipelineSet)
    {
        luaL_error(L,
                   "setPipeline must be called before draw/setVertexBuffer/"
                   "setBindGroup");
    }
}

static int gpurenderpass_setpipeline(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    auto* pipeline = lua_torive<ScriptedGPUPipeline>(L, 2);
    if (pipeline->sampleCount != self->sampleCount)
    {
        luaL_error(L,
                   "pipeline sampleCount (%u) does not match render pass "
                   "sampleCount (%u) — recreate the pipeline with matching "
                   "sampleCount",
                   pipeline->sampleCount,
                   self->sampleCount);
    }
    // Capture any attachment-compat failure from ore::RenderPass::setPipeline.
    // Without this, checkPipelineCompat failures silently no-op the Metal
    // encoder state and subsequent draws crash inside the driver.
    Context* oreCtx = getOreContext(L);
    if (oreCtx)
        oreCtx->clearLastError();
    self->pass->setPipeline(pipeline->pipeline.get());
    if (oreCtx && !oreCtx->lastError().empty())
    {
        luaL_error(L, "setPipeline: %s", oreCtx->lastError().c_str());
    }
    self->m_pipelineSet = true;
    return 0;
}

static int gpurenderpass_setvertexbuffer(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    // Note: WebGPU / Metal / Vulkan / D3D11 all permit `setVertexBuffer`
    // before `setPipeline` — vertex-buffer state is layered onto whatever
    // pipeline is current at draw time. Don't gate on `m_pipelineSet`.
    uint32_t slot = static_cast<uint32_t>(luaL_checkunsigned(L, 2));
    if (slot > 7)
    {
        luaL_error(L, "setVertexBuffer: slot must be 0-7 (got %u)", slot);
    }
    auto* buffer = lua_torive<ScriptedGPUBuffer>(L, 3);
    self->pass->setVertexBuffer(slot, buffer->buffer.get());
    return 0;
}

static int gpurenderpass_setindexbuffer(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    auto* buffer = lua_torive<ScriptedGPUBuffer>(L, 2);
    IndexFormat fmt = IndexFormat::uint16;
    if (lua_isstring(L, 3))
    {
        const char* s = lua_tostring(L, 3);
        if (strcmp(s, "uint32") == 0)
            fmt = IndexFormat::uint32;
    }
    self->pass->setIndexBuffer(buffer->buffer.get(), fmt);
    return 0;
}

static int gpurenderpass_setbindgroup(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    uint32_t groupIndex = static_cast<uint32_t>(luaL_checkunsigned(L, 2));
    if (groupIndex >= ore::kMaxBindGroups)
    {
        luaL_error(L,
                   "setBindGroup: groupIndex must be in [0, %u) (got %u)",
                   ore::kMaxBindGroups,
                   groupIndex);
    }
    auto* bg = lua_torive<ScriptedGPUBindGroup>(L, 3);

    // Parse optional dynamic offsets array.
    //
    // WebGPU contract: `dynamicOffsets[i]` corresponds to the i-th dynamic
    // entry in the BindGroupLayout (= ascending `@binding` per RFC §3.6).
    // The count must equal the BindGroup's dynamic-offset count exactly,
    // and each value must be aligned to `minUniformBufferOffsetAlignment`
    // (256 bytes — D3D11.1's `firstConstant` requirement, D3D12's CBV
    // alignment, Vulkan's adapter-default minimum). Validate here so a
    // misuse error surfaces on the Lua call site instead of a backend
    // assert / silent misbind.
    constexpr uint32_t kDynamicOffsetAlign = 256;
    uint32_t dynamicOffsets[8] = {};
    uint32_t dynamicOffsetCount = 0;
    if (lua_istable(L, 4))
    {
        int tbl = 4;
        int n = (int)lua_objlen(L, tbl);
        if (n > 8)
        {
            luaL_error(L,
                       "setBindGroup: dynamicOffsets count %d exceeds "
                       "maximum of 8",
                       n);
        }
        for (int i = 0; i < n; i++)
        {
            lua_rawgeti(L, tbl, i + 1);
            uint32_t off = static_cast<uint32_t>(lua_tonumber(L, -1));
            lua_pop(L, 1);
            if ((off % kDynamicOffsetAlign) != 0)
            {
                luaL_error(L,
                           "setBindGroup: dynamicOffsets[%d] = %u is not a "
                           "multiple of %u (alignment requirement)",
                           i,
                           off,
                           kDynamicOffsetAlign);
            }
            dynamicOffsets[dynamicOffsetCount++] = off;
        }
    }

    if (bg->bindGroup &&
        dynamicOffsetCount != bg->bindGroup->dynamicOffsetCount())
    {
        luaL_error(L,
                   "setBindGroup: dynamicOffsets count %u does not match the "
                   "BindGroup's declared dynamic UBO count %u",
                   dynamicOffsetCount,
                   bg->bindGroup->dynamicOffsetCount());
    }

    self->pass->setBindGroup(groupIndex,
                             bg->bindGroup.get(),
                             dynamicOffsetCount > 0 ? dynamicOffsets : nullptr,
                             dynamicOffsetCount);
    return 0;
}

static int gpurenderpass_setviewport(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_checknumber(L, 4));
    float h = static_cast<float>(luaL_checknumber(L, 5));
    self->pass->setViewport(x, y, w, h);
    return 0;
}

static int gpurenderpass_setscissorrect(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    uint32_t x = static_cast<uint32_t>(luaL_checkunsigned(L, 2));
    uint32_t y = static_cast<uint32_t>(luaL_checkunsigned(L, 3));
    uint32_t w = static_cast<uint32_t>(luaL_checkunsigned(L, 4));
    uint32_t h = static_cast<uint32_t>(luaL_checkunsigned(L, 5));
    self->pass->setScissorRect(x, y, w, h);
    return 0;
}

static int gpurenderpass_setstencilreference(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    uint32_t ref = static_cast<uint32_t>(luaL_checkunsigned(L, 2));
    self->pass->setStencilReference(ref);
    return 0;
}

static int gpurenderpass_setblendcolor(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    self->pass->setBlendColor(r, g, b, a);
    return 0;
}

// `pass:draw(vertexCount [, instanceCount [, firstVertex
//                       [, firstInstance]]])`
static int gpurenderpass_draw(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    validate_pipeline_set(L, self);
    uint32_t vertexCount = static_cast<uint32_t>(luaL_checkunsigned(L, 2));
    uint32_t instanceCount =
        lua_isnumber(L, 3) ? static_cast<uint32_t>(lua_tonumber(L, 3)) : 1;
    uint32_t firstVertex =
        lua_isnumber(L, 4) ? static_cast<uint32_t>(lua_tonumber(L, 4)) : 0;
    uint32_t firstInstance =
        lua_isnumber(L, 5) ? static_cast<uint32_t>(lua_tonumber(L, 5)) : 0;
    if (firstInstance > 0 && !getOreContext(L)->features().drawBaseInstance)
    {
        luaL_error(L,
                   "draw: firstInstance=%u requires the drawBaseInstance "
                   "feature, which the active backend does not support",
                   firstInstance);
    }
    self->pass->draw(vertexCount, instanceCount, firstVertex, firstInstance);
    self->drawCallCount++;
    return 0;
}

// `pass:drawIndexed(indexCount [, instanceCount [, firstIndex
//                                [, baseVertex [, firstInstance]]]])`
static int gpurenderpass_drawindexed(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    validate_pipeline_set(L, self);
    uint32_t indexCount = static_cast<uint32_t>(luaL_checkunsigned(L, 2));
    uint32_t instanceCount =
        lua_isnumber(L, 3) ? static_cast<uint32_t>(lua_tonumber(L, 3)) : 1;
    uint32_t firstIndex =
        lua_isnumber(L, 4) ? static_cast<uint32_t>(lua_tonumber(L, 4)) : 0;
    int32_t baseVertex =
        lua_isnumber(L, 5) ? static_cast<int32_t>(lua_tointeger(L, 5)) : 0;
    uint32_t firstInstance =
        lua_isnumber(L, 6) ? static_cast<uint32_t>(lua_tonumber(L, 6)) : 0;
    if (firstInstance > 0 && !getOreContext(L)->features().drawBaseInstance)
    {
        luaL_error(L,
                   "drawIndexed: firstInstance=%u requires the "
                   "drawBaseInstance feature, which the active backend "
                   "does not support",
                   firstInstance);
    }
    self->pass->drawIndexed(indexCount,
                            instanceCount,
                            firstIndex,
                            baseVertex,
                            firstInstance);
    self->drawCallCount++;
    return 0;
}

static int gpurenderpass_finish(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPURenderPass>(L, 1);
    validate_render_pass(L, self);
    self->pass->finish();
    self->m_finished = true;
    // Clear the context's active pass pointer so the next beginRenderPass
    // doesn't see a stale (already-finished) pass.
    Context* oreCtx = getOreContext(L);
    if (oreCtx && oreCtx->activeRenderPass() == self->pass)
        oreCtx->setActiveRenderPass(nullptr);
    return 0;
}

static int gpurenderpass_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::setPipeline:
                return gpurenderpass_setpipeline(L);
            case (int)LuaAtoms::setVertexBuffer:
                return gpurenderpass_setvertexbuffer(L);
            case (int)LuaAtoms::setIndexBuffer:
                return gpurenderpass_setindexbuffer(L);
            case (int)LuaAtoms::setBindGroup:
                return gpurenderpass_setbindgroup(L);
            case (int)LuaAtoms::setViewport:
                return gpurenderpass_setviewport(L);
            case (int)LuaAtoms::setScissorRect:
                return gpurenderpass_setscissorrect(L);
            case (int)LuaAtoms::setStencilReference:
                return gpurenderpass_setstencilreference(L);
            case (int)LuaAtoms::setBlendColor:
                return gpurenderpass_setblendcolor(L);
            case (int)LuaAtoms::draw:
                return gpurenderpass_draw(L);
            case (int)LuaAtoms::drawIndexed:
                return gpurenderpass_drawindexed(L);
            case (int)LuaAtoms::finish:
                return gpurenderpass_finish(L);
        }
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedGPURenderPass::luaName);
    return 0;
}

// ============================================================================
// ScriptedGPUCanvas / ScriptedCanvas — destructors
// ============================================================================

ScriptedGPUCanvas::~ScriptedGPUCanvas()
{
    delete m_activePass;
    m_activePass = nullptr;
    if (m_L != nullptr && m_imageRef != LUA_NOREF)
    {
        lua_unref(m_L, m_imageRef);
    }
}

ScriptedGPURenderPass::~ScriptedGPURenderPass()
{
    if (m_L != nullptr && m_canvasRef != LUA_NOREF)
    {
        lua_unref(m_L, m_canvasRef);
    }
}

ScriptedCanvas::~ScriptedCanvas()
{
    // Clean up any active frame renderer that was never ended
    delete m_riveRenderer;
    m_riveRenderer = nullptr;
    if (m_L != nullptr)
    {
        if (m_rendererRef != LUA_NOREF)
            lua_unref(m_L, m_rendererRef);
        if (m_imageRef != LUA_NOREF)
            lua_unref(m_L, m_imageRef);
    }
}

// ============================================================================
// GPUCanvas
// ============================================================================

static LoadOp lua_toloadop_str(const char* s)
{
    if (strcmp(s, "load") == 0)
        return LoadOp::load;
    return LoadOp::clear;
}

static StoreOp lua_tostoreop_str(const char* s)
{
    if (strcmp(s, "discard") == 0)
        return StoreOp::discard;
    return StoreOp::store;
}

static int gpucanvashandle_beginrenderpass(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUCanvas>(L, 1);
    if (!self->oreColorView)
    {
        luaL_error(L, "GPUCanvas has no color view");
    }
    if (getOreContext(L) == nullptr)
    {
        luaL_error(L, "GPU context not initialized");
    }

    RenderPassDesc passDesc{};
    passDesc.colorCount = 1;
    // Default: if the canvas has an MSAA color texture, render into it and
    // resolve to the 1× platform backing; otherwise render directly to it.
    if (self->oreMSAAColorView)
    {
        passDesc.colorAttachments[0].view = self->oreMSAAColorView.get();
        passDesc.colorAttachments[0].resolveTarget = self->oreColorView.get();
        passDesc.colorAttachments[0].storeOp = StoreOp::discard;
    }
    else
    {
        passDesc.colorAttachments[0].view = self->oreColorView.get();
    }
    passDesc.colorAttachments[0].loadOp = LoadOp::clear;

    uint32_t passSampleCount =
        self->oreMSAAColorView ? self->oreMSAAColorTexture->sampleCount() : 1;

    // Parse optional descriptor table
    if (lua_gettop(L) >= 2 && lua_istable(L, 2))
    {
        lua_getfield(L, 2, "color");
        if (lua_isnil(L, -1))
        {
            // Descriptor provided but no 'color' key — caller wants a
            // depth-only pass (e.g. shadow map).  Drop the default canvas
            // color attachment so the render pass matches pipelines that
            // have no color outputs.
            passDesc.colorCount = 0;
            // Fall through to the lua_pop(L, 1) below (don't pop here).
        }
        else if (lua_istable(L, -1))
        {
            // color is an array of attachment descriptor tables.  Each entry
            // Each entry is an attachment descriptor table.  Supported fields:
            //   view: GPUTextureView?       — overrides the default canvas view
            //   resolveTarget: GPUTextureView? — 1× resolve (for MSAA)
            //   loadOp: LoadOp?
            //   storeOp: StoreOp?           — use 'discard' on MSAA color
            //   clearColor: {r, g, b, a}?
            passDesc.colorCount = 0;
            for (int ci = 1; ci <= 4; ++ci)
            {
                lua_rawgeti(L, -1, ci);
                if (!lua_istable(L, -1))
                {
                    lua_pop(L, 1);
                    break;
                }
                int slot = passDesc.colorCount++;

                // Optional explicit TextureView override
                lua_getfield(L, -1, "view");
                if (!lua_isnil(L, -1))
                {
                    auto* tv = lua_torive<ScriptedGPUTextureView>(L, -1);
                    if (tv && tv->view)
                    {
                        passDesc.colorAttachments[slot].view = tv->view.get();
                        if (slot == 0)
                            passSampleCount =
                                tv->view->texture()->sampleCount();
                    }
                }
                lua_pop(L, 1); // view

                // Default view for slot 0 is the canvas itself; other slots
                // require an explicit view from the descriptor. This
                // mirrors the depthStencil-attachment shape below — both
                // depth and color slot 1+ surface a clear error when no
                // view is supplied, so a malformed `colorAttachments`
                // table can't silently truncate the pass to fewer
                // attachments than declared.
                if (passDesc.colorAttachments[slot].view == nullptr)
                {
                    if (slot == 0)
                    {
                        passDesc.colorAttachments[0].view =
                            self->oreColorView.get();
                    }
                    else
                    {
                        luaL_error(L,
                                   "beginRenderPass: colorAttachments[%d] "
                                   "has no `view` field — slot %d requires "
                                   "an explicit GPUTextureView (only slot 0 "
                                   "defaults to the canvas color view)",
                                   slot + 1,
                                   slot);
                    }
                }

                // resolveTarget (optional — for MSAA)
                lua_getfield(L, -1, "resolveTarget");
                if (!lua_isnil(L, -1))
                {
                    auto* tv = lua_torive<ScriptedGPUTextureView>(L, -1);
                    if (tv && tv->view)
                    {
                        // Validate: resolveTarget format must match the MSAA
                        // attachment format. Metal/Vulkan/D3D all require this;
                        // mismatches produce silent corruption (channel swaps,
                        // broken alpha). bgra8 canvas vs rgba8 MSAA is the
                        // classic footgun — catch it here.
                        auto* msaaTex = passDesc.colorAttachments[slot].view
                                            ? passDesc.colorAttachments[slot]
                                                  .view->texture()
                                            : nullptr;
                        if (msaaTex &&
                            tv->view->texture()->format() != msaaTex->format())
                        {
                            luaL_error(
                                L,
                                "beginRenderPass: resolveTarget format '%s' "
                                "does not match MSAA attachment format '%s' — "
                                "resolve requires identical formats. Use "
                                "canvas.format to "
                                "match your pipeline and textures.",
                                lua_totextureformatstring(
                                    tv->view->texture()->format()),
                                lua_totextureformatstring(msaaTex->format()));
                        }
                        passDesc.colorAttachments[slot].resolveTarget =
                            tv->view.get();
                    }
                }
                lua_pop(L, 1); // resolveTarget

                lua_getfield(L, -1, "loadOp");
                if (!lua_isnil(L, -1))
                {
                    passDesc.colorAttachments[slot].loadOp =
                        lua_toloadop_str(luaL_checkstring(L, -1));
                }
                lua_pop(L, 1); // loadOp

                lua_getfield(L, -1, "storeOp");
                if (lua_isnil(L, -1))
                {
                    lua_pop(L, 1);
                    luaL_error(L,
                               "beginRenderPass: color[%d].storeOp is required "
                               "— use 'discard' for MSAA color (after resolve) "
                               "or 'store' to keep the rendered output",
                               slot + 1);
                }
                passDesc.colorAttachments[slot].storeOp =
                    lua_tostoreop_str(luaL_checkstring(L, -1));
                lua_pop(L, 1); // storeOp

                lua_getfield(L, -1, "clearColor");
                if (lua_istable(L, -1))
                {
                    lua_rawgeti(L, -1, 1);
                    passDesc.colorAttachments[slot].clearColor.r =
                        (float)lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    lua_rawgeti(L, -1, 2);
                    passDesc.colorAttachments[slot].clearColor.g =
                        (float)lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    lua_rawgeti(L, -1, 3);
                    passDesc.colorAttachments[slot].clearColor.b =
                        (float)lua_tonumber(L, -1);
                    lua_pop(L, 1);
                    lua_rawgeti(L, -1, 4);
                    passDesc.colorAttachments[slot].clearColor.a =
                        (float)lua_tonumber(L, -1);
                    lua_pop(L, 1);
                }
                lua_pop(L, 1); // clearColor
                lua_pop(L, 1); // entry table
            }
            // If no explicit entries were found, fall back to the canvas view
            if (passDesc.colorCount == 0)
            {
                passDesc.colorCount = 1;
                passDesc.colorAttachments[0].view = self->oreColorView.get();
            }
        }
        lua_pop(L, 1); // color

        lua_getfield(L, 2, "depthStencil");
        if (lua_istable(L, -1))
        {
            // Prefer an explicit view from the descriptor; fall back to the
            // canvas's own depth view if available.
            lua_getfield(L, -1, "view");
            if (!lua_isnil(L, -1))
            {
                auto* tv = lua_torive<ScriptedGPUTextureView>(L, -1);
                if (tv && tv->view)
                    passDesc.depthStencil.view = tv->view.get();
            }
            else if (self->oreDepthView)
            {
                passDesc.depthStencil.view = self->oreDepthView.get();
            }
            lua_pop(L, 1); // view

            // Require an explicit view, matching WebGPU validation. Without
            // one, GL silently drops depth testing while Metal/TBDR implicitly
            // allocates tile memory — making the same script behave differently
            // across backends.
            if (!passDesc.depthStencil.view)
            {
                luaL_error(L,
                           "beginRenderPass: depthStencil.view is required — "
                           "pass GPUTexture:view()");
            }

            if (passDesc.depthStencil.view)
            {
                passDesc.depthStencil.depthLoadOp = LoadOp::clear;
                lua_getfield(L, -1, "depthLoadOp");
                if (!lua_isnil(L, -1))
                {
                    passDesc.depthStencil.depthLoadOp =
                        lua_toloadop_str(luaL_checkstring(L, -1));
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "depthStoreOp");
                if (lua_isnil(L, -1))
                {
                    lua_pop(L, 1);
                    luaL_error(L,
                               "beginRenderPass: depthStencil.depthStoreOp is "
                               "required — use 'discard' for transient/MSAA "
                               "depth or 'store' if you need to read it later");
                }
                passDesc.depthStencil.depthStoreOp =
                    lua_tostoreop_str(luaL_checkstring(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "depthClearValue");
                if (!lua_isnil(L, -1))
                {
                    passDesc.depthStencil.depthClearValue =
                        static_cast<float>(lua_tonumber(L, -1));
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1); // depthStencil
    }

    // Delete any previous active pass (ore::RenderPass destructor handles
    // cleanup)
    // Metal (and other backends) only allow one active encoder per command
    // buffer. If another canvas handle (or another VM sharing the same
    // oreCtx) left a pass open, finish it before opening a new encoder.
    Context* oreCtx = getOreContext(L);
    if (oreCtx->activeRenderPass() && !oreCtx->activeRenderPass()->isFinished())
    {
        oreCtx->activeRenderPass()->finish();
    }

    delete self->m_activePass;
    self->m_activePass = nullptr;

    ore::RenderPass raw = oreCtx->beginRenderPass(passDesc);
    self->m_activePass = new ore::RenderPass(std::move(raw));
    self->m_activePassLabel = passDesc.label ? passDesc.label : "";
    oreCtx->setActiveRenderPass(self->m_activePass);

    auto* rp = lua_newrive<ScriptedGPURenderPass>(L);
    rp->pass = self->m_activePass;
    rp->m_finished = false;
    rp->sampleCount = passSampleCount;
    rp->label = self->m_activePassLabel;
    rp->drawCallCount = 0;
    // Hold a Lua ref to the owning canvas so it can't be GC'd while the
    // pass userdata is reachable. Without this, `~ScriptedGPUCanvas`
    // deletes `m_activePass` (which `rp->pass` aliases) and any
    // subsequent method on the pass dereferences a freed pointer.
    rp->m_L = L;
    lua_pushvalue(L, 1); // canvas userdata is the `self` argument.
    rp->m_canvasRef = lua_ref(L, -1);
    lua_pop(L, 1);
    return 1;
}

// Recreate the underlying RenderCanvas at a new size, then re-wrap its backing
// texture for use in ORE render passes.  The handle's `.image` ref continues to
// point to the updated canvas image.
static int gpucanvashandle_resize(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUCanvas>(L, 1);
    uint32_t w = static_cast<uint32_t>(luaL_checkunsigned(L, 2));
    uint32_t h = static_cast<uint32_t>(luaL_checkunsigned(L, 3));

    if (self->renderCtx == nullptr)
    {
        luaL_error(L, "GPUCanvas: renderCtx not initialized");
    }
    auto* oreCtx = getOreContext(L);
    if (oreCtx == nullptr)
    {
        luaL_error(L, "GPUCanvas: GPU context not initialized");
    }

    auto newCanvas = self->renderCtx->makeRenderCanvas(w, h);
    if (!newCanvas)
    {
        luaL_error(L, "GPUCanvas:resize() failed to create RenderCanvas");
    }
    auto newColorView = oreCtx->wrapCanvasTexture(newCanvas.get());
    if (!newColorView)
    {
        luaL_error(L, "GPUCanvas:resize() failed to wrap canvas texture");
    }

    // Update the image ref to point to the new canvas
    if (self->m_L != nullptr && self->m_imageRef != LUA_NOREF)
    {
        lua_unref(self->m_L, self->m_imageRef);
        self->m_imageRef = LUA_NOREF;
    }
    self->canvas = std::move(newCanvas);
    self->oreColorView = std::move(newColorView);

    // Rebuild MSAA color texture if canvas was created with sampleCount > 1.
    if (self->oreMSAAColorTexture)
    {
        uint32_t sc = self->oreMSAAColorTexture->sampleCount();
        ore::TextureDesc msaaDesc;
        msaaDesc.width = w;
        msaaDesc.height = h;
        msaaDesc.format = self->oreColorView->texture()->format();
        msaaDesc.renderTarget = true;
        msaaDesc.sampleCount = sc;
        msaaDesc.label = "GPUCanvasMSAAColor";
        self->oreMSAAColorTexture = oreCtx->makeTexture(msaaDesc);
        ore::TextureViewDesc msaaViewDesc;
        msaaViewDesc.texture = self->oreMSAAColorTexture.get();
        self->oreMSAAColorView = oreCtx->makeTextureView(msaaViewDesc);

        ore::TextureDesc depthDesc;
        depthDesc.width = w;
        depthDesc.height = h;
        depthDesc.format = ore::TextureFormat::depth32float;
        depthDesc.renderTarget = true;
        depthDesc.sampleCount = sc;
        depthDesc.label = "GPUCanvasMSAADepth";
        self->oreDepthTexture = oreCtx->makeTexture(depthDesc);
        ore::TextureViewDesc depthViewDesc;
        depthViewDesc.texture = self->oreDepthTexture.get();
        self->oreDepthView = oreCtx->makeTextureView(depthViewDesc);
    }
    else
    {
        self->oreMSAAColorTexture = nullptr;
        self->oreMSAAColorView = nullptr;
        self->oreDepthTexture = nullptr;
        self->oreDepthView = nullptr;
    }

    auto* img = lua_newrive<ScriptedImage>(L);
    img->image =
        ref_rcp(static_cast<RenderImage*>(self->canvas->renderImage()));
    self->m_imageRef = lua_ref(L, -1);
    lua_pop(L, 1); // pop image

    return 0;
}

static int gpucanvashandle_colorview(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUCanvas>(L, 1);
    if (!self->oreColorView)
    {
        lua_pushnil(L);
        return 1;
    }
    auto* tv = lua_newrive<ScriptedGPUTextureView>(L);
    tv->view = self->oreColorView;
    return 1;
}

static int gpucanvashandle_index(lua_State* L)
{
    auto* self = lua_torive<ScriptedGPUCanvas>(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "image") == 0)
    {
        if (self->m_imageRef != LUA_NOREF)
        {
            rive_lua_pushRef(L, self->m_imageRef);
            return 1;
        }
        lua_pushnil(L);
        return 1;
    }
    if (strcmp(key, "width") == 0 && self->canvas)
    {
        lua_pushnumber(L, self->canvas->width());
        return 1;
    }
    if (strcmp(key, "height") == 0 && self->canvas)
    {
        lua_pushnumber(L, self->canvas->height());
        return 1;
    }
    if (strcmp(key, "sampleCount") == 0)
    {
        uint32_t sc = self->oreMSAAColorTexture
                          ? self->oreMSAAColorTexture->sampleCount()
                          : 1;
        lua_pushnumber(L, sc);
        return 1;
    }
    if (strcmp(key, "hasDepth") == 0)
    {
        lua_pushboolean(L, self->oreDepthView != nullptr);
        return 1;
    }
    if (strcmp(key, "format") == 0)
    {
        if (self->oreColorView && self->oreColorView->texture())
            lua_pushstring(L,
                           lua_totextureformatstring(
                               self->oreColorView->texture()->format()));
        else
            lua_pushnil(L);
        return 1;
    }
    luaL_error(L, "%s is not a valid property of GPUCanvas", key);
    return 0;
}

static int gpucanvashandle_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::beginRenderPass:
                return gpucanvashandle_beginrenderpass(L);
            case (int)LuaAtoms::colorView:
                return gpucanvashandle_colorview(L);
            case (int)LuaAtoms::resize:
                return gpucanvashandle_resize(L);
            default:
                break;
        }
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedGPUCanvas::luaName);
    return 0;
}

// ============================================================================
// Canvas (2D Rive renderer canvas)
// ============================================================================

// Recreate the underlying RenderCanvas at a new size.  Must not be called
// between beginFrame() and endFrame().
static int canvashandle_resize(lua_State* L)
{
    auto* self = lua_torive<ScriptedCanvas>(L, 1);
    uint32_t w = static_cast<uint32_t>(luaL_checkunsigned(L, 2));
    uint32_t h = static_cast<uint32_t>(luaL_checkunsigned(L, 3));

    if (self->renderCtx == nullptr)
    {
        luaL_error(L, "Canvas: renderCtx not initialized");
    }
    if (self->m_state != CanvasState::Idle)
    {
        luaL_error(L, "Canvas:resize() called during an active frame");
    }

    auto newCanvas = self->renderCtx->makeRenderCanvas(w, h);
    if (!newCanvas)
    {
        luaL_error(L, "Canvas:resize() failed to create RenderCanvas");
    }

    if (self->m_L != nullptr && self->m_imageRef != LUA_NOREF)
    {
        lua_unref(self->m_L, self->m_imageRef);
        self->m_imageRef = LUA_NOREF;
    }
    self->canvas = std::move(newCanvas);

    auto* img = lua_newrive<ScriptedImage>(L);
    img->image =
        ref_rcp(static_cast<RenderImage*>(self->canvas->renderImage()));
    self->m_imageRef = lua_ref(L, -1);
    lua_pop(L, 1);

    return 0;
}

// Begin a Rive rendering frame on this canvas.  Returns a Renderer that the
// caller can use to issue Rive draw calls.  Must be paired with endFrame().
//
// Optional `desc` table fields:
//   clearColor: Color?   ARGB integer (e.g. 0xFF000000 for opaque black).
//                        Defaults to transparent black (0).
static int canvashandle_beginframe(lua_State* L)
{
    auto* self = lua_torive<ScriptedCanvas>(L, 1);
    if (self->renderCtx == nullptr)
    {
        luaL_error(L, "Canvas: renderCtx not initialized");
    }
    if (self->m_state != CanvasState::Idle)
    {
        luaL_error(L, "Canvas:beginFrame() called during an active frame");
    }
    if (!self->canvas)
    {
        luaL_error(L, "Canvas:beginFrame() — canvas is null");
    }

    gpu::RenderContext::FrameDescriptor desc{};
    desc.renderTargetWidth = self->canvas->width();
    desc.renderTargetHeight = self->canvas->height();
    desc.loadAction = gpu::LoadAction::clear;
    desc.clearColor = 0; // transparent black

    if (lua_gettop(L) >= 2 && lua_istable(L, 2))
    {
        lua_getfield(L, 2, "clearColor");
        if (!lua_isnil(L, -1))
        {
            // clearColor is a ColorInt (ARGB uint32)
            desc.clearColor = static_cast<ColorInt>(lua_tounsigned(L, -1));
        }
        lua_pop(L, 1);
    }

    self->renderCtx->beginFrame(desc);

    // Allocate a RiveRenderer that issues into this render context.
    // Deleted in endFrame() (or in the destructor if endFrame is never called).
    self->m_riveRenderer = new RiveRenderer(self->renderCtx);
    self->m_state = CanvasState::Rendering;

    // Push a non-owning ScriptedRenderer wrapping our RiveRenderer and keep a
    // registry ref so the Lua object stays alive until endFrame().
    lua_newrive<ScriptedRenderer>(L, self->m_riveRenderer);
    lua_pushvalue(L, -1);
    self->m_rendererRef = lua_ref(L, -1);
    lua_pop(L, 1); // pop the extra copy used for ref; original stays on stack

    return 1; // returns the ScriptedRenderer
}

// Flush all pending Rive draw calls for this frame to the canvas render target,
// then release the renderer.  Must be called after beginFrame().
static int canvashandle_endframe(lua_State* L)
{
    auto* self = lua_torive<ScriptedCanvas>(L, 1);
    if (self->m_state != CanvasState::Rendering)
    {
        luaL_error(L, "Canvas:endFrame() called without beginFrame()");
    }

    // Null out the ScriptedRenderer's pointer so it can no longer issue draws.
    if (self->m_L != nullptr && self->m_rendererRef != LUA_NOREF)
    {
        rive_lua_pushRef(L, self->m_rendererRef);
        if (!lua_isnil(L, -1))
        {
            auto* sr = lua_torive<ScriptedRenderer>(L, -1);
            if (sr != nullptr)
                sr->end();
        }
        lua_pop(L, 1);
        lua_unref(self->m_L, self->m_rendererRef);
        self->m_rendererRef = LUA_NOREF;
    }

    // Create a command buffer, flush the render context into the canvas
    // render target, then commit. Without a proper command buffer the
    // buffer ring mutex would never be unlocked (the completion handler
    // that unlocks it is registered on the command buffer).
    void* commandBuffer = self->renderCtx->impl()->makeCommandBuffer();

    gpu::RenderContext::FlushResources flush{};
    flush.renderTarget = self->canvas->renderTarget();
    flush.externalCommandBuffer = commandBuffer;
    self->renderCtx->flush(flush);

    self->renderCtx->impl()->commitCommandBuffer(commandBuffer);

    // Destroy the RiveRenderer — it is no longer valid after flush().
    delete self->m_riveRenderer;
    self->m_riveRenderer = nullptr;
    self->m_state = CanvasState::Idle;

    return 0;
}

static int canvashandle_index(lua_State* L)
{
    auto* self = lua_torive<ScriptedCanvas>(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "image") == 0)
    {
        if (self->m_imageRef != LUA_NOREF)
        {
            rive_lua_pushRef(L, self->m_imageRef);
            return 1;
        }
        lua_pushnil(L);
        return 1;
    }
    if (strcmp(key, "width") == 0 && self->canvas)
    {
        lua_pushnumber(L, self->canvas->width());
        return 1;
    }
    if (strcmp(key, "height") == 0 && self->canvas)
    {
        lua_pushnumber(L, self->canvas->height());
        return 1;
    }
    luaL_error(L, "%s is not a valid property of Canvas", key);
    return 0;
}

static int canvashandle_namecall(lua_State* L)
{
    int atom;
    const char* str = lua_namecallatom(L, &atom);
    if (str != nullptr)
    {
        switch (atom)
        {
            case (int)LuaAtoms::beginFrame:
                return canvashandle_beginframe(L);
            case (int)LuaAtoms::endFrame:
                return canvashandle_endframe(L);
            case (int)LuaAtoms::resize:
                return canvashandle_resize(L);
            default:
                break;
        }
    }
    luaL_error(L,
               "%s is not a valid method of %s",
               str,
               ScriptedCanvas::luaName);
    return 0;
}

// ============================================================================
// Registration
// ============================================================================

static const luaL_Reg empty[] = {
    {NULL, NULL},
};

template <typename T>
static void register_type_with_constructor(lua_State* L,
                                           lua_CFunction constructor,
                                           lua_CFunction namecall = nullptr,
                                           lua_CFunction indexfn = nullptr)
{
    luaL_register(L, T::luaName, empty);
    lua_register_rive<T>(L);

    if (namecall)
    {
        lua_pushcfunction(L, namecall, nullptr);
        lua_setfield(L, -2, "__namecall");
    }
    if (indexfn)
    {
        lua_pushcfunction(L, indexfn, nullptr);
        lua_setfield(L, -2, "__index");
    }

    // Create metatable for the metatable (so we can call T.new())
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, constructor, nullptr);
    lua_setfield(L, -2, "__index");

    // Also set __call so T.new(...) works
    // Actually the pattern is T.new(), so set new on the __index table
    // Let's do it properly: make __index return the new function
    lua_pop(L, 1); // pop the meta-meta

    // Simpler: just put "new" on the library table directly
    lua_pushcfunction(L, constructor, nullptr);
    lua_setfield(L, -2, "new");

    lua_setreadonly(L, -1, true);
    lua_pop(L, 1); // pop the metatable
}

int luaopen_rive_gpu(lua_State* L)
{
    static const luaL_Reg shaderStatics[] = {{"new", shader_construct},
                                             {nullptr, nullptr}};
    static const luaL_Reg gpuBufferStatics[] = {{"new", gpubuffer_construct},
                                                {nullptr, nullptr}};
    static const luaL_Reg gpuTextureStatics[] = {{"new", gputexture_construct},
                                                 {nullptr, nullptr}};
    static const luaL_Reg gpuSamplerStatics[] = {{"new", gpusampler_construct},
                                                 {nullptr, nullptr}};
    static const luaL_Reg gpuPipelineStatics[] = {
        {"new", gpupipeline_construct},
        {nullptr, nullptr}};
    static const luaL_Reg gpuBindGroupStatics[] = {
        {"new", gpubindgroup_construct},
        {nullptr, nullptr}};
    static const luaL_Reg gpuBindGroupLayoutStatics[] = {
        {"new", gpubindgrouplayout_construct},
        {nullptr, nullptr}};
    // Shader
    {
        luaL_register(L, ScriptedShader::luaName, shaderStatics);
        lua_register_rive<ScriptedShader>(L);
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // GPUBuffer
    {
        luaL_register(L, ScriptedGPUBuffer::luaName, gpuBufferStatics);
        lua_register_rive<ScriptedGPUBuffer>(L);
        lua_pushcfunction(L, gpubuffer_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");
        lua_pushcfunction(L, gpubuffer_index, nullptr);
        lua_setfield(L, -2, "__index");
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // GPUTexture
    {
        luaL_register(L, ScriptedGPUTexture::luaName, gpuTextureStatics);
        lua_register_rive<ScriptedGPUTexture>(L);
        lua_pushcfunction(L, gputexture_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");
        lua_pushcfunction(L, gputexture_index, nullptr);
        lua_setfield(L, -2, "__index");
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // GPUSampler
    {
        luaL_register(L, ScriptedGPUSampler::luaName, gpuSamplerStatics);
        lua_register_rive<ScriptedGPUSampler>(L);
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // GPUPipeline
    {
        luaL_register(L, ScriptedGPUPipeline::luaName, gpuPipelineStatics);
        lua_register_rive<ScriptedGPUPipeline>(L);
        lua_pushcfunction(L, gpupipeline_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // GPUBindGroup
    {
        luaL_register(L, ScriptedGPUBindGroup::luaName, gpuBindGroupStatics);
        lua_register_rive<ScriptedGPUBindGroup>(L);
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // GPUBindGroupLayout
    {
        luaL_register(L,
                      ScriptedGPUBindGroupLayout::luaName,
                      gpuBindGroupLayoutStatics);
        lua_register_rive<ScriptedGPUBindGroupLayout>(L);
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // GPURenderPass
    {
        luaL_register(L, ScriptedGPURenderPass::luaName, empty);
        lua_register_rive<ScriptedGPURenderPass>(L);
        lua_pushcfunction(L, gpurenderpass_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // GPUTextureView (no constructor — created via GPUTexture:view())
    {
        luaL_register(L, ScriptedGPUTextureView::luaName, empty);
        lua_register_rive<ScriptedGPUTextureView>(L);
        lua_pushcfunction(L, gputextureview_index, nullptr);
        lua_setfield(L, -2, "__index");
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // GPUCanvas (no public constructor — created via context:gpuCanvas())
    {
        luaL_register(L, ScriptedGPUCanvas::luaName, empty);
        lua_register_rive<ScriptedGPUCanvas>(L);
        lua_pushcfunction(L, gpucanvashandle_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");
        lua_pushcfunction(L, gpucanvashandle_index, nullptr);
        lua_setfield(L, -2, "__index");
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // Canvas (no public constructor — created via context:canvas())
    {
        luaL_register(L, ScriptedCanvas::luaName, empty);
        lua_register_rive<ScriptedCanvas>(L);
        lua_pushcfunction(L, canvashandle_namecall, nullptr);
        lua_setfield(L, -2, "__namecall");
        lua_pushcfunction(L, canvashandle_index, nullptr);
        lua_setfield(L, -2, "__index");
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    return 0;
}

// ============================================================================
// Image:view() implementation — called from lua_image.cpp
// Lives here because ore headers require ObjC++ on Apple.
// ============================================================================

#include "rive/renderer/rive_render_image.hpp"

// ore::TextureView is complete here — define the destructor and factory.
ScriptedImage::~ScriptedImage() = default;
ScriptedImage* ScriptedImage::luaNew(lua_State* L)
{
    return lua_newrive<ScriptedImage>(L);
}

int riveImageViewImpl(lua_State* L)
{
    auto* self = lua_torive<ScriptedImage>(L, 1);
    if (!self->image)
    {
        luaL_error(L, "Image has no backing texture");
        return 0;
    }

    // Safe cast — returns nullptr if the image isn't GPU-backed.
    auto* riveImage = lite_rtti_cast<RiveRenderImage*>(self->image.get());
    if (!riveImage)
    {
        luaL_error(L, "Image is not a GPU-backed RiveRenderImage");
        return 0;
    }
    gpu::Texture* sourceGpuTex = riveImage->getTexture();
    if (!sourceGpuTex)
    {
        luaL_error(L, "Image GPU texture not available");
        return 0;
    }

    // Get ore::Context from scripting context.
    auto* ctx = static_cast<ScriptingContext*>(lua_getthreaddata(L));
    auto* oreCtx = static_cast<ore::Context*>(ctx->oreContext());
    if (!oreCtx)
    {
        luaL_error(L, "GPU context not available for Image:view()");
        return 0;
    }

    if (!self->cachedOreView)
    {
        // GL canvas-import boundary: on GL/WebGL, sampling a Rive 2D
        // RenderCanvas as a WGSL texture requires a Y-flipped companion
        // because PLS renders the canvas bottom-up while WGSL expects
        // V=0 at the visual top of the image. The render context's
        // getCanvasImportMirror returns nullptr on every backend except
        // GL — on GL it lazily allocates a companion texture, registers
        // a per-flush blit hook, and returns the companion image. We
        // cache the companion's RiveRenderImage so the companion stays
        // alive as long as this ScriptedImage does.
        //
        // See dev/ore_canvas_import_invariant.md.
        gpu::Texture* texToWrap = sourceGpuTex;
#if defined(ORE_BACKEND_GL)
        {
            auto* renderCtx =
                static_cast<gpu::RenderContext*>(ctx->renderContext());
            self->cachedMirrorImage =
                getCanvasImportMirrorGL(renderCtx,
                                        sourceGpuTex,
                                        self->image->width(),
                                        self->image->height());
            if (self->cachedMirrorImage != nullptr)
            {
                auto* mirrorRive = lite_rtti_cast<RiveRenderImage*>(
                    self->cachedMirrorImage.get());
                if (mirrorRive != nullptr &&
                    mirrorRive->getTexture() != nullptr)
                {
                    texToWrap = mirrorRive->getTexture();
                }
            }
        }
#endif // ORE_BACKEND_GL

        self->cachedOreView = oreCtx->wrapRiveTexture(texToWrap,
                                                      self->image->width(),
                                                      self->image->height());
        if (!self->cachedOreView)
        {
            luaL_error(L, "Image:view() not supported on this backend");
            return 0;
        }
    }

    // Create the GPUTextureView and have it retain the RenderImage so the
    // underlying gpu::Texture stays alive even if the Image is GC'd.
    auto* tv = lua_newrive<ScriptedGPUTextureView>(L);
    tv->view = self->cachedOreView;
    tv->retainedImage = self->image;

    return 1;
}

#endif // RIVE_CANVAS && RIVE_ORE
#endif // WITH_RIVE_SCRIPTING

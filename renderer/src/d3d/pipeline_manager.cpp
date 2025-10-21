/*
 * Copyright 2025 Rive
 */
#include "rive/renderer/d3d/pipeline_manager.hpp"
#include "rive/renderer/d3d/d3d_constants.hpp"
#include "generated/shaders/constants.glsl.hpp"
#include <sstream>

#include <d3dcompiler.h>

#include "generated/shaders/advanced_blend.glsl.hpp"
#include "generated/shaders/atomic_draw.glsl.hpp"
#include "generated/shaders/constants.glsl.hpp"
#include "generated/shaders/common.glsl.hpp"
#include "generated/shaders/draw_image_mesh.vert.hpp"
#include "generated/shaders/draw_raster_order_mesh.frag.hpp"
#include "generated/shaders/draw_path_common.glsl.hpp"
#include "generated/shaders/draw_path.vert.hpp"
#include "generated/shaders/draw_raster_order_path.frag.hpp"
#include "generated/shaders/hlsl.glsl.hpp"

namespace rive::gpu::d3d_utils
{
static std::string build_shader(DrawType drawType,
                                ShaderFeatures shaderFeatures,
                                InterlockMode interlockMode,
                                ShaderMiscFlags shaderMiscFlags,
                                const D3DCapabilities& d3dCapabilities,
                                const char* shaderTypeDefine)
{

    std::ostringstream s;
    s << "#define " << shaderTypeDefine << '\n';
    for (size_t i = 0; i < kShaderFeatureCount; ++i)
    {
        ShaderFeatures feature = static_cast<ShaderFeatures>(1 << i);
        if (shaderFeatures & feature)
        {
            s << "#define " << GetShaderFeatureGLSLName(feature) << " 1\n";
        }
    }
    if (d3dCapabilities.supportsRasterizerOrderedViews)
    {
        if (interlockMode == InterlockMode::rasterOrdering &&
            drawType != DrawType::interiorTriangulation)
        {
            s << "#define " << GLSL_ENABLE_RASTERIZER_ORDERED_VIEWS << '\n';
        }
    }
    if (d3dCapabilities.supportsTypedUAVLoadStore)
    {
        s << "#define " << GLSL_ENABLE_TYPED_UAV_LOAD_STORE << '\n';
    }
    if (d3dCapabilities.supportsMin16Precision)
    {
        s << "#define " << GLSL_ENABLE_MIN_16_PRECISION << '\n';
    }
    if (shaderMiscFlags & ShaderMiscFlags::fixedFunctionColorOutput)
    {
        s << "#define " << GLSL_FIXED_FUNCTION_COLOR_OUTPUT << '\n';
    }
    if (shaderMiscFlags & ShaderMiscFlags::coalescedResolveAndTransfer)
    {
        s << "#define " << GLSL_COALESCED_PLS_RESOLVE_AND_TRANSFER << '\n';
        if (!d3dCapabilities.allowsUAVSlot0WithColorOutput)
        {
            s << "#define " << GLSL_COLOR_PLANE_IDX_OVERRIDE << ' '
              << COALESCED_OFFSCREEN_COLOR_PLANE_IDX << '\n';
        }
    }
    if (shaderMiscFlags & ShaderMiscFlags::clockwiseFill)
    {
        s << "#define " << GLSL_CLOCKWISE_FILL << " 1\n";
    }
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
            s << "#define " << GLSL_DRAW_PATH << '\n';
            break;
        case DrawType::interiorTriangulation:
            s << "#define " << GLSL_DRAW_INTERIOR_TRIANGLES << '\n';
            break;
        case DrawType::atlasBlit:
            s << "#define " << GLSL_ATLAS_BLIT << " 1\n";
            break;
        case DrawType::imageRect:
            assert(interlockMode == InterlockMode::atomics);
            s << "#define " << GLSL_DRAW_IMAGE << '\n';
            s << "#define " << GLSL_DRAW_IMAGE_RECT << '\n';
            break;
        case DrawType::imageMesh:
            s << "#define " << GLSL_DRAW_IMAGE << '\n';
            s << "#define " << GLSL_DRAW_IMAGE_MESH << '\n';
            break;
        case DrawType::renderPassResolve:
            assert(interlockMode == InterlockMode::atomics);
            s << "#define " << GLSL_DRAW_RENDER_TARGET_UPDATE_BOUNDS << '\n';
            s << "#define " << GLSL_RESOLVE_PLS << '\n';
            break;
        case DrawType::msaaStrokes:
        case DrawType::msaaMidpointFanBorrowedCoverage:
        case DrawType::msaaMidpointFans:
        case DrawType::msaaMidpointFanStencilReset:
        case DrawType::msaaMidpointFanPathsStencil:
        case DrawType::msaaMidpointFanPathsCover:
        case DrawType::msaaOuterCubics:
        case DrawType::msaaStencilClipReset:
        case DrawType::renderPassInitialize:
            RIVE_UNREACHABLE();
    }
    s << glsl::constants << '\n';
    s << glsl::hlsl << '\n';
    s << glsl::common << '\n';
    if (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND)
    {
        s << glsl::advanced_blend << '\n';
    }
    if (interlockMode == InterlockMode::rasterOrdering)
    {
        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
            case DrawType::interiorTriangulation:
                s << glsl::draw_path_common << '\n';
                s << glsl::draw_path_vert << '\n';
                s << glsl::draw_raster_order_path_frag << '\n';
                break;
            case DrawType::atlasBlit:
                s << glsl::draw_path_common << '\n';
                s << glsl::draw_path_vert << '\n';
                s << glsl::draw_raster_order_mesh_frag << '\n';
                break;
            case DrawType::imageMesh:
                s << glsl::draw_image_mesh_vert << '\n';
                s << glsl::draw_raster_order_mesh_frag << '\n';
                break;
            case DrawType::imageRect:
            case DrawType::renderPassResolve:
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStencilClipReset:
            case DrawType::renderPassInitialize:
                RIVE_UNREACHABLE();
        }
    }
    else
    {
        assert(interlockMode == InterlockMode::atomics);
        s << glsl::draw_path_common << '\n';
        s << glsl::atomic_draw << '\n';
    }

    return s.str();
}

static const char* shader_type_define_from_target(const char* target)
{
    if (target[0] == 'v')
    {
        return GLSL_VERTEX;
    }
    else if (target[0] == 'p')
    {
        return GLSL_FRAGMENT;
    }

    assert(false);
    return "<unknown>";
}

ComPtr<ID3DBlob> compile_shader_to_blob(DrawType drawType,
                                        ShaderFeatures shaderFeatures,
                                        InterlockMode interlockMode,
                                        ShaderMiscFlags shaderMiscFlags,
                                        const D3DCapabilities& d3dCapabilities,
                                        const char* target)
{
    const std::string& sourceStr =
        build_shader(drawType,
                     shaderFeatures,
                     interlockMode,
                     shaderMiscFlags,
                     d3dCapabilities,
                     shader_type_define_from_target(target));

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;

#ifdef RIVE_RAW_SHADERS
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG |
                 D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#endif

    HRESULT hr = D3DCompile(sourceStr.c_str(),
                            sourceStr.length(),
                            nullptr,
                            nullptr,
                            nullptr,
                            "main",
                            target,
                            flags,
                            0,
                            &blob,
                            &errors);
    if (errors && errors->GetBufferPointer())
    {
        fprintf(stderr, "Errors or warnings compiling shader.\n");
        int l = 1;
        std::stringstream stream(sourceStr);
        std::string lineStr;
        while (std::getline(stream, lineStr, '\n'))
        {
            fprintf(stderr, "%4i| %s\n", l++, lineStr.c_str());
        }
        fprintf(stderr,
                "%s\n",
                reinterpret_cast<char*>(errors->GetBufferPointer()));
        abort();
    }
    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to compile shader.\n");
        abort();
    }
    return blob;
}
}; // namespace rive::gpu::d3d_utils

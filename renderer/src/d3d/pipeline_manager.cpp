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
#include "generated/shaders/color_ramp.glsl.hpp"
#include "generated/shaders/constants.glsl.hpp"
#include "generated/shaders/common.glsl.hpp"
#include "generated/shaders/draw_image_mesh.glsl.hpp"
#include "generated/shaders/draw_path_common.glsl.hpp"
#include "generated/shaders/draw_path.glsl.hpp"
#include "generated/shaders/hlsl.glsl.hpp"
#include "generated/shaders/bezier_utils.glsl.hpp"
#include "generated/shaders/render_atlas.glsl.hpp"
#include "generated/shaders/tessellate.glsl.hpp"

namespace rive::gpu::d3d_utils
{
std::string build_shader(DrawType drawType,
                         ShaderFeatures shaderFeatures,
                         InterlockMode interlockMode,
                         ShaderMiscFlags shaderMiscFlags,
                         const D3DCapabilities& d3dCapabilities)
{
    std::ostringstream s;
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
        case DrawType::atlasBlit:
            s << "#define " << GLSL_ATLAS_BLIT << " 1\n";
            [[fallthrough]];
        case DrawType::interiorTriangulation:
            s << "#define " << GLSL_DRAW_INTERIOR_TRIANGLES << '\n';
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
        case DrawType::atomicResolve:
            assert(interlockMode == InterlockMode::atomics);
            s << "#define " << GLSL_DRAW_RENDER_TARGET_UPDATE_BOUNDS << '\n';
            s << "#define " << GLSL_RESOLVE_PLS << '\n';
            break;
        case DrawType::atomicInitialize:
        case DrawType::msaaStrokes:
        case DrawType::msaaMidpointFanBorrowedCoverage:
        case DrawType::msaaMidpointFans:
        case DrawType::msaaMidpointFanStencilReset:
        case DrawType::msaaMidpointFanPathsStencil:
        case DrawType::msaaMidpointFanPathsCover:
        case DrawType::msaaOuterCubics:
        case DrawType::msaaStencilClipReset:
            RIVE_UNREACHABLE();
    }
    s << glsl::constants << '\n';
    s << glsl::hlsl << '\n';
    s << glsl::common << '\n';
    if (shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND)
    {
        s << glsl::advanced_blend << '\n';
    }
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
            s << glsl::draw_path_common << '\n';
            s << (interlockMode == InterlockMode::rasterOrdering
                      ? glsl::draw_path
                      : glsl::atomic_draw)
              << '\n';
            break;
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
            s << glsl::draw_path_common << '\n';
            s << (interlockMode == InterlockMode::rasterOrdering
                      ? glsl::draw_path
                      : glsl::atomic_draw)
              << '\n';
            break;
        case DrawType::imageRect:
            assert(interlockMode == InterlockMode::atomics);
            s << glsl::draw_path_common << '\n';
            s << glsl::atomic_draw << '\n';
            break;
        case DrawType::imageMesh:
            if (interlockMode == InterlockMode::rasterOrdering)
            {
                s << glsl::draw_image_mesh << '\n';
            }
            else
            {
                s << glsl::draw_path_common << '\n';
                s << glsl::atomic_draw << '\n';
            }
            break;
        case DrawType::atomicResolve:
        case DrawType::msaaStencilClipReset:
            assert(interlockMode == InterlockMode::atomics);
            s << glsl::draw_path_common << '\n';
            s << glsl::atomic_draw << '\n';
            break;
        case DrawType::atomicInitialize:
        case DrawType::msaaStrokes:
        case DrawType::msaaMidpointFanBorrowedCoverage:
        case DrawType::msaaMidpointFans:
        case DrawType::msaaMidpointFanStencilReset:
        case DrawType::msaaMidpointFanPathsStencil:
        case DrawType::msaaMidpointFanPathsCover:
        case DrawType::msaaOuterCubics:
            RIVE_UNREACHABLE();
    }

    return s.str();
}

ComPtr<ID3DBlob> compile_pixel_source_to_blob(const std::string& sharedSource,
                                              const char* target)
{
    std::ostringstream source;
    source << "#define " << GLSL_FRAGMENT << '\n';
    source << sharedSource;

    const std::string& sourceStr = source.str();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(sourceStr.c_str(),
                            sourceStr.length(),
                            nullptr,
                            nullptr,
                            nullptr,
                            "main",
                            target,
                            D3DCOMPILE_ENABLE_STRICTNESS,
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

ComPtr<ID3DBlob> compile_vertex_source_to_blob(const std::string& sharedSource,
                                               const char* target)
{
    std::ostringstream source;
    source << "#define " << GLSL_VERTEX << '\n';
    source << sharedSource;

    const std::string& sourceStr = source.str();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(sourceStr.c_str(),
                            sourceStr.length(),
                            nullptr,
                            nullptr,
                            nullptr,
                            "main",
                            target,
                            D3DCOMPILE_ENABLE_STRICTNESS,
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
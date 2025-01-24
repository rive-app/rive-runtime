/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/gpu.hpp"

#include "rive/renderer/render_target.hpp"
#include "shaders/constants.glsl"
#include "rive/renderer/texture.hpp"
#include "rive_render_paint.hpp"
#include "gradient.hpp"

#include "generated/shaders/draw_path.exports.h"

namespace rive::gpu
{
static_assert(kGradTextureWidth == GRAD_TEXTURE_WIDTH);
static_assert(kTessTextureWidth == TESS_TEXTURE_WIDTH);
static_assert(kTessTextureWidthLog2 == TESS_TEXTURE_WIDTH_LOG2);

uint32_t ShaderUniqueKey(DrawType drawType,
                         ShaderFeatures shaderFeatures,
                         InterlockMode interlockMode,
                         ShaderMiscFlags miscFlags)
{
    if (miscFlags & ShaderMiscFlags::coalescedResolveAndTransfer)
    {
        assert(drawType == DrawType::atomicResolve);
        assert(shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND);
        assert(interlockMode == InterlockMode::atomics);
    }
    if (miscFlags & (ShaderMiscFlags::storeColorClear |
                     ShaderMiscFlags::swizzleColorBGRAToRGBA))
    {
        assert(drawType == DrawType::atomicInitialize);
    }
    uint32_t drawTypeKey;
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
            drawTypeKey = 0;
            break;
        case DrawType::interiorTriangulation:
            drawTypeKey = 1;
            break;
        case DrawType::imageRect:
            drawTypeKey = 2;
            break;
        case DrawType::imageMesh:
            drawTypeKey = 3;
            break;
        case DrawType::atomicInitialize:
            assert(interlockMode == gpu::InterlockMode::atomics);
            drawTypeKey = 4;
            break;
        case DrawType::atomicResolve:
            assert(interlockMode == gpu::InterlockMode::atomics);
            drawTypeKey = 5;
            break;
        case DrawType::stencilClipReset:
            assert(interlockMode == gpu::InterlockMode::msaa);
            drawTypeKey = 6;
            break;
    }
    uint32_t key = static_cast<uint32_t>(miscFlags);
    assert(static_cast<uint32_t>(interlockMode) < 1 << 2);
    key = (key << 2) | static_cast<uint32_t>(interlockMode);
    key = (key << kShaderFeatureCount) |
          (shaderFeatures & ShaderFeaturesMaskFor(drawType, interlockMode))
              .bits();
    assert(drawTypeKey < 1 << 3);
    key = (key << 3) | drawTypeKey;
    return key;
}

const char* GetShaderFeatureGLSLName(ShaderFeatures feature)
{
    switch (feature)
    {
        case ShaderFeatures::NONE:
            RIVE_UNREACHABLE();
        case ShaderFeatures::ENABLE_CLIPPING:
            return GLSL_ENABLE_CLIPPING;
        case ShaderFeatures::ENABLE_CLIP_RECT:
            return GLSL_ENABLE_CLIP_RECT;
        case ShaderFeatures::ENABLE_ADVANCED_BLEND:
            return GLSL_ENABLE_ADVANCED_BLEND;
        case ShaderFeatures::ENABLE_FEATHER:
            return GLSL_ENABLE_FEATHER;
        case ShaderFeatures::ENABLE_EVEN_ODD:
            return GLSL_ENABLE_EVEN_ODD;
        case ShaderFeatures::ENABLE_NESTED_CLIPPING:
            return GLSL_ENABLE_NESTED_CLIPPING;
        case ShaderFeatures::ENABLE_HSL_BLEND_MODES:
            return GLSL_ENABLE_HSL_BLEND_MODES;
    }
    RIVE_UNREACHABLE();
}

constexpr static float pack_params(int32_t patchSegmentSpan, int32_t vertexType)
{
    return static_cast<float>((patchSegmentSpan << 2) | vertexType);
}

static void generate_buffer_data_for_patch_type(PatchType patchType,
                                                PatchVertex vertices[],
                                                uint16_t indices[],
                                                uint16_t baseVertex)
{
    // AA border vertices. "Inner tessellation curves" have one more segment
    // without a fan triangle whose purpose is to be a bowtie join.
    size_t vertexCount = 0;
    int32_t patchSegmentSpan = patchType == PatchType::outerCurves
                                   ? kOuterCurvePatchSegmentSpan
                                   : kMidpointFanPatchSegmentSpan;
    for (int i = 0; i < patchSegmentSpan; ++i)
    {
        float params = pack_params(patchSegmentSpan, STROKE_VERTEX);
        float l = static_cast<float>(i);
        float r = l + 1;
        if (patchType == PatchType::outerCurves)
        {
            vertices[vertexCount + 0].set(l, 0.f, .5f, params);
            vertices[vertexCount + 1].set(l, 1.f, .0f, params);
            vertices[vertexCount + 2].set(r, 0.f, .5f, params);
            vertices[vertexCount + 3].set(r, 1.f, .0f, params);

            // Give the vertex an alternate position when mirrored so the border
            // has the same diagonals whether mirrored or not.
            vertices[vertexCount + 0].setMirroredPosition(r, 0.f, .5f);
            vertices[vertexCount + 1].setMirroredPosition(l, 0.f, .5f);
            vertices[vertexCount + 2].setMirroredPosition(r, 1.f, .0f);
            vertices[vertexCount + 3].setMirroredPosition(l, 1.f, .0f);
        }
        else if (patchType == PatchType::midpointFanCenterAA)
        {
            vertices[vertexCount + 0].set(l, 0.f, .5f, params);
            vertices[vertexCount + 1].set(l, 1.f, .0f, params);
            vertices[vertexCount + 2].set(r, 0.f, .5f, params);
            vertices[vertexCount + 3].set(r, 1.f, .0f, params);

            // Give the vertex an alternate position when mirrored so the border
            // has the same diagonals whether mirrored or not.
            vertices[vertexCount + 0].setMirroredPosition(r - 1.f, 0.f, .5f);
            vertices[vertexCount + 1].setMirroredPosition(l - 1.f, 0.f, .5f);
            vertices[vertexCount + 2].setMirroredPosition(r - 1.f, 1.f, .0f);
            vertices[vertexCount + 3].setMirroredPosition(l - 1.f, 1.f, .0f);
        }
        else
        {
            assert(patchType == PatchType::midpointFan);
            vertices[vertexCount + 0].set(l, -1.f, 1.f, params);
            vertices[vertexCount + 1].set(l, +1.f, 0.f, params);
            vertices[vertexCount + 2].set(r, -1.f, 1.f, params);
            vertices[vertexCount + 3].set(r, +1.f, 0.f, params);

            // Give the vertex an alternate position when mirrored so the border
            // has the same diagonals whether morrored or not.
            vertices[vertexCount + 0].setMirroredPosition(r - 1.f, -1.f, 1.f);
            vertices[vertexCount + 1].setMirroredPosition(l - 1.f, -1.f, 1.f);
            vertices[vertexCount + 2].setMirroredPosition(r - 1.f, +1.f, 0.f);
            vertices[vertexCount + 3].setMirroredPosition(l - 1.f, +1.f, 0.f);
        }
        vertexCount += 4;
    }

    // Bottom (negative coverage) side of the AA border.
    for (int i = 0; i < patchSegmentSpan; ++i)
    {
        float params = pack_params(patchSegmentSpan, STROKE_VERTEX);
        float l = static_cast<float>(i);
        float r = l + 1;
        if (patchType == PatchType::outerCurves)
        {
            vertices[vertexCount + 0].set(l, -0.f, .5f, params);
            vertices[vertexCount + 1].set(r, -0.f, .5f, params);
            vertices[vertexCount + 2].set(l, -1.f, .0f, params);
            vertices[vertexCount + 3].set(r, -1.f, .0f, params);

            // Give the vertex an alternate position when mirrored so the border
            // has the same diagonals whether mirrored or not.
            vertices[vertexCount + 0].setMirroredPosition(r, -0.f, .5f);
            vertices[vertexCount + 1].setMirroredPosition(r, -1.f, .0f);
            vertices[vertexCount + 2].setMirroredPosition(l, -0.f, .5f);
            vertices[vertexCount + 3].setMirroredPosition(l, -1.f, .0f);
            vertexCount += 4;
        }
        else if (patchType == PatchType::midpointFanCenterAA)
        {
            vertices[vertexCount + 0].set(l, -0.f, .5f, params);
            vertices[vertexCount + 1].set(r, -0.f, .5f, params);
            vertices[vertexCount + 2].set(l, -1.f, .0f, params);
            vertices[vertexCount + 3].set(r, -1.f, .0f, params);

            // Give the vertex an alternate position when mirrored so the border
            // has the same diagonals whether mirrored or not.
            vertices[vertexCount + 0].setMirroredPosition(r - 1.f, -0.f, .5f);
            vertices[vertexCount + 1].setMirroredPosition(r - 1.f, -1.f, .0f);
            vertices[vertexCount + 2].setMirroredPosition(l - 1.f, -0.f, .5f);
            vertices[vertexCount + 3].setMirroredPosition(l - 1.f, -1.f, .0f);
            vertexCount += 4;
        }
    }

    // Triangle fan vertices. (These only touch the first "fanSegmentSpan"
    // segments on inner tessellation curves.
    size_t fanVerticesIdx = vertexCount;
    size_t fanSegmentSpan = patchType == PatchType::outerCurves
                                ? patchSegmentSpan - 1
                                : patchSegmentSpan;
    // The fan must be a power of two.
    assert((fanSegmentSpan & (fanSegmentSpan - 1)) == 0);
    for (int i = 0; i <= fanSegmentSpan; ++i)
    {
        float params = pack_params(patchSegmentSpan, FAN_VERTEX);
        if (patchType == PatchType::outerCurves)
        {
            vertices[vertexCount].set(static_cast<float>(i), 0.f, 1, params);
        }
        else if (patchType == PatchType::midpointFanCenterAA)
        {
            vertices[vertexCount].set(static_cast<float>(i), 0, 1, params);
            vertices[vertexCount].setMirroredPosition(static_cast<float>(i) - 1,
                                                      0,
                                                      1);
        }
        else
        {
            vertices[vertexCount].set(static_cast<float>(i), -1.f, 1, params);
            vertices[vertexCount].setMirroredPosition(static_cast<float>(i) - 1,
                                                      -1.f,
                                                      1);
        }
        ++vertexCount;
    }

    // The midpoint vertex isn't included in outer cubic patches.
    size_t midpointIdx = vertexCount;
    if (patchType != PatchType::outerCurves)
    {
        vertices[vertexCount++]
            .set(0, 0, 1, pack_params(patchSegmentSpan, FAN_MIDPOINT_VERTEX));
    }
    if (patchType == PatchType::outerCurves)
        assert(vertexCount == kOuterCurvePatchVertexCount);
    else if (patchType == PatchType::midpointFanCenterAA)
        assert(vertexCount == kMidpointFanCenterAAPatchVertexCount);
    else
        assert(vertexCount == kMidpointFanPatchVertexCount);

    // AA border indices.
    constexpr static size_t kBorderPatternVertexCount = 4;
    constexpr static size_t kBorderPatternIndexCount = 6;
    constexpr static uint16_t kBorderPattern[kBorderPatternIndexCount] =
        {0, 1, 2, 2, 1, 3};
    constexpr static uint16_t kNegativeBorderPattern[kBorderPatternIndexCount] =
        {0, 2, 1, 1, 2, 3};

    size_t indexCount = 0;
    size_t borderEdgeVerticesIdx = 0;
    for (size_t borderSegmentIdx = 0; borderSegmentIdx < patchSegmentSpan;
         ++borderSegmentIdx)
    {
        for (size_t i = 0; i < kBorderPatternIndexCount; ++i)
        {
            indices[indexCount++] =
                baseVertex + borderEdgeVerticesIdx + kBorderPattern[i];
        }
        borderEdgeVerticesIdx += kBorderPatternVertexCount;
    }

    // Bottom (negative coverage) side of the AA border.
    if (patchType == PatchType::midpointFan)
    {
        assert(indexCount == kMidpointFanPatchBorderIndexCount);
    }
    else
    {
        for (size_t borderSegmentIdx = 0; borderSegmentIdx < patchSegmentSpan;
             ++borderSegmentIdx)
        {
            for (size_t i = 0; i < kBorderPatternIndexCount; ++i)
            {
                indices[indexCount++] = baseVertex + borderEdgeVerticesIdx +
                                        kNegativeBorderPattern[i];
            }
            borderEdgeVerticesIdx += kBorderPatternVertexCount;
        }
        if (patchType == PatchType::midpointFanCenterAA)
            assert(indexCount == kMidpointFanCenterAAPatchBorderIndexCount);
        else
            assert(indexCount == kOuterCurvePatchBorderIndexCount);
    }

    assert(borderEdgeVerticesIdx == fanVerticesIdx);

    // Triangle fan indices, in a middle-out topology.
    // Don't include the final bowtie join if this is an "outerStroke" patch.
    // (i.e., use fanSegmentSpan and not "patchSegmentSpan".)
    for (int step = 1; step < fanSegmentSpan; step <<= 1)
    {
        for (int i = 0; i < fanSegmentSpan; i += step * 2)
        {
            indices[indexCount++] = fanVerticesIdx + i + baseVertex;
            indices[indexCount++] = fanVerticesIdx + i + step + baseVertex;
            indices[indexCount++] = fanVerticesIdx + i + step * 2 + baseVertex;
        }
    }
    if (patchType == PatchType::midpointFan ||
        patchType == PatchType::midpointFanCenterAA)
    {
        // Triangle to the contour midpoint.
        indices[indexCount++] = fanVerticesIdx + baseVertex;
        indices[indexCount++] = fanVerticesIdx + fanSegmentSpan + baseVertex;
        indices[indexCount++] = midpointIdx + baseVertex;
        if (patchType == PatchType::midpointFan)
            assert(indexCount == kMidpointFanPatchIndexCount);
        else
            assert(indexCount == kMidpointFanCenterAAPatchIndexCount);
    }
    else
    {
        assert(patchType == PatchType::outerCurves);
        assert(indexCount == kOuterCurvePatchIndexCount);
    }
}

void GeneratePatchBufferData(PatchVertex vertices[kPatchVertexBufferCount],
                             uint16_t indices[kPatchIndexBufferCount])
{
    generate_buffer_data_for_patch_type(PatchType::midpointFan,
                                        vertices,
                                        indices,
                                        0);
    generate_buffer_data_for_patch_type(PatchType::midpointFanCenterAA,
                                        vertices + kMidpointFanPatchVertexCount,
                                        indices + kMidpointFanPatchIndexCount,
                                        kMidpointFanPatchVertexCount);
    generate_buffer_data_for_patch_type(
        PatchType::outerCurves,
        vertices + kMidpointFanPatchVertexCount +
            kMidpointFanCenterAAPatchVertexCount,
        indices + kMidpointFanPatchIndexCount +
            kMidpointFanCenterAAPatchIndexCount,
        kMidpointFanPatchVertexCount + kMidpointFanCenterAAPatchVertexCount);
}

void ClipRectInverseMatrix::reset(const Mat2D& clipMatrix, const AABB& clipRect)
{
    // Find the matrix that transforms from pixel space to "normalized clipRect
    // space", where the clipRect is the normalized rectangle: [-1, -1, +1, +1].
    Mat2D m = clipMatrix * Mat2D(clipRect.width() * .5f,
                                 0,
                                 0,
                                 clipRect.height() * .5f,
                                 clipRect.center().x,
                                 clipRect.center().y);
    if (clipRect.width() <= 0 || clipRect.height() <= 0 ||
        !m.invert(&m_inverseMatrix))
    {
        // If the width or height went zero or negative, or if "m" is
        // non-invertible, clip away everything.
        *this = Empty();
    }
}

static uint32_t paint_type_to_glsl_id(PaintType paintType)
{
    return static_cast<uint32_t>(paintType);
    static_assert((int)PaintType::clipUpdate == CLIP_UPDATE_PAINT_TYPE);
    static_assert((int)PaintType::solidColor == SOLID_COLOR_PAINT_TYPE);
    static_assert((int)PaintType::linearGradient == LINEAR_GRADIENT_PAINT_TYPE);
    static_assert((int)PaintType::radialGradient == RADIAL_GRADIENT_PAINT_TYPE);
    static_assert((int)PaintType::image == IMAGE_PAINT_TYPE);
}

uint32_t ConvertBlendModeToPLSBlendMode(BlendMode riveMode)
{
    switch (riveMode)
    {
        case BlendMode::srcOver:
            return BLEND_SRC_OVER;
        case BlendMode::screen:
            return BLEND_MODE_SCREEN;
        case BlendMode::overlay:
            return BLEND_MODE_OVERLAY;
        case BlendMode::darken:
            return BLEND_MODE_DARKEN;
        case BlendMode::lighten:
            return BLEND_MODE_LIGHTEN;
        case BlendMode::colorDodge:
            return BLEND_MODE_COLORDODGE;
        case BlendMode::colorBurn:
            return BLEND_MODE_COLORBURN;
        case BlendMode::hardLight:
            return BLEND_MODE_HARDLIGHT;
        case BlendMode::softLight:
            return BLEND_MODE_SOFTLIGHT;
        case BlendMode::difference:
            return BLEND_MODE_DIFFERENCE;
        case BlendMode::exclusion:
            return BLEND_MODE_EXCLUSION;
        case BlendMode::multiply:
            return BLEND_MODE_MULTIPLY;
        case BlendMode::hue:
            return BLEND_MODE_HUE;
        case BlendMode::saturation:
            return BLEND_MODE_SATURATION;
        case BlendMode::color:
            return BLEND_MODE_COLOR;
        case BlendMode::luminosity:
            return BLEND_MODE_LUMINOSITY;
    }
    RIVE_UNREACHABLE();
}

uint32_t SwizzleRiveColorToRGBAPremul(ColorInt riveColor)
{
    uint4 rgba = (rive::uint4(riveColor) >> uint4{16, 8, 0, 24}) & 0xffu;
    uint32_t alpha = rgba.w;
    rgba.w = 255;
    uint4 premul = rgba * alpha / 255;
    return simd::reduce_or(premul << uint4{0, 8, 16, 24});
}

FlushUniforms::InverseViewports::InverseViewports(
    const FlushDescriptor& flushDesc,
    const PlatformFeatures& platformFeatures)
{
    float4 numerators = 2;
    // When rendering to the gradient and tessellation textures, ensure that
    // row 0 in input coordinates gets written to row 0 in texture memory.
    // This requires a Y inversion if clip space and framebuffer space have
    // opposing senses of which way is up.
    if (platformFeatures.clipSpaceBottomUp !=
        platformFeatures.framebufferBottomUp)
    {
        numerators.xy = -numerators.xy;
    }
    // When drawing to a render target, ensure that Y=0 (in Rive pixel space)
    // gets drawn to the top of thew viewport.
    // This requires a Y inversion if Rive pixel space and clip space have
    // opposing senses of which way is up.
    if (platformFeatures.clipSpaceBottomUp)
    {
        numerators.w = -numerators.w;
    }
    float4 vals = numerators /
                  float4{static_cast<float>(flushDesc.gradDataHeight),
                         static_cast<float>(flushDesc.tessDataHeight),
                         static_cast<float>(flushDesc.renderTarget->width()),
                         static_cast<float>(flushDesc.renderTarget->height())};
    m_vals[0] = vals[0];
    m_vals[1] = vals[1];
    m_vals[2] = vals[2];
    m_vals[3] = vals[3];
}

FlushUniforms::FlushUniforms(const FlushDescriptor& flushDesc,
                             const PlatformFeatures& platformFeatures) :
    m_inverseViewports(flushDesc, platformFeatures),
    m_renderTargetWidth(flushDesc.renderTarget->width()),
    m_renderTargetHeight(flushDesc.renderTarget->height()),
    m_colorClearValue(SwizzleRiveColorToRGBAPremul(flushDesc.clearColor)),
    m_coverageClearValue(flushDesc.coverageClearValue),
    m_renderTargetUpdateBounds(flushDesc.renderTargetUpdateBounds),
    m_coverageBufferPrefix(flushDesc.coverageBufferPrefix),
    m_pathIDGranularity(platformFeatures.pathIDGranularity)
{}

static void write_matrix(volatile float* dst, const Mat2D& matrix)
{
    const float* vals = matrix.values();
    for (size_t i = 0; i < 6; ++i)
    {
        dst[i] = vals[i];
    }
}

void PathData::set(const Mat2D& m,
                   float strokeRadius,
                   float featherRadius,
                   uint32_t zIndex,
                   const CoverageBufferRange& coverageBufferRange)
{
    write_matrix(m_matrix, m);
    m_strokeRadius = strokeRadius; // 0 if the path is filled.
    m_zIndex = zIndex;
    m_featherRadius = featherRadius;
    m_coverageBufferRange.offset = coverageBufferRange.offset;
    m_coverageBufferRange.pitch = coverageBufferRange.pitch;
    m_coverageBufferRange.offsetX = coverageBufferRange.offsetX;
    m_coverageBufferRange.offsetY = coverageBufferRange.offsetY;
}

void PaintData::set(DrawContents singleDrawContents,
                    PaintType paintType,
                    SimplePaintValue simplePaintValue,
                    GradTextureLayout gradTextureLayout,
                    uint32_t clipID,
                    bool hasClipRect,
                    BlendMode blendMode)
{
    uint32_t shiftedClipID = clipID << 16;
    uint32_t shiftedBlendMode = ConvertBlendModeToPLSBlendMode(blendMode) << 4;
    uint32_t localParams = paint_type_to_glsl_id(paintType);
    switch (paintType)
    {
        case PaintType::solidColor:
        {
            // Swizzle the riveColor to little-endian RGBA (the order expected
            // by GLSL).
            m_color = SwizzleRiveColorToRGBA(simplePaintValue.color);
            localParams |= shiftedClipID | shiftedBlendMode;
            break;
        }
        case PaintType::linearGradient:
        case PaintType::radialGradient:
        {
            uint32_t row = simplePaintValue.colorRampLocation.row;
            if (simplePaintValue.colorRampLocation.isComplex())
            {
                // Complex gradients rows are offset after the simple gradients.
                row += gradTextureLayout.complexOffsetY;
            }
            m_gradTextureY = (static_cast<float>(row) + .5f) *
                             gradTextureLayout.inverseHeight;
            localParams |= shiftedClipID | shiftedBlendMode;
            break;
        }
        case PaintType::image:
        {
            m_opacity = simplePaintValue.imageOpacity;
            localParams |= shiftedClipID | shiftedBlendMode;
            break;
        }
        case PaintType::clipUpdate:
        {
            m_shiftedClipReplacementID = shiftedClipID;
            localParams |= simplePaintValue.outerClipID << 16;
            break;
        }
    }
    if (singleDrawContents & gpu::DrawContents::nonZeroFill)
    {
        localParams |= PAINT_FLAG_NON_ZERO_FILL;
    }
    else if (singleDrawContents & gpu::DrawContents::evenOddFill)
    {
        localParams |= PAINT_FLAG_EVEN_ODD_FILL;
    }
    if (hasClipRect)
    {
        localParams |= PAINT_FLAG_HAS_CLIP_RECT;
    }
    m_params = localParams;
}

void PaintAuxData::set(const Mat2D& viewMatrix,
                       PaintType paintType,
                       SimplePaintValue simplePaintValue,
                       const Gradient* gradient,
                       const Texture* imageTexture,
                       const ClipRectInverseMatrix* clipRectInverseMatrix,
                       const RenderTarget* renderTarget,
                       const gpu::PlatformFeatures& platformFeatures)
{
    switch (paintType)
    {
        case PaintType::solidColor:
        {
            break;
        }
        case PaintType::linearGradient:
        case PaintType::radialGradient:
        case PaintType::image:
        {
            Mat2D paintMatrix;
            viewMatrix.invert(&paintMatrix);
            if (platformFeatures.framebufferBottomUp)
            {
                // Flip _fragCoord.y.
                paintMatrix =
                    paintMatrix * Mat2D(1, 0, 0, -1, 0, renderTarget->height());
            }
            if (paintType == PaintType::image)
            {
                // Since we don't use perspective transformations, the image
                // mipmap level-of-detail is constant throughout the entire
                // path. Compute it ahead of time here.
                float dudx = paintMatrix.xx() * imageTexture->width();
                float dudy = paintMatrix.yx() * imageTexture->height();
                float dvdx = paintMatrix.xy() * imageTexture->width();
                float dvdy = paintMatrix.yy() * imageTexture->height();
                float maxScaleFactorPow2 = std::max(dudx * dudx + dvdx * dvdx,
                                                    dudy * dudy + dvdy * dvdy);
                // Instead of finding sqrt(maxScaleFactorPow2), just multiply
                // the log by .5.
                m_imageTextureLOD =
                    log2f(std::max(maxScaleFactorPow2, 1.f)) * .5f;
            }
            else
            {
                assert(gradient != nullptr);
                const float* gradCoeffs = gradient->coeffs();
                if (paintType == PaintType::linearGradient)
                {
                    paintMatrix = Mat2D(gradCoeffs[0],
                                        0,
                                        gradCoeffs[1],
                                        0,
                                        gradCoeffs[2],
                                        0) *
                                  paintMatrix;
                }
                else
                {
                    assert(paintType == PaintType::radialGradient);
                    float w = 1 / gradCoeffs[2];
                    paintMatrix = Mat2D(w,
                                        0,
                                        0,
                                        w,
                                        -gradCoeffs[0] * w,
                                        -gradCoeffs[1] * w) *
                                  paintMatrix;
                }
                float left, right;
                if (simplePaintValue.colorRampLocation.isComplex())
                {
                    left = 0;
                    right = kGradTextureWidth;
                }
                else
                {
                    left = simplePaintValue.colorRampLocation.col;
                    right = left + 2;
                }
                m_gradTextureHorizontalSpan[0] =
                    (right - left - 1) * GRAD_TEXTURE_INVERSE_WIDTH;
                m_gradTextureHorizontalSpan[1] =
                    (left + .5f) * GRAD_TEXTURE_INVERSE_WIDTH;
            }
            write_matrix(m_matrix, paintMatrix);
            break;
        }
        case PaintType::clipUpdate:
        {
            break;
        }
    }

    if (clipRectInverseMatrix != nullptr)
    {
        Mat2D m = clipRectInverseMatrix->inverseMatrix();
        if (platformFeatures.framebufferBottomUp)
        {
            // Flip _fragCoord.y.
            m = m * Mat2D(1, 0, 0, -1, 0, renderTarget->height());
        }
        write_matrix(m_clipRectInverseMatrix, m);
        m_inverseFwidth.x = -1.f / (fabsf(m.xx()) + fabsf(m.xy()));
        m_inverseFwidth.y = -1.f / (fabsf(m.yx()) + fabsf(m.yy()));
    }
    else
    {
        write_matrix(m_clipRectInverseMatrix,
                     ClipRectInverseMatrix::WideOpen().inverseMatrix());
        m_inverseFwidth.x = 0;
        m_inverseFwidth.y = 0;
    }
}

ImageDrawUniforms::ImageDrawUniforms(
    const Mat2D& matrix,
    float opacity,
    const ClipRectInverseMatrix* clipRectInverseMatrix,
    uint32_t clipID,
    BlendMode blendMode,
    uint32_t zIndex)
{
    write_matrix(m_matrix, matrix);
    m_opacity = opacity;
    write_matrix(m_clipRectInverseMatrix,
                 clipRectInverseMatrix != nullptr
                     ? clipRectInverseMatrix->inverseMatrix()
                     : ClipRectInverseMatrix::WideOpen().inverseMatrix());
    m_clipID = clipID;
    m_blendMode = ConvertBlendModeToPLSBlendMode(blendMode);
    m_zIndex = zIndex;
}

std::tuple<uint32_t, uint32_t> StorageTextureSize(
    size_t bufferSizeInBytes,
    StorageBufferStructure bufferStructure)
{
    assert(bufferSizeInBytes %
               gpu::StorageBufferElementSizeInBytes(bufferStructure) ==
           0);
    uint32_t elementCount =
        math::lossless_numeric_cast<uint32_t>(bufferSizeInBytes) /
        gpu::StorageBufferElementSizeInBytes(bufferStructure);
    uint32_t height =
        (elementCount + STORAGE_TEXTURE_WIDTH - 1) / STORAGE_TEXTURE_WIDTH;
    // RenderContext is responsible for breaking up a flush before any storage
    // buffer grows larger than can be supported by a GL texture of width
    // "STORAGE_TEXTURE_WIDTH". (2048 is the min required value for
    // GL_MAX_TEXTURE_SIZE.)
    constexpr int kMaxRequredTextureHeight RIVE_MAYBE_UNUSED = 2048;
    assert(height <= kMaxRequredTextureHeight);
    uint32_t width = std::min<uint32_t>(elementCount, STORAGE_TEXTURE_WIDTH);
    return {width, height};
}

size_t StorageTextureBufferSize(size_t bufferSizeInBytes,
                                StorageBufferStructure bufferStructure)
{
    // The polyfill texture needs to be updated in entire rows at a time. Extend
    // the buffer's length to be able to service a worst-case scenario.
    return bufferSizeInBytes +
           (STORAGE_TEXTURE_WIDTH - 1) *
               gpu::StorageBufferElementSizeInBytes(bufferStructure);
}

float find_transformed_area(const AABB& bounds, const Mat2D& matrix)
{
    Vec2D pts[4] = {{bounds.left(), bounds.top()},
                    {bounds.right(), bounds.top()},
                    {bounds.right(), bounds.bottom()},
                    {bounds.left(), bounds.bottom()}};
    Vec2D screenSpacePts[4];
    matrix.mapPoints(screenSpacePts, pts, 4);
    Vec2D v[3] = {screenSpacePts[1] - screenSpacePts[0],
                  screenSpacePts[2] - screenSpacePts[0],
                  screenSpacePts[3] - screenSpacePts[0]};
    return (fabsf(Vec2D::cross(v[0], v[1])) + fabsf(Vec2D::cross(v[1], v[2]))) *
           .5f;
}

// Code to generate g_gaussianIntegralTableF16.
#if 0
static float eval_normal_distribution(float x, float mu, float inverseSigma)
{
    constexpr static float ONE_OVER_SQRT_2_PI = 0.398942280401433f;
    float y = (x - mu) * inverseSigma;
    return expf(-.5 * y * y) * inverseSigma * ONE_OVER_SQRT_2_PI;
}

void generate_gausian_integral_table(float table[], size_t tableSize)
{
    float sigma = tableSize / (FEATHER_TEXTURE_STDDEVS * 2);
    float inverseSigma = 1 / sigma;
    float mu = tableSize * .5f;
    float integral = 0;
    for (size_t i = 0; i < tableSize; ++i)
    {
        // Sample the normal distribution in multiple locations for each entry
        // of the table, in order to get a more accurate integral.
        constexpr static int SAMPLES = 7;
        float barCenterX = static_cast<float>(i);
        for (int sample = 0; sample < SAMPLES; ++sample)
        {
            float dx = static_cast<float>(sample - (SAMPLES >> 1)) / SAMPLES;
            integral +=
                eval_normal_distribution(barCenterX + dx, mu, inverseSigma) /
                SAMPLES;
        }
        table[i] = integral;
    }
    // Account for the area under the curve prior to our table by shifting so
    // the middle value of the table is exactly 1/2.
    float shift =
        .5 - ((tableSize & 1)
                  ? table[tableSize / 2]
                  : (table[tableSize / 2 - 1] + table[tableSize / 2]) / 2);
    table[0] = fminf(fmaxf(0, table[0] + shift), 1);
    for (size_t i = 1; i < tableSize; ++i)
    {
        table[i] = fminf(fmaxf(table[i - 1], table[i] + shift), 1);
    }
}
#endif

const uint16_t g_gaussianIntegralTableF16[GAUSSIAN_TABLE_SIZE] = {
    0x15a3, 0x15db, 0x1616, 0x1652, 0x1691, 0x16d1, 0x1715, 0x175a, 0x17a2,
    0x17ec, 0x181c, 0x1844, 0x186d, 0x1898, 0x18c4, 0x18f1, 0x1920, 0x1951,
    0x1983, 0x19b7, 0x19ec, 0x1a23, 0x1a5d, 0x1a98, 0x1ad4, 0x1b13, 0x1b54,
    0x1b97, 0x1bdc, 0x1c12, 0x1c36, 0x1c5c, 0x1c83, 0x1cac, 0x1cd5, 0x1d00,
    0x1d2c, 0x1d5a, 0x1d89, 0x1db9, 0x1deb, 0x1e1e, 0x1e53, 0x1e89, 0x1ec1,
    0x1efb, 0x1f36, 0x1f73, 0x1fb2, 0x1ff3, 0x201b, 0x203d, 0x2060, 0x2084,
    0x20a9, 0x20d0, 0x20f7, 0x211f, 0x2149, 0x2173, 0x219f, 0x21cc, 0x21fb,
    0x222a, 0x225b, 0x228d, 0x22c0, 0x22f5, 0x232b, 0x2363, 0x239c, 0x23d6,
    0x2409, 0x2428, 0x2447, 0x2468, 0x2489, 0x24ab, 0x24cd, 0x24f1, 0x2516,
    0x253b, 0x2561, 0x2589, 0x25b1, 0x25da, 0x2604, 0x262f, 0x265b, 0x2688,
    0x26b7, 0x26e6, 0x2716, 0x2748, 0x277a, 0x27ae, 0x27e3, 0x280c, 0x2828,
    0x2844, 0x2861, 0x287e, 0x289c, 0x28bb, 0x28da, 0x28fa, 0x291b, 0x293c,
    0x295f, 0x2981, 0x29a5, 0x29c9, 0x29ee, 0x2a13, 0x2a3a, 0x2a61, 0x2a89,
    0x2ab1, 0x2adb, 0x2b05, 0x2b30, 0x2b5c, 0x2b89, 0x2bb6, 0x2be4, 0x2c0a,
    0x2c22, 0x2c3a, 0x2c53, 0x2c6c, 0x2c86, 0x2ca0, 0x2cbb, 0x2cd6, 0x2cf2,
    0x2d0e, 0x2d2a, 0x2d47, 0x2d65, 0x2d82, 0x2da1, 0x2dc0, 0x2ddf, 0x2dff,
    0x2e1f, 0x2e40, 0x2e62, 0x2e84, 0x2ea6, 0x2ec9, 0x2eec, 0x2f10, 0x2f35,
    0x2f5a, 0x2f7f, 0x2fa5, 0x2fcc, 0x2ff3, 0x300d, 0x3021, 0x3036, 0x304a,
    0x305f, 0x3074, 0x308a, 0x309f, 0x30b5, 0x30cc, 0x30e2, 0x30f9, 0x3110,
    0x3127, 0x313f, 0x3157, 0x316f, 0x3187, 0x31a0, 0x31b9, 0x31d2, 0x31eb,
    0x3205, 0x321f, 0x323a, 0x3254, 0x326f, 0x328a, 0x32a5, 0x32c1, 0x32dd,
    0x32f9, 0x3315, 0x3332, 0x334f, 0x336c, 0x338a, 0x33a7, 0x33c5, 0x33e3,
    0x3401, 0x3410, 0x3420, 0x342f, 0x343f, 0x344f, 0x345f, 0x346f, 0x347f,
    0x348f, 0x349f, 0x34b0, 0x34c0, 0x34d1, 0x34e2, 0x34f3, 0x3504, 0x3515,
    0x3526, 0x3537, 0x3548, 0x355a, 0x356b, 0x357d, 0x358f, 0x35a0, 0x35b2,
    0x35c4, 0x35d6, 0x35e8, 0x35fa, 0x360d, 0x361f, 0x3631, 0x3644, 0x3656,
    0x3669, 0x367b, 0x368e, 0x36a0, 0x36b3, 0x36c6, 0x36d9, 0x36ec, 0x36ff,
    0x3711, 0x3724, 0x3737, 0x374a, 0x375d, 0x3771, 0x3784, 0x3797, 0x37aa,
    0x37bd, 0x37d0, 0x37e3, 0x37f6, 0x3805, 0x380e, 0x3818, 0x3822, 0x382b,
    0x3835, 0x383e, 0x3848, 0x3851, 0x385b, 0x3864, 0x386e, 0x3877, 0x3881,
    0x388a, 0x3894, 0x389d, 0x38a6, 0x38b0, 0x38b9, 0x38c2, 0x38cc, 0x38d5,
    0x38de, 0x38e7, 0x38f1, 0x38fa, 0x3903, 0x390c, 0x3915, 0x391e, 0x3927,
    0x3930, 0x3939, 0x3942, 0x394a, 0x3953, 0x395c, 0x3964, 0x396d, 0x3976,
    0x397e, 0x3987, 0x398f, 0x3998, 0x39a0, 0x39a8, 0x39b0, 0x39b9, 0x39c1,
    0x39c9, 0x39d1, 0x39d9, 0x39e1, 0x39e8, 0x39f0, 0x39f8, 0x3a00, 0x3a07,
    0x3a0f, 0x3a16, 0x3a1e, 0x3a25, 0x3a2c, 0x3a33, 0x3a3b, 0x3a42, 0x3a49,
    0x3a50, 0x3a57, 0x3a5d, 0x3a64, 0x3a6b, 0x3a72, 0x3a78, 0x3a7f, 0x3a85,
    0x3a8b, 0x3a92, 0x3a98, 0x3a9e, 0x3aa4, 0x3aaa, 0x3ab0, 0x3ab6, 0x3abc,
    0x3ac2, 0x3ac7, 0x3acd, 0x3ad3, 0x3ad8, 0x3ade, 0x3ae3, 0x3ae8, 0x3aed,
    0x3af3, 0x3af8, 0x3afd, 0x3b02, 0x3b07, 0x3b0b, 0x3b10, 0x3b15, 0x3b19,
    0x3b1e, 0x3b22, 0x3b27, 0x3b2b, 0x3b30, 0x3b34, 0x3b38, 0x3b3c, 0x3b40,
    0x3b44, 0x3b48, 0x3b4c, 0x3b50, 0x3b53, 0x3b57, 0x3b5b, 0x3b5e, 0x3b62,
    0x3b65, 0x3b69, 0x3b6c, 0x3b6f, 0x3b72, 0x3b76, 0x3b79, 0x3b7c, 0x3b7f,
    0x3b82, 0x3b85, 0x3b87, 0x3b8a, 0x3b8d, 0x3b90, 0x3b92, 0x3b95, 0x3b97,
    0x3b9a, 0x3b9c, 0x3b9f, 0x3ba1, 0x3ba3, 0x3ba6, 0x3ba8, 0x3baa, 0x3bac,
    0x3bae, 0x3bb0, 0x3bb2, 0x3bb4, 0x3bb6, 0x3bb8, 0x3bba, 0x3bbc, 0x3bbe,
    0x3bbf, 0x3bc1, 0x3bc3, 0x3bc4, 0x3bc6, 0x3bc7, 0x3bc9, 0x3bca, 0x3bcc,
    0x3bcd, 0x3bcf, 0x3bd0, 0x3bd1, 0x3bd2, 0x3bd4, 0x3bd5, 0x3bd6, 0x3bd7,
    0x3bd8, 0x3bda, 0x3bdb, 0x3bdc, 0x3bdd, 0x3bde, 0x3bdf, 0x3be0, 0x3be1,
    0x3be2, 0x3be2, 0x3be3, 0x3be4, 0x3be5, 0x3be6, 0x3be7, 0x3be7, 0x3be8,
    0x3be9, 0x3bea, 0x3bea, 0x3beb, 0x3bec, 0x3bec, 0x3bed, 0x3bed, 0x3bee,
    0x3bee, 0x3bef, 0x3bf0, 0x3bf0, 0x3bf1, 0x3bf1, 0x3bf2, 0x3bf2, 0x3bf2,
    0x3bf3, 0x3bf3, 0x3bf4, 0x3bf4, 0x3bf5, 0x3bf5, 0x3bf5, 0x3bf6, 0x3bf6,
    0x3bf6, 0x3bf7, 0x3bf7, 0x3bf7, 0x3bf8, 0x3bf8, 0x3bf8, 0x3bf8, 0x3bf9,
    0x3bf9, 0x3bf9, 0x3bf9, 0x3bfa, 0x3bfa, 0x3bfa, 0x3bfa, 0x3bfa, 0x3bfb,
    0x3bfb, 0x3bfb, 0x3bfb, 0x3bfb, 0x3bfc, 0x3bfc, 0x3bfc, 0x3bfc, 0x3bfc,
    0x3bfc, 0x3bfc, 0x3bfd, 0x3bfd, 0x3bfd, 0x3bfd, 0x3bfd, 0x3bfd,
};

// Code to generate g_inverseGaussianIntegralTableF32.
#if 0
void generate_inverse_gausian_integral_table(float table[], size_t tableSize)
{
    // Evaluate 32 samples for every table value, for better precision.
    size_t MULTIPLIER = 32;
    float sigma = tableSize / (FEATHER_TEXTURE_STDDEVS * 2);
    float inverseSigma = 1 / sigma;
    float mu = tableSize * .5f;
    size_t samples = tableSize * MULTIPLIER;

    // Integrate half the curve in order to determine the initial value of our
    // integral (the table doesn't begin until -FEATHER_TEXTURE_STDDEVS).
    float integral = 0;
    for (size_t i = 0; i < (samples + 1) / 2; ++i)
    {
        float barCenterX = static_cast<float>(i) / MULTIPLIER;
        integral +=
            eval_normal_distribution(barCenterX, mu, inverseSigma) / MULTIPLIER;
    }
    integral = .5 - integral;

    // Reboot now that we know the initial integral value and fill in the
    // inverse table this time around.
    float lastInverseX = std::numeric_limits<float>::quiet_NaN(),
          lastInverseY = 0;
    table[0] = 0;
    table[tableSize - 1] = 1;
    for (size_t i = 0; i < samples; ++i)
    {
        float barCenterX = static_cast<float>(i) / MULTIPLIER;
        integral +=
            eval_normal_distribution(barCenterX, mu, inverseSigma) / MULTIPLIER;
        float inverseX = fminf(fmaxf(0, integral), 1) * tableSize;
        float inverseY = (i + .5f) / samples;
        size_t cell = static_cast<size_t>(inverseX);
        float cellCenterX = cell + .5f;
        if (cellCenterX == mu)
        {
            // Make sure the center value is exactly .5, just because.
            table[cell] = .5f;
        }
        else if (lastInverseX <= cellCenterX && inverseX >= cellCenterX)
        {
            float t = (cellCenterX - lastInverseX) / (inverseX - lastInverseX);
            float y = lerp(lastInverseY, inverseY, t);
            assert(0 <= cell && cell < tableSize);
            table[cell] = y;
        }
        lastInverseX = inverseX;
        lastInverseY = inverseY;
    }

    // Use a large enough tableSize that the beginning and ending values are 0
    // and 1!
    assert(table[0] == 0 && table[tableSize - 1] == 1);
}
#endif

const float g_inverseGaussianIntegralTableF32[GAUSSIAN_TABLE_SIZE] = {
    0.000000f, 0.039369f, 0.068465f, 0.088398f, 0.103756f, 0.116343f, 0.127060f,
    0.136427f, 0.144769f, 0.152305f, 0.159192f, 0.165541f, 0.171439f, 0.176952f,
    0.182133f, 0.187024f, 0.191659f, 0.196068f, 0.200273f, 0.204296f, 0.208153f,
    0.211861f, 0.215431f, 0.218875f, 0.222203f, 0.225423f, 0.228545f, 0.231574f,
    0.234516f, 0.237379f, 0.240166f, 0.242882f, 0.245531f, 0.248118f, 0.250645f,
    0.253116f, 0.255534f, 0.257902f, 0.260222f, 0.262496f, 0.264727f, 0.266917f,
    0.269067f, 0.271179f, 0.273256f, 0.275297f, 0.277306f, 0.279282f, 0.281228f,
    0.283145f, 0.285033f, 0.286894f, 0.288729f, 0.290539f, 0.292324f, 0.294086f,
    0.295825f, 0.297542f, 0.299238f, 0.300914f, 0.302569f, 0.304206f, 0.305823f,
    0.307423f, 0.309005f, 0.310570f, 0.312118f, 0.313651f, 0.315167f, 0.316669f,
    0.318156f, 0.319628f, 0.321087f, 0.322532f, 0.323964f, 0.325383f, 0.326789f,
    0.328183f, 0.329566f, 0.330936f, 0.332296f, 0.333644f, 0.334981f, 0.336308f,
    0.337625f, 0.338931f, 0.340228f, 0.341515f, 0.342793f, 0.344061f, 0.345321f,
    0.346572f, 0.347814f, 0.349048f, 0.350274f, 0.351491f, 0.352701f, 0.353903f,
    0.355097f, 0.356284f, 0.357464f, 0.358637f, 0.359802f, 0.360961f, 0.362114f,
    0.363259f, 0.364399f, 0.365532f, 0.366658f, 0.367779f, 0.368894f, 0.370003f,
    0.371106f, 0.372203f, 0.373296f, 0.374382f, 0.375464f, 0.376540f, 0.377611f,
    0.378677f, 0.379738f, 0.380794f, 0.381845f, 0.382892f, 0.383934f, 0.384972f,
    0.386005f, 0.387034f, 0.388058f, 0.389079f, 0.390095f, 0.391107f, 0.392115f,
    0.393119f, 0.394120f, 0.395116f, 0.396109f, 0.397098f, 0.398083f, 0.399065f,
    0.400044f, 0.401019f, 0.401990f, 0.402959f, 0.403924f, 0.404886f, 0.405844f,
    0.406800f, 0.407753f, 0.408702f, 0.409649f, 0.410592f, 0.411533f, 0.412471f,
    0.413406f, 0.414339f, 0.415269f, 0.416196f, 0.417121f, 0.418043f, 0.418962f,
    0.419879f, 0.420794f, 0.421706f, 0.422616f, 0.423524f, 0.424429f, 0.425333f,
    0.426234f, 0.427133f, 0.428029f, 0.428924f, 0.429817f, 0.430707f, 0.431596f,
    0.432482f, 0.433367f, 0.434250f, 0.435131f, 0.436011f, 0.436888f, 0.437764f,
    0.438638f, 0.439510f, 0.440381f, 0.441250f, 0.442117f, 0.442983f, 0.443848f,
    0.444711f, 0.445572f, 0.446432f, 0.447290f, 0.448147f, 0.449003f, 0.449858f,
    0.450711f, 0.451562f, 0.452413f, 0.453262f, 0.454110f, 0.454957f, 0.455803f,
    0.456648f, 0.457491f, 0.458333f, 0.459175f, 0.460015f, 0.460854f, 0.461693f,
    0.462530f, 0.463366f, 0.464202f, 0.465036f, 0.465870f, 0.466703f, 0.467535f,
    0.468366f, 0.469196f, 0.470026f, 0.470855f, 0.471683f, 0.472511f, 0.473337f,
    0.474164f, 0.474989f, 0.475814f, 0.476638f, 0.477462f, 0.478285f, 0.479108f,
    0.479930f, 0.480752f, 0.481573f, 0.482394f, 0.483214f, 0.484034f, 0.484853f,
    0.485673f, 0.486491f, 0.487310f, 0.488128f, 0.488946f, 0.489764f, 0.490581f,
    0.491398f, 0.492215f, 0.493032f, 0.493848f, 0.494665f, 0.495481f, 0.496297f,
    0.497113f, 0.497930f, 0.498746f, 0.499561f, 0.500377f, 0.501193f, 0.502009f,
    0.502825f, 0.503641f, 0.504458f, 0.505274f, 0.506090f, 0.506907f, 0.507724f,
    0.508541f, 0.509358f, 0.510175f, 0.510993f, 0.511811f, 0.512629f, 0.513447f,
    0.514266f, 0.515085f, 0.515905f, 0.516725f, 0.517545f, 0.518366f, 0.519187f,
    0.520008f, 0.520831f, 0.521653f, 0.522476f, 0.523300f, 0.524124f, 0.524949f,
    0.525775f, 0.526601f, 0.527427f, 0.528255f, 0.529083f, 0.529912f, 0.530741f,
    0.531572f, 0.532403f, 0.533235f, 0.534068f, 0.534901f, 0.535736f, 0.536571f,
    0.537407f, 0.538245f, 0.539083f, 0.539922f, 0.540762f, 0.541603f, 0.542446f,
    0.543289f, 0.544134f, 0.544979f, 0.545826f, 0.546674f, 0.547523f, 0.548374f,
    0.549225f, 0.550078f, 0.550933f, 0.551788f, 0.552645f, 0.553504f, 0.554364f,
    0.555225f, 0.556088f, 0.556952f, 0.557818f, 0.558685f, 0.559554f, 0.560424f,
    0.561297f, 0.562171f, 0.563046f, 0.563924f, 0.564803f, 0.565684f, 0.566566f,
    0.567451f, 0.568338f, 0.569226f, 0.570117f, 0.571009f, 0.571904f, 0.572800f,
    0.573699f, 0.574600f, 0.575503f, 0.576408f, 0.577315f, 0.578225f, 0.579137f,
    0.580052f, 0.580969f, 0.581888f, 0.582810f, 0.583735f, 0.584662f, 0.585591f,
    0.586524f, 0.587459f, 0.588396f, 0.589337f, 0.590280f, 0.591227f, 0.592176f,
    0.593129f, 0.594084f, 0.595042f, 0.596004f, 0.596969f, 0.597937f, 0.598908f,
    0.599883f, 0.600861f, 0.601843f, 0.602828f, 0.603817f, 0.604809f, 0.605806f,
    0.606806f, 0.607810f, 0.608817f, 0.609829f, 0.610845f, 0.611865f, 0.612889f,
    0.613918f, 0.614951f, 0.615988f, 0.617030f, 0.618076f, 0.619127f, 0.620183f,
    0.621244f, 0.622310f, 0.623380f, 0.624456f, 0.625537f, 0.626623f, 0.627715f,
    0.628812f, 0.629915f, 0.631024f, 0.632138f, 0.633258f, 0.634385f, 0.635517f,
    0.636656f, 0.637801f, 0.638953f, 0.640111f, 0.641276f, 0.642449f, 0.643628f,
    0.644814f, 0.646008f, 0.647210f, 0.648419f, 0.649636f, 0.650861f, 0.652094f,
    0.653336f, 0.654586f, 0.655845f, 0.657112f, 0.658390f, 0.659676f, 0.660972f,
    0.662278f, 0.663594f, 0.664920f, 0.666256f, 0.667604f, 0.668962f, 0.670332f,
    0.671713f, 0.673107f, 0.674512f, 0.675930f, 0.677361f, 0.678805f, 0.680263f,
    0.681734f, 0.683220f, 0.684721f, 0.686236f, 0.687767f, 0.689315f, 0.690878f,
    0.692459f, 0.694057f, 0.695674f, 0.697308f, 0.698963f, 0.700637f, 0.702331f,
    0.704046f, 0.705784f, 0.707544f, 0.709328f, 0.711136f, 0.712969f, 0.714828f,
    0.716714f, 0.718629f, 0.720573f, 0.722547f, 0.724553f, 0.726592f, 0.728666f,
    0.730776f, 0.732923f, 0.735109f, 0.737337f, 0.739608f, 0.741925f, 0.744289f,
    0.746703f, 0.749171f, 0.751694f, 0.754276f, 0.756921f, 0.759632f, 0.762413f,
    0.765270f, 0.768206f, 0.771228f, 0.774343f, 0.777556f, 0.780876f, 0.784311f,
    0.787871f, 0.791568f, 0.795413f, 0.799423f, 0.803614f, 0.808006f, 0.812624f,
    0.817495f, 0.822653f, 0.828140f, 0.834008f, 0.840321f, 0.847164f, 0.854648f,
    0.862924f, 0.872203f, 0.882806f, 0.895230f, 0.910338f, 0.929832f, 0.957939f,
    1.000000f,
};

float gaussian_table_lookup(const float (&table)[GAUSSIAN_TABLE_SIZE], float x)
{
    x = fminf(fmaxf(0, x), 1);
    float sampleBoxLeft = x * GAUSSIAN_TABLE_SIZE - .5f;
    int rightIdx =
        static_cast<int>(fminf(sampleBoxLeft + 1, GAUSSIAN_TABLE_SIZE - 1));
    int leftIdx = std::max(rightIdx - 1, 0);
    float t = fminf(fmaxf(0, sampleBoxLeft - leftIdx), 1);
    return lerp(table[leftIdx], table[rightIdx], t);
}
} // namespace rive::gpu

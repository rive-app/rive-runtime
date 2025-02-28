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
        case DrawType::atlasBlit:
            drawTypeKey = 2;
            break;
        case DrawType::imageRect:
            drawTypeKey = 3;
            break;
        case DrawType::imageMesh:
            drawTypeKey = 4;
            break;
        case DrawType::atomicInitialize:
            assert(interlockMode == gpu::InterlockMode::atomics);
            drawTypeKey = 5;
            break;
        case DrawType::atomicResolve:
            assert(interlockMode == gpu::InterlockMode::atomics);
            drawTypeKey = 6;
            break;
        case DrawType::stencilClipReset:
            assert(interlockMode == gpu::InterlockMode::msaa);
            drawTypeKey = 7;
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
    m_atlasTextureInverseSize(1.f / flushDesc.atlasTextureWidth,
                              1.f / flushDesc.atlasTextureHeight),
    m_atlasContentInverseViewport(2.f / flushDesc.atlasContentWidth,
                                  (platformFeatures.clipSpaceBottomUp !=
                                           platformFeatures.framebufferBottomUp
                                       ? -2.f
                                       : 2.f) /
                                      flushDesc.atlasContentHeight),
    m_coverageBufferPrefix(flushDesc.coverageBufferPrefix),
    m_pathIDGranularity(platformFeatures.pathIDGranularity),
    m_vertexDiscardValue(std::numeric_limits<float>::quiet_NaN()),
    m_wireframeEnabled(flushDesc.wireframe)
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
                   const AtlasTransform& atlasTransform,
                   const CoverageBufferRange& coverageBufferRange)
{
    write_matrix(m_matrix, m);
    m_strokeRadius = strokeRadius; // 0 if the path is filled.
    m_zIndex = zIndex;
    m_featherRadius = featherRadius;
    m_atlasTransform.scaleFactor = atlasTransform.scaleFactor;
    m_atlasTransform.translateX = atlasTransform.translateX;
    m_atlasTransform.translateY = atlasTransform.translateY;
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

// Borrowed from:
// https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion
//
// IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15,
// +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
float4 cast_f16_to_f32(uint16x4 x16)
{
    uint4 x = simd::cast<uint32_t>(x16);
    uint4 e = (x & 0x7C00) >> 10; // exponent
    uint4 m = (x & 0x03FF) << 13; // mantissa
    // evil log2 bit hack to count leading zeros in denormalized format
    uint4 v = math::bit_cast<uint4>(simd::cast<float>(m)) >> 23;
    // sign : normalized : denormalized
    return math::bit_cast<float4>(
        (x & 0x8000u) << 16 |
        simd::cast<uint32_t>((e != 0u) & 1) * ((e + 112) << 23 | m) |
        simd::cast<uint32_t>((e == 0u) & (m != 0u) & 1) *
            ((v - 37u) << 23 | ((m << (150u - v)) & 0x007FE000u)));
}

// Borrowed from:
// https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion
//
// IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15,
// +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
uint16x4 cast_f32_to_f16(float4 x)
{
    // round-to-nearest-even: add last bit after truncated mantissa
    uint4 b = math::bit_cast<uint4>(x) + 0x00001000;
    uint4 e = (b & 0x7F800000) >> 23; // exponent
    // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal
    // indicator flag - initial rounding
    uint4 m = b & 0x007FFFFF;
    // sign : normalized : denormalized : saturate
    return simd::cast<uint16_t>(
        ((b & 0x80000000u) >> 16) |
        simd::cast<uint32_t>((e > 112u) & 1) *
            ((((e - 112u) << 10) & 0x7C00u) | m >> 13) |
        simd::cast<uint32_t>((e < 113u) & (e > 101u) & 1) *
            ((((0x007FF000u + m) >> (125u - e)) + 1u) >> 1) |
        simd::cast<uint32_t>((e > 143u) & 1) * 0x7FFFu);
}

// Code to generate g_gaussianIntegralTableF16.
#if 0
static float eval_normal_distribution(float x, float mu, float inverseSigma)
{
    constexpr static float ONE_OVER_SQRT_2_PI = 0.398942280401433f;
    float y = (x - mu) * inverseSigma;
    return expf(-.5 * y * y) * inverseSigma * ONE_OVER_SQRT_2_PI;
}

void generate_gausian_integral_table(float (&table)[GAUSSIAN_TABLE_SIZE])
{
    float sigma = GAUSSIAN_TABLE_SIZE / (FEATHER_TEXTURE_STDDEVS * 2);
    float inverseSigma = 1 / sigma;
    float mu = GAUSSIAN_TABLE_SIZE * .5f;
    float integral = 0;
    for (size_t i = 0; i < GAUSSIAN_TABLE_SIZE; ++i)
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
        .5 - ((GAUSSIAN_TABLE_SIZE & 1) ? table[GAUSSIAN_TABLE_SIZE / 2]
                                        : (table[GAUSSIAN_TABLE_SIZE / 2 - 1] +
                                           table[GAUSSIAN_TABLE_SIZE / 2]) /
                                              2);
    table[0] = fminf(fmaxf(0, table[0] + shift), 1);
    for (size_t i = 1; i < GAUSSIAN_TABLE_SIZE; ++i)
    {
        table[i] = fminf(fmaxf(table[i - 1], table[i] + shift), 1);
    }
    printf("\nconst float g_gaussianIntegralTableF16[GAUSSIAN_TABLE_SIZE] = "
           "{\n");
    for (size_t i = 0; i < GAUSSIAN_TABLE_SIZE; ++i)
    {
        printf("%f, ", table[i]);
    }
    printf("\n};\n");
    printf("\nconst uint16_t g_gaussianIntegralTableF16[GAUSSIAN_TABLE_SIZE] = "
           "{\n");
    for (size_t i = 0; i < GAUSSIAN_TABLE_SIZE; ++i)
    {
        printf("0x%x, ", gpu::cast_f32_to_f16(table[i]).x);
    }
    printf("\n};\n");
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
void generate_inverse_gausian_integral_table(
    float (&table)[GAUSSIAN_TABLE_SIZE])
{
    // Evaluate 32 samples for every table value, for better precision.
    size_t MULTIPLIER = 32;
    float sigma = GAUSSIAN_TABLE_SIZE / (FEATHER_TEXTURE_STDDEVS * 2);
    float inverseSigma = 1 / sigma;
    float mu = GAUSSIAN_TABLE_SIZE * .5f;
    size_t samples = GAUSSIAN_TABLE_SIZE * MULTIPLIER;

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
    table[GAUSSIAN_TABLE_SIZE - 1] = 1;
    for (size_t i = 0; i < samples; ++i)
    {
        float barCenterX = static_cast<float>(i) / MULTIPLIER;
        integral +=
            eval_normal_distribution(barCenterX, mu, inverseSigma) / MULTIPLIER;
        float inverseX = fminf(fmaxf(0, integral), 1) * GAUSSIAN_TABLE_SIZE;
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
            assert(0 <= cell && cell < GAUSSIAN_TABLE_SIZE);
            table[cell] = y;
        }
        lastInverseX = inverseX;
        lastInverseY = inverseY;
    }

    // Use a large enough GAUSSIAN_TABLE_SIZE that the beginning and ending
    // values are 0 and 1!
    assert(table[0] == 0 && table[GAUSSIAN_TABLE_SIZE - 1] == 1);

    printf("\nconst float "
           "g_inverseGaussianIntegralTableF32[GAUSSIAN_TABLE_SIZE] = {\n");
    for (size_t i = 0; i < GAUSSIAN_TABLE_SIZE; ++i)
    {
        printf("%ff, ", table[i]);
    }
    printf("\n};\n");
    printf("\nconst uint16_t "
           "g_inverseGaussianIntegralTableF16[GAUSSIAN_TABLE_SIZE] = "
           "{\n");
    for (size_t i = 0; i < GAUSSIAN_TABLE_SIZE; ++i)
    {
        printf("0x%x, ", gpu::cast_f32_to_f16(table[i]).x);
    }
    printf("\n};\n");
}
#endif

const uint16_t g_inverseGaussianIntegralTableF16[GAUSSIAN_TABLE_SIZE] = {
    0x0,    0x290a, 0x2c62, 0x2da8, 0x2ea4, 0x2f72, 0x3011, 0x305e, 0x30a2,
    0x30e0, 0x3118, 0x314c, 0x317c, 0x31aa, 0x31d4, 0x31fc, 0x3222, 0x3246,
    0x3269, 0x328a, 0x32a9, 0x32c8, 0x32e5, 0x3301, 0x331c, 0x3337, 0x3350,
    0x3369, 0x3381, 0x3399, 0x33af, 0x33c6, 0x33db, 0x33f1, 0x3403, 0x340d,
    0x3417, 0x3420, 0x342a, 0x3433, 0x343c, 0x3445, 0x344e, 0x3457, 0x345f,
    0x3468, 0x3470, 0x3478, 0x3480, 0x3488, 0x348f, 0x3497, 0x349f, 0x34a6,
    0x34ad, 0x34b5, 0x34bc, 0x34c3, 0x34ca, 0x34d1, 0x34d7, 0x34de, 0x34e5,
    0x34eb, 0x34f2, 0x34f8, 0x34fe, 0x3505, 0x350b, 0x3511, 0x3517, 0x351d,
    0x3523, 0x3529, 0x352f, 0x3535, 0x353b, 0x3540, 0x3546, 0x354c, 0x3551,
    0x3557, 0x355c, 0x3562, 0x3567, 0x356c, 0x3572, 0x3577, 0x357c, 0x3581,
    0x3586, 0x358c, 0x3591, 0x3596, 0x359b, 0x35a0, 0x35a5, 0x35aa, 0x35ae,
    0x35b3, 0x35b8, 0x35bd, 0x35c2, 0x35c6, 0x35cb, 0x35d0, 0x35d5, 0x35d9,
    0x35de, 0x35e2, 0x35e7, 0x35ec, 0x35f0, 0x35f5, 0x35f9, 0x35fd, 0x3602,
    0x3606, 0x360b, 0x360f, 0x3613, 0x3618, 0x361c, 0x3620, 0x3625, 0x3629,
    0x362d, 0x3631, 0x3635, 0x363a, 0x363e, 0x3642, 0x3646, 0x364a, 0x364e,
    0x3652, 0x3656, 0x365b, 0x365f, 0x3663, 0x3667, 0x366b, 0x366f, 0x3673,
    0x3676, 0x367a, 0x367e, 0x3682, 0x3686, 0x368a, 0x368e, 0x3692, 0x3696,
    0x3699, 0x369d, 0x36a1, 0x36a5, 0x36a9, 0x36ad, 0x36b0, 0x36b4, 0x36b8,
    0x36bc, 0x36bf, 0x36c3, 0x36c7, 0x36ca, 0x36ce, 0x36d2, 0x36d6, 0x36d9,
    0x36dd, 0x36e1, 0x36e4, 0x36e8, 0x36eb, 0x36ef, 0x36f3, 0x36f6, 0x36fa,
    0x36fd, 0x3701, 0x3705, 0x3708, 0x370c, 0x370f, 0x3713, 0x3716, 0x371a,
    0x371e, 0x3721, 0x3725, 0x3728, 0x372c, 0x372f, 0x3733, 0x3736, 0x373a,
    0x373d, 0x3741, 0x3744, 0x3748, 0x374b, 0x374e, 0x3752, 0x3755, 0x3759,
    0x375c, 0x3760, 0x3763, 0x3767, 0x376a, 0x376d, 0x3771, 0x3774, 0x3778,
    0x377b, 0x377e, 0x3782, 0x3785, 0x3789, 0x378c, 0x378f, 0x3793, 0x3796,
    0x379a, 0x379d, 0x37a0, 0x37a4, 0x37a7, 0x37aa, 0x37ae, 0x37b1, 0x37b5,
    0x37b8, 0x37bb, 0x37bf, 0x37c2, 0x37c5, 0x37c9, 0x37cc, 0x37cf, 0x37d3,
    0x37d6, 0x37d9, 0x37dd, 0x37e0, 0x37e3, 0x37e7, 0x37ea, 0x37ed, 0x37f1,
    0x37f4, 0x37f8, 0x37fb, 0x37fe, 0x3801, 0x3802, 0x3804, 0x3806, 0x3807,
    0x3809, 0x380b, 0x380c, 0x380e, 0x3810, 0x3811, 0x3813, 0x3815, 0x3817,
    0x3818, 0x381a, 0x381c, 0x381d, 0x381f, 0x3821, 0x3822, 0x3824, 0x3826,
    0x3827, 0x3829, 0x382b, 0x382c, 0x382e, 0x3830, 0x3831, 0x3833, 0x3835,
    0x3836, 0x3838, 0x383a, 0x383c, 0x383d, 0x383f, 0x3841, 0x3842, 0x3844,
    0x3846, 0x3847, 0x3849, 0x384b, 0x384d, 0x384e, 0x3850, 0x3852, 0x3853,
    0x3855, 0x3857, 0x3859, 0x385a, 0x385c, 0x385e, 0x3860, 0x3861, 0x3863,
    0x3865, 0x3867, 0x3868, 0x386a, 0x386c, 0x386e, 0x386f, 0x3871, 0x3873,
    0x3875, 0x3876, 0x3878, 0x387a, 0x387c, 0x387e, 0x387f, 0x3881, 0x3883,
    0x3885, 0x3887, 0x3888, 0x388a, 0x388c, 0x388e, 0x3890, 0x3891, 0x3893,
    0x3895, 0x3897, 0x3899, 0x389b, 0x389c, 0x389e, 0x38a0, 0x38a2, 0x38a4,
    0x38a6, 0x38a8, 0x38aa, 0x38ab, 0x38ad, 0x38af, 0x38b1, 0x38b3, 0x38b5,
    0x38b7, 0x38b9, 0x38bb, 0x38bd, 0x38bf, 0x38c1, 0x38c3, 0x38c5, 0x38c7,
    0x38c9, 0x38cb, 0x38cd, 0x38cf, 0x38d1, 0x38d3, 0x38d5, 0x38d7, 0x38d9,
    0x38db, 0x38dd, 0x38df, 0x38e1, 0x38e3, 0x38e5, 0x38e7, 0x38e9, 0x38eb,
    0x38ee, 0x38f0, 0x38f2, 0x38f4, 0x38f6, 0x38f8, 0x38fa, 0x38fd, 0x38ff,
    0x3901, 0x3903, 0x3906, 0x3908, 0x390a, 0x390c, 0x390f, 0x3911, 0x3913,
    0x3916, 0x3918, 0x391a, 0x391d, 0x391f, 0x3921, 0x3924, 0x3926, 0x3929,
    0x392b, 0x392d, 0x3930, 0x3932, 0x3935, 0x3937, 0x393a, 0x393d, 0x393f,
    0x3942, 0x3944, 0x3947, 0x394a, 0x394c, 0x394f, 0x3952, 0x3954, 0x3957,
    0x395a, 0x395d, 0x3960, 0x3963, 0x3965, 0x3968, 0x396b, 0x396e, 0x3971,
    0x3974, 0x3977, 0x397a, 0x397d, 0x3981, 0x3984, 0x3987, 0x398a, 0x398d,
    0x3991, 0x3994, 0x3997, 0x399b, 0x399e, 0x39a2, 0x39a5, 0x39a9, 0x39ad,
    0x39b0, 0x39b4, 0x39b8, 0x39bc, 0x39c0, 0x39c4, 0x39c8, 0x39cc, 0x39d0,
    0x39d4, 0x39d9, 0x39dd, 0x39e2, 0x39e6, 0x39eb, 0x39ef, 0x39f4, 0x39f9,
    0x39fe, 0x3a03, 0x3a09, 0x3a0e, 0x3a14, 0x3a19, 0x3a1f, 0x3a25, 0x3a2b,
    0x3a32, 0x3a38, 0x3a3f, 0x3a46, 0x3a4e, 0x3a55, 0x3a5d, 0x3a65, 0x3a6e,
    0x3a77, 0x3a80, 0x3a8a, 0x3a95, 0x3aa0, 0x3aac, 0x3ab9, 0x3ac7, 0x3ad6,
    0x3ae7, 0x3afa, 0x3b10, 0x3b29, 0x3b48, 0x3b70, 0x3baa, 0x3c00,
};
} // namespace rive::gpu

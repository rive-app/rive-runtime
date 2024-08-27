/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/gpu.hpp"

#include "rive/renderer/render_target.hpp"
#include "shaders/constants.glsl"
#include "rive/renderer/image.hpp"
#include "rive_render_paint.hpp"

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
        assert(drawType == DrawType::gpuAtomicResolve);
        assert(shaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND);
        assert(interlockMode == InterlockMode::atomics);
    }
    if (miscFlags & (ShaderMiscFlags::storeColorClear | ShaderMiscFlags::swizzleColorBGRAToRGBA))
    {
        assert(drawType == DrawType::gpuAtomicInitialize);
    }
    uint32_t drawTypeKey;
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
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
        case DrawType::gpuAtomicInitialize:
            assert(interlockMode == gpu::InterlockMode::atomics);
            drawTypeKey = 4;
            break;
        case DrawType::gpuAtomicResolve:
            assert(interlockMode == gpu::InterlockMode::atomics);
            drawTypeKey = 5;
            break;
        case DrawType::stencilClipReset:
            assert(interlockMode == gpu::InterlockMode::depthStencil);
            drawTypeKey = 6;
            break;
    }
    uint32_t key = static_cast<uint32_t>(miscFlags);
    assert(static_cast<uint32_t>(interlockMode) < 1 << 2);
    key = (key << 2) | static_cast<uint32_t>(interlockMode);
    key = (key << kShaderFeatureCount) |
          (shaderFeatures & ShaderFeaturesMaskFor(drawType, interlockMode)).bits();
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
    // AA border vertices. "Inner tessellation curves" have one more segment without a fan triangle
    // whose purpose is to be a bowtie join.
    size_t vertexCount = 0;
    int32_t patchSegmentSpan = patchType == PatchType::midpointFan ? kMidpointFanPatchSegmentSpan
                                                                   : kOuterCurvePatchSegmentSpan;
    for (int32_t i = 0; i < patchSegmentSpan; ++i)
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

            // Give the vertex an alternate position when mirrored so the border has the same
            // diagonals whether morrored or not.
            vertices[vertexCount + 0].setMirroredPosition(r, 0.f, .5f);
            vertices[vertexCount + 1].setMirroredPosition(l, 0.f, .5f);
            vertices[vertexCount + 2].setMirroredPosition(r, 1.f, .0f);
            vertices[vertexCount + 3].setMirroredPosition(l, 1.f, .0f);
        }
        else
        {
            assert(patchType == PatchType::midpointFan);
            vertices[vertexCount + 0].set(l, -1.f, 1.f, params);
            vertices[vertexCount + 1].set(l, +1.f, 0.f, params);
            vertices[vertexCount + 2].set(r, -1.f, 1.f, params);
            vertices[vertexCount + 3].set(r, +1.f, 0.f, params);

            // Give the vertex an alternate position when mirrored so the border has the same
            // diagonals whether morrored or not.
            vertices[vertexCount + 0].setMirroredPosition(r - 1.f, -1.f, 1.f);
            vertices[vertexCount + 1].setMirroredPosition(l - 1.f, -1.f, 1.f);
            vertices[vertexCount + 2].setMirroredPosition(r - 1.f, +1.f, 0.f);
            vertices[vertexCount + 3].setMirroredPosition(l - 1.f, +1.f, 0.f);
        }
        vertexCount += 4;
    }

    // Bottom (negative coverage) side of the AA border.
    if (patchType == PatchType::outerCurves)
    {
        float params = pack_params(patchSegmentSpan, STROKE_VERTEX);
        for (int i = 0; i < patchSegmentSpan; ++i)
        {
            float l = static_cast<float>(i);
            float r = l + 1;

            vertices[vertexCount + 0].set(l, -.0f, .5f, params);
            vertices[vertexCount + 1].set(r, -.0f, .5f, params);
            vertices[vertexCount + 2].set(l, -1.f, .0f, params);
            vertices[vertexCount + 3].set(r, -1.f, .0f, params);

            // Give the vertex an alternate position when mirrored so the border has the same
            // diagonals whether morrored or not.
            vertices[vertexCount + 0].setMirroredPosition(r, -0.f, .5f);
            vertices[vertexCount + 1].setMirroredPosition(r, -1.f, .0f);
            vertices[vertexCount + 2].setMirroredPosition(l, -0.f, .5f);
            vertices[vertexCount + 3].setMirroredPosition(l, -1.f, .0f);

            vertexCount += 4;
        }
    }

    // Triangle fan vertices. (These only touch the first "fanSegmentSpan" segments on inner
    // tessellation curves.
    size_t fanVerticesIdx = vertexCount;
    size_t fanSegmentSpan =
        patchType == PatchType::midpointFan ? patchSegmentSpan : patchSegmentSpan - 1;
    assert((fanSegmentSpan & (fanSegmentSpan - 1)) == 0); // The fan must be a power of two.
    for (int i = 0; i <= fanSegmentSpan; ++i)
    {
        float params = pack_params(patchSegmentSpan, FAN_VERTEX);
        if (patchType == PatchType::outerCurves)
        {
            vertices[vertexCount].set(static_cast<float>(i), 0.f, 1, params);
        }
        else
        {
            vertices[vertexCount].set(static_cast<float>(i), -1.f, 1, params);
            vertices[vertexCount].setMirroredPosition(static_cast<float>(i) - 1, -1.f, 1);
        }
        ++vertexCount;
    }

    // The midpoint vertex is only included on midpoint fan patches.
    size_t midpointIdx = vertexCount;
    if (patchType == PatchType::midpointFan)
    {
        vertices[vertexCount++].set(0, 0, 1, pack_params(patchSegmentSpan, FAN_MIDPOINT_VERTEX));
    }
    assert(vertexCount == (patchType == PatchType::outerCurves ? kOuterCurvePatchVertexCount
                                                               : kMidpointFanPatchVertexCount));

    // AA border indices.
    constexpr static size_t kBorderPatternVertexCount = 4;
    constexpr static size_t kBorderPatternIndexCount = 6;
    constexpr static uint16_t kBorderPattern[kBorderPatternIndexCount] = {0, 1, 2, 2, 1, 3};
    constexpr static uint16_t kNegativeBorderPattern[kBorderPatternIndexCount] = {0, 2, 1, 1, 2, 3};

    size_t indexCount = 0;
    size_t borderEdgeVerticesIdx = 0;
    for (size_t borderSegmentIdx = 0; borderSegmentIdx < patchSegmentSpan; ++borderSegmentIdx)
    {
        for (size_t i = 0; i < kBorderPatternIndexCount; ++i)
        {
            indices[indexCount++] = baseVertex + borderEdgeVerticesIdx + kBorderPattern[i];
        }
        borderEdgeVerticesIdx += kBorderPatternVertexCount;
    }

    // Bottom (negative coverage) side of the AA border.
    if (patchType == PatchType::outerCurves)
    {
        for (size_t borderSegmentIdx = 0; borderSegmentIdx < patchSegmentSpan; ++borderSegmentIdx)
        {
            for (size_t i = 0; i < kBorderPatternIndexCount; ++i)
            {
                indices[indexCount++] =
                    baseVertex + borderEdgeVerticesIdx + kNegativeBorderPattern[i];
            }
            borderEdgeVerticesIdx += kBorderPatternVertexCount;
        }
        assert(indexCount == kOuterCurvePatchBorderIndexCount);
    }
    else
    {
        assert(indexCount == kMidpointFanPatchBorderIndexCount);
    }

    assert(borderEdgeVerticesIdx == fanVerticesIdx);

    // Triangle fan indices, in a middle-out topology.
    // Don't include the final bowtie join if this is an "outerStroke" patch. (i.e., use
    // fanSegmentSpan and not "patchSegmentSpan".)
    for (int step = 1; step < fanSegmentSpan; step <<= 1)
    {
        for (int i = 0; i < fanSegmentSpan; i += step * 2)
        {
            indices[indexCount++] = fanVerticesIdx + i + baseVertex;
            indices[indexCount++] = fanVerticesIdx + i + step + baseVertex;
            indices[indexCount++] = fanVerticesIdx + i + step * 2 + baseVertex;
        }
    }
    if (patchType == PatchType::midpointFan)
    {
        // Triangle to the contour midpoint.
        indices[indexCount++] = fanVerticesIdx + baseVertex;
        indices[indexCount++] = fanVerticesIdx + fanSegmentSpan + baseVertex;
        indices[indexCount++] = midpointIdx + baseVertex;
        assert(indexCount == kMidpointFanPatchIndexCount);
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
    generate_buffer_data_for_patch_type(PatchType::midpointFan, vertices, indices, 0);
    generate_buffer_data_for_patch_type(PatchType::outerCurves,
                                        vertices + kMidpointFanPatchVertexCount,
                                        indices + kMidpointFanPatchIndexCount,
                                        kMidpointFanPatchVertexCount);
}

void ClipRectInverseMatrix::reset(const Mat2D& clipMatrix, const AABB& clipRect)
{
    // Find the matrix that transforms from pixel space to "normalized clipRect space", where the
    // clipRect is the normalized rectangle: [-1, -1, +1, +1].
    Mat2D m = clipMatrix * Mat2D(clipRect.width() * .5f,
                                 0,
                                 0,
                                 clipRect.height() * .5f,
                                 clipRect.center().x,
                                 clipRect.center().y);
    if (clipRect.width() <= 0 || clipRect.height() <= 0 || !m.invert(&m_inverseMatrix))
    {
        // If the width or height went zero or negative, or if "m" is non-invertible, clip away
        // everything.
        *this = Empty();
    }
}

static uint32_t paint_type_to_glsl_id(PaintType paintType)
{
    return static_cast<uint32_t>(paintType);
    static_assert((int)PaintType::solidColor == SOLID_COLOR_PAINT_TYPE);
    static_assert((int)PaintType::linearGradient == LINEAR_GRADIENT_PAINT_TYPE);
    static_assert((int)PaintType::radialGradient == RADIAL_GRADIENT_PAINT_TYPE);
    static_assert((int)PaintType::image == IMAGE_PAINT_TYPE);
    static_assert((int)PaintType::clipUpdate == CLIP_UPDATE_PAINT_TYPE);
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

FlushUniforms::InverseViewports::InverseViewports(const FlushDescriptor& flushDesc,
                                                  const PlatformFeatures& platformFeatures)
{
    float4 numerators = 2;
    if (platformFeatures.invertOffscreenY)
    {
        numerators.xy = -numerators.xy;
    }
    if (platformFeatures.uninvertOnScreenY)
    {
        numerators.w = -numerators.w;
    }
    float4 vals = numerators / float4{static_cast<float>(flushDesc.complexGradRowsHeight),
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
    m_colorClearValue(SwizzleRiveColorToRGBA(flushDesc.clearColor)),
    m_coverageClearValue(flushDesc.coverageClearValue),
    m_renderTargetUpdateBounds(flushDesc.renderTargetUpdateBounds),
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

void PathData::set(const Mat2D& m, float strokeRadius, uint32_t zIndex)
{
    write_matrix(m_matrix, m);
    m_strokeRadius = strokeRadius; // 0 if the path is filled.
    m_zIndex = zIndex;
}

void PaintData::set(FillRule fillRule,
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
            // Swizzle the riveColor to little-endian RGBA (the order expected by GLSL).
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
            m_gradTextureY = (static_cast<float>(row) + .5f) * gradTextureLayout.inverseHeight;
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
    if (fillRule == FillRule::evenOdd)
    {
        localParams |= PAINT_FLAG_EVEN_ODD;
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
                       const PLSGradient* gradient,
                       const PLSTexture* imageTexture,
                       const ClipRectInverseMatrix* clipRectInverseMatrix,
                       const PLSRenderTarget* renderTarget,
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
            if (platformFeatures.fragCoordBottomUp)
            {
                // Flip _fragCoord.y.
                paintMatrix = paintMatrix * Mat2D(1, 0, 0, -1, 0, renderTarget->height());
            }
            if (paintType == PaintType::image)
            {
                uint64_t bindlessTextureHandle = imageTexture->bindlessTextureHandle();
                m_bindlessTextureHandle[0] = static_cast<uint32_t>(bindlessTextureHandle);
                m_bindlessTextureHandle[1] = bindlessTextureHandle >> 32;
            }
            else
            {
                assert(gradient != nullptr);
                const float* gradCoeffs = gradient->coeffs();
                if (paintType == PaintType::linearGradient)
                {
                    paintMatrix =
                        Mat2D(gradCoeffs[0], 0, gradCoeffs[1], 0, gradCoeffs[2], 0) * paintMatrix;
                }
                else
                {
                    assert(paintType == PaintType::radialGradient);
                    float w = 1 / gradCoeffs[2];
                    paintMatrix =
                        Mat2D(w, 0, 0, w, -gradCoeffs[0] * w, -gradCoeffs[1] * w) * paintMatrix;
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
                m_gradTextureHorizontalSpan[0] = (right - left - 1) * GRAD_TEXTURE_INVERSE_WIDTH;
                m_gradTextureHorizontalSpan[1] = (left + .5f) * GRAD_TEXTURE_INVERSE_WIDTH;
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
        if (platformFeatures.fragCoordBottomUp)
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
        write_matrix(m_clipRectInverseMatrix, ClipRectInverseMatrix::WideOpen().inverseMatrix());
        m_inverseFwidth.x = 0;
        m_inverseFwidth.y = 0;
    }
}

ImageDrawUniforms::ImageDrawUniforms(const Mat2D& matrix,
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

std::tuple<uint32_t, uint32_t> StorageTextureSize(size_t bufferSizeInBytes,
                                                  StorageBufferStructure bufferStructure)
{
    assert(bufferSizeInBytes % gpu::StorageBufferElementSizeInBytes(bufferStructure) == 0);
    uint32_t elementCount = math::lossless_numeric_cast<uint32_t>(bufferSizeInBytes) /
                            gpu::StorageBufferElementSizeInBytes(bufferStructure);
    uint32_t height = (elementCount + STORAGE_TEXTURE_WIDTH - 1) / STORAGE_TEXTURE_WIDTH;
    // PLSRenderContext is responsible for breaking up a flush before any storage buffer grows
    // larger than can be supported by a GL texture of width "STORAGE_TEXTURE_WIDTH".
    // (2048 is the min required value for GL_MAX_TEXTURE_SIZE.)
    constexpr int kMaxRequredTextureHeight RIVE_MAYBE_UNUSED = 2048;
    assert(height <= kMaxRequredTextureHeight);
    uint32_t width = std::min<uint32_t>(elementCount, STORAGE_TEXTURE_WIDTH);
    return {width, height};
}

size_t StorageTextureBufferSize(size_t bufferSizeInBytes, StorageBufferStructure bufferStructure)
{
    // The polyfill texture needs to be updated in entire rows at a time. Extend the buffer's length
    // to be able to service a worst-case scenario.
    return bufferSizeInBytes +
           (STORAGE_TEXTURE_WIDTH - 1) * gpu::StorageBufferElementSizeInBytes(bufferStructure);
}

float FindTransformedArea(const AABB& bounds, const Mat2D& matrix)
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
    return (fabsf(Vec2D::cross(v[0], v[1])) + fabsf(Vec2D::cross(v[1], v[2]))) * .5f;
}
} // namespace rive::gpu

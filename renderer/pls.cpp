/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls.hpp"

#include "shaders/constants.glsl"

#include "../out/obj/generated/draw_path.exports.h"

namespace rive::pls
{
static_assert(kGradTextureWidth == GRAD_TEXTURE_WIDTH);
static_assert(kTessTextureWidth == TESS_TEXTURE_WIDTH);
static_assert(kTessTextureWidthLog2 == TESS_TEXTURE_WIDTH_LOG2);

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
    size_t patchSegmentSpan = patchType == PatchType::midpointFan ? kMidpointFanPatchSegmentSpan
                                                                  : kOuterCurvePatchSegmentSpan;
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

void PathData::set(const Mat2D& m,
                   float strokeRadius_,
                   FillRule fillRule,
                   PaintType paintType,
                   uint32_t clipID,
                   BlendMode riveBlendMode,
                   const PaintData& paintData_,
                   const ClipRectInverseMatrix* clipRectInverseMatrix_)
{
    matrix = m;
    strokeRadius = strokeRadius_;
    uint32_t localParams = ConvertBlendModeToPLSBlendMode(riveBlendMode);
    localParams |= clipID << 4;
    localParams |= paint_type_to_glsl_id(paintType) << 20;
    if (fillRule == FillRule::evenOdd && strokeRadius_ == 0)
    {
        localParams |= EVEN_ODD_PATH_FLAG;
    }
    params = localParams;
    paintData = paintData_;
    clipRectInverseMatrix = clipRectInverseMatrix_ != nullptr ? *clipRectInverseMatrix_
                                                              : ClipRectInverseMatrix::WideOpen();
}

ImageDrawUniforms::ImageDrawUniforms(const Mat2D& matrix_,
                                     float opacity_,
                                     const ClipRectInverseMatrix* clipRectInverseMatrix_,
                                     uint32_t clipID_,
                                     BlendMode blendMode_) :
    matrix(matrix_),
    opacity(opacity_),
    clipRectInverseMatrix(clipRectInverseMatrix_ != nullptr ? *clipRectInverseMatrix_
                                                            : ClipRectInverseMatrix::WideOpen()),
    clipID(clipID_),
    blendMode(ConvertBlendModeToPLSBlendMode(blendMode_))
{}
} // namespace rive::pls

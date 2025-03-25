/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/enum_bitset.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/math/simd.hpp"
#include "rive/refcnt.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/renderer/trivial_block_allocator.hpp"

namespace rive
{
class GrInnerFanTriangulator;
class RenderBuffer;
} // namespace rive

// This header defines constants and data structures for Rive's pixel local
// storage path rendering algorithm.
//
// Main algorithm:
// https://docs.google.com/document/d/19Uk9eyFxav6dNSYsI2ZyiX9zHU1YOaJsMB2sdDFVz6s/edit
//
// Batching multiple unique paths:
// https://docs.google.com/document/d/1DLrQimS5pbNaJJ2sAW5oSOsH6_glwDPo73-mtG5_zns/edit
//
// Batching strokes as well:
// https://docs.google.com/document/d/1CRKihkFjbd1bwT08ErMCP4fwSR7D4gnHvgdw_esY9GM/edit
namespace rive::gpu
{
class Draw;
class Gradient;
class RenderContextImpl;
class RenderTarget;
class Texture;

// Tessellate in parametric space until each segment is within 1/4 pixel of the
// true curve.
constexpr static int kParametricPrecision = 4;

// Tessellate in polar space until the outset edge is within 1/8 pixel of the
// true stroke.
constexpr static int kPolarPrecision = 8;

// Maximum supported numbers of tessellated segments in a single curve.
constexpr static uint32_t kMaxParametricSegments = 1023;
constexpr static uint32_t kMaxPolarSegments = 1023;

// The Gaussian distribution is very blurry on the outer edges. Regardless of
// how wide a feather is, the polar segments never need to have a finer angle
// than this value.
constexpr static float FEATHER_POLAR_SEGMENT_MIN_ANGLE = math::PI / 16;

// cos(FEATHER_MIN_POLAR_SEGMENT_ANGLE / 2)
constexpr static float COS_FEATHER_POLAR_SEGMENT_MIN_ANGLE_OVER_2 =
    0.99518472667f;

// We allocate all our GPU buffers in rings. This ensures the CPU can prepare
// frames in parallel while the GPU renders them.
constexpr static int kBufferRingSize = 3;

// Every coverage value in pixel local storage has an associated 16-bit path ID.
// This ID enables us to batch multiple paths together without having to clear
// the coverage buffer in between. This ID is implemented as an fp16, so the
// maximum path ID therefore cannot be NaN (or conservatively, all 5 exponent
// bits cannot be 1's). We also skip denormalized values (exp == 0) because they
// have been empirically unreliable on Android as ID values.
constexpr static int kLargestFP16BeforeExponentAll1s = (0x1f << 10) - 1;
constexpr static int kLargestDenormalizedFP16 = 1023;
constexpr static int MaxPathID(int granularity)
{
    // Floating point equality gets funky when the exponent bits are all 1's, so
    // the largest pathID we can support is kLargestFP16BeforeExponentAll1s.
    //
    // The shader converts an integer path ID to fp16 as:
    //
    //     (id + kLargestDenormalizedFP16) * granularity
    //
    // So the largest path ID we can support is as follows.
    return kLargestFP16BeforeExponentAll1s / granularity -
           kLargestDenormalizedFP16;
}

// Each contour has its own unique ID, which it uses to index a data record
// containing per-contour information. This value is currently 16 bit.
constexpr static size_t kMaxContourID = 65535;
constexpr static uint32_t kContourIDMask = 0xffff;
static_assert((kMaxContourID & kContourIDMask) == kMaxContourID);

// Tessellation is performed by rendering vertices into a data texture. These
// values define the dimensions of the tessellation data texture.
constexpr static size_t kTessTextureWidth =
    2048; // GL_MAX_TEXTURE_SIZE spec minimum on ES3/WebGL2.
constexpr static size_t kTessTextureWidthLog2 = 11;
static_assert(1 << kTessTextureWidthLog2 == kTessTextureWidth);

// Gradients are implemented by sampling a horizontal ramp of pixels allocated
// in a global gradient texture.
constexpr static uint32_t kGradTextureWidth = 512;
constexpr static uint32_t kGradTextureWidthInSimpleRamps =
    kGradTextureWidth / 2;

// Depth/stencil parameters
constexpr static float DEPTH_MIN = 0.0f;
constexpr static float DEPTH_MAX = 1.0f;
constexpr static uint32_t STENCIL_CLEAR = 0u;

// Backend-specific capabilities/workarounds and fine tunin// g.
struct PlatformFeatures
{
    // InterlockMode::rasterOrdering.
    bool supportsRasterOrdering = false;
    // InterlockMode::atomics.
    bool supportsFragmentShaderAtomics = false;
    // Experimental rendering mode selected by InterlockMode::clockwiseAtomic.
    bool supportsClockwiseAtomicRendering = false;
    // Use KHR_blend_equation_advanced in msaa mode?
    bool supportsKHRBlendEquations = false;
    // Required for @ENABLE_CLIP_RECT in msaa mode.
    bool supportsClipPlanes = false;
    bool avoidFlatVaryings = false;
    // Vivo Y21 (PowerVR Rogue GE8320; OpenGL ES 3.2 build 1.13@5776728a) seems
    // to hit some sort of reset condition that corrupts pixel local storage
    // when rendering a complex feather. Provide a workaround that allows the
    // implementation to opt in to always feathering to the atlas instead of
    // rendering directly to the screen.
    bool alwaysFeatherToAtlas = false;
    // clipSpaceBottomUp specifies whether the top of the viewport, in clip
    // coordinates, is at Y=+1 (OpenGL, Metal, D3D, WebGPU) or Y=-1 (Vulkan).
    //
    // framebufferBottomUp specifies whether "row 0" of the framebuffer is the
    // bottom of the image (OpenGL) or the top (Metal, D3D, WebGPU, Vulkan).
    //
    //
    //                                OpenGL
    //           (clipSpaceBottomUp=true, framebufferBottomUp=true)
    //
    //  Rive Pixel Space             Clip Space              Framebuffer
    //
    //  0 ----------->                   ^ +1                ^ height
    //  |          width                 |                   |
    //  |                         -1     |     +1            |
    //  |                 ===>    <------|------>    ===>    |
    //  |                                |                   |
    //  |                                |                   |          width
    //  v height                         v -1                0 ----------->
    //
    //
    //
    //                            Metal/D3D/WebGPU
    //           (clipSpaceBottomUp=true, framebufferBottomUp=false)
    //
    //  Rive Pixel Space             Clip Space              Framebuffer
    //
    //  0 ----------->                   ^ +1                0 ----------->
    //  |          width                 |                   |          width
    //  |                         -1     |     +1            |
    //  |                 ===>    <------|------>    ===>    |
    //  |                                |                   |
    //  |                                |                   |
    //  v height                         v -1                v height
    //
    //
    //
    //                                Vulkan
    //          (clipSpaceBottomUp=false, framebufferBottomUp=false)
    //
    //  Rive Pixel Space             Clip Space              Framebuffer
    //
    //  0 ----------->                   ^ -1                0 ----------->
    //  |          width                 |                   |          width
    //  |                         -1     |     +1            |
    //  |                 ===>    <------|------>    ===>    |
    //  |                                |                   |
    //  |                                |                   |
    //  v height                         v +1                v height
    //
    bool clipSpaceBottomUp = false;
    bool framebufferBottomUp = false;
    // Backend cannot initialize PLS with typical clear/load APIs in atomic
    // mode. Issue a "DrawType::atomicInitialize" draw instead.
    bool atomicPLSMustBeInitializedAsDraw = false;
    // Workaround for precision issues. Determines how far apart we space unique
    // path IDs when they will be bit-casted to fp16.
    uint8_t pathIDGranularity = 1;
    // Maximum size (width or height) of a texture.
    uint32_t maxTextureSize = 2048;
    // Maximum length (in 32-bit uints) of the coverage buffer used for paths in
    // clockwiseFill/atomic mode. 2^27 bytes is the minimum storage buffer size
    // requirement in the Vulkan, GL, and D3D11 specs. Metal guarantees 256 MB.
    size_t maxCoverageBufferLength = (1 << 27) / sizeof(uint32_t);
};

// Gradient color stops are implemented as a horizontal span of pixels in a
// global gradient texture. They are rendered by "GradientSpan" instances.
struct GradientSpan
{
    // x0Fixed and x1Fixed are normalized texel x coordinates, in the
    // fixed-point range 0..65535.
    RIVE_ALWAYS_INLINE void set(uint32_t x0Fixed,
                                uint32_t x1Fixed,
                                uint32_t y,
                                uint32_t flags,
                                ColorInt color0_,
                                ColorInt color1_)
    {
        assert(x0Fixed < 65536);
        assert(x1Fixed < 65536);
        horizontalSpan = (x1Fixed << 16) | x0Fixed;
        yWithFlags = flags | y;
        color0 = color0_;
        color1 = color1_;
    }
    uint32_t horizontalSpan;
    uint32_t yWithFlags;
    uint32_t color0;
    uint32_t color1;
};
static_assert(sizeof(GradientSpan) == sizeof(uint32_t) * 4);
static_assert(256 % sizeof(GradientSpan) == 0);
// Metal requires vertex buffers to be 256-byte aligned.
constexpr static size_t kGradSpanBufferAlignmentInElements =
    256 / sizeof(GradientSpan);

// Gradient spans are drawn as 1px-tall triangle strips with 3 sub-rectangles.
constexpr uint32_t GRAD_SPAN_TRI_STRIP_VERTEX_COUNT = 8;

// Each curve gets tessellated into vertices. This is performed by rendering a
// horizontal span of positions and normals into the tessellation data texture,
// GP-GPU style. TessVertexSpan defines one instance of a horizontal
// tessellation span for rendering.
//
// Each span has an optional reflection, rendered right to left, with the same
// vertices in reverse order. These are used to draw mirrored patches with
// negative coverage when we have back-face culling enabled. This emits every
// triangle twice, once clockwise and once counterclockwise, and back-face
// culling naturally selects the triangle with the appropriately signed coverage
// (discarding the other).
struct TessVertexSpan
{
    RIVE_ALWAYS_INLINE void set(const Vec2D pts_[4],
                                Vec2D joinTangent_,
                                float y_,
                                int32_t x0,
                                int32_t x1,
                                uint32_t parametricSegmentCount,
                                uint32_t polarSegmentCount,
                                uint32_t joinSegmentCount,
                                uint32_t contourIDWithFlags_)
    {
        set(pts_,
            joinTangent_,
            y_,
            x0,
            x1,
            std::numeric_limits<float>::quiet_NaN(), // Discard the reflection.
            -1,
            -1,
            parametricSegmentCount,
            polarSegmentCount,
            joinSegmentCount,
            contourIDWithFlags_);
    }

    RIVE_ALWAYS_INLINE void set(const Vec2D pts_[4],
                                Vec2D joinTangent_,
                                float y_,
                                int32_t x0,
                                int32_t x1,
                                float reflectionY_,
                                int32_t reflectionX0,
                                int32_t reflectionX1,
                                uint32_t parametricSegmentCount,
                                uint32_t polarSegmentCount,
                                uint32_t joinSegmentCount,
                                uint32_t contourIDWithFlags_)
    {
#ifndef NDEBUG
        // Write to an intermediate local object in debug mode, so we can check
        // its values. (Otherwise we can't read it because mapped memory is
        // write-only.)
        TessVertexSpan localCopy;
#define LOCAL(VAR) localCopy.VAR
#else
#define LOCAL(VAR) VAR
#endif
        RIVE_INLINE_MEMCPY(LOCAL(pts), pts_, sizeof(LOCAL(pts)));
        LOCAL(joinTangent) = joinTangent_;
        LOCAL(y) = y_;
        LOCAL(reflectionY) = reflectionY_;
        LOCAL(x0x1) = (x1 << 16 | (x0 & 0xffff));
        LOCAL(reflectionX0X1) = (reflectionX1 << 16 | (reflectionX0 & 0xffff));
        LOCAL(segmentCounts) = (joinSegmentCount << 20) |
                               (polarSegmentCount << 10) |
                               parametricSegmentCount;
        LOCAL(contourIDWithFlags) = contourIDWithFlags_;
#undef LOCAL

        // Ensure we didn't lose any data from packing.
        assert(localCopy.x0x1 << 16 >> 16 == x0);
        assert(localCopy.x0x1 >> 16 == x1);
        assert(localCopy.reflectionX0X1 << 16 >> 16 == reflectionX0);
        assert(localCopy.reflectionX0X1 >> 16 == reflectionX1);
        assert((localCopy.segmentCounts & 0x3ff) == parametricSegmentCount);
        assert(((localCopy.segmentCounts >> 10) & 0x3ff) == polarSegmentCount);
        assert(localCopy.segmentCounts >> 20 == joinSegmentCount);

#ifndef NDEBUG
        memcpy(this, &localCopy, sizeof(*this));
#endif
    }

    Vec2D pts[4];      // Cubic bezier curve.
    Vec2D joinTangent; // Ending tangent of the join that follows the cubic.
    float y;
    float reflectionY;
    int32_t x0x1;
    int32_t reflectionX0X1;
    uint32_t segmentCounts;      // [joinSegmentCount, polarSegmentCount,
                                 // parametricSegmentCount]
    uint32_t contourIDWithFlags; // flags | contourID
};
static_assert(sizeof(TessVertexSpan) == sizeof(float) * 16);
static_assert(256 % sizeof(TessVertexSpan) == 0);
// Metal requires vertex buffers to be 256-byte aligned.
constexpr static size_t kTessVertexBufferAlignmentInElements =
    256 / sizeof(TessVertexSpan);

// Tessellation spans are drawn as two distinct, 1px-tall rectangles: the span
// and its reflection.
constexpr uint16_t kTessSpanIndices[4 * 3] =
    {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};

// ImageRects are a special type of non-overlapping antialiased draw that we
// only have to use in atomic mode. They allow us to bind a texture and draw it
// in its entirety in a single pass.
struct ImageRectVertex
{
    float x;
    float y;
    float aaOffsetX;
    float aaOffsetY;
};

constexpr ImageRectVertex kImageRectVertices[12] = {
    {0, 0, .0, -1},
    {1, 0, .0, -1},
    {1, 0, +1, .0},
    {1, 1, +1, .0},
    {1, 1, .0, +1},
    {0, 1, .0, +1},
    {0, 1, -1, .0},
    {0, 0, -1, .0},
    {0, 0, +1, +1},
    {1, 0, -1, +1},
    {1, 1, -1, -1},
    {0, 1, +1, -1},
};

constexpr uint16_t kImageRectIndices[14 * 3] = {
    8,  0, 9, 9, 0, 1,  1,  2, 9, 9, 2, 10, 10, 2, 3, 3, 4,  10, 10, 4, 11,
    11, 4, 5, 5, 6, 11, 11, 6, 8, 8, 6, 7,  7,  0, 8, 9, 10, 8,  10, 8, 11,
};

enum class PaintType : uint32_t
{
    clipUpdate, // Update the clip buffer instead of drawing to the framebuffer.
    solidColor,
    linearGradient,
    radialGradient,
    image,
};

// Specifies the location of a simple or complex horizontal color ramp within
// the gradient texture. A simple color ramp is two texels wide, beginning at
// the specified row and column. A complex color ramp spans the entire width of
// the gradient texture, on the row:
//     "GradTextureLayout::complexOffsetY + ColorRampLocation::row".
struct ColorRampLocation
{
    constexpr static uint16_t kComplexGradientMarker = 0xffff;
    bool isComplex() const { return col == kComplexGradientMarker; }
    uint16_t row;
    uint16_t col;
};

// Most of a paint's information can be described in a single value. Gradients
// and images reference an additional Gradient* and Texture* respectively.
union SimplePaintValue
{
    ColorInt color = 0xff000000;         // PaintType::solidColor
    ColorRampLocation colorRampLocation; // Paintype::linear/radialGradient
    float imageOpacity;                  // PaintType::image
    uint32_t outerClipID;                // Paintype::clipUpdate
};
static_assert(sizeof(SimplePaintValue) == 4);

// This class encapsulates a matrix that maps from _fragCoord to a space where
// the clipRect is the normalized rectangle: [-1, -1, +1, +1]
class ClipRectInverseMatrix
{
public:
    // When the clipRect inverse matrix is singular (e.g., all 0 in scale and
    // skew), the shader uses tx and ty as fixed clip coverage values instead of
    // finding edge distances.
    constexpr static ClipRectInverseMatrix WideOpen()
    {
        return Mat2D{0, 0, 0, 0, 1, 1};
    }
    constexpr static ClipRectInverseMatrix Empty()
    {
        return Mat2D{0, 0, 0, 0, 0, 0};
    }

    ClipRectInverseMatrix() = default;

    ClipRectInverseMatrix(const Mat2D& clipMatrix, const AABB& clipRect)
    {
        reset(clipMatrix, clipRect);
    }

    void reset(const Mat2D& clipMatrix, const AABB& clipRect);

    const Mat2D& inverseMatrix() const { return m_inverseMatrix; }

private:
    constexpr ClipRectInverseMatrix(const Mat2D& inverseMatrix) :
        m_inverseMatrix(inverseMatrix)
    {}
    Mat2D m_inverseMatrix;
};

// Specifies the height of the gradient texture, and the row at which we
// transition from simple color ramps to complex.
//
// This information is computed at flush time, once we know exactly how many
// color ramps of each type will be in the gradient texture.
struct GradTextureLayout
{
    uint32_t complexOffsetY; // Row of the first complex gradient.
    float inverseHeight;     // 1 / textureHeight
};

// Once all curves in a contour have been tessellated, we render the tessellated
// vertices in "patches" (aka specific instanced geometry).
//
// See:
// https://docs.google.com/document/d/19Uk9eyFxav6dNSYsI2ZyiX9zHU1YOaJsMB2sdDFVz6s/edit#heading=h.fa4kubk3vimk
//
// With strokes:
// https://docs.google.com/document/d/1CRKihkFjbd1bwT08ErMCP4fwSR7D4gnHvgdw_esY9GM/edit#heading=h.dcd0c58pxfs5
//
// A single patch spans N tessellation segments, connecting N + 1 tessellation
// vertices. It is composed of a an AA border and fan triangles. The specifics
// of the fan triangles depend on the PatchType.
enum class PatchType
{
    // Patches fan around the contour midpoint. Outer edges are inset by ~1px,
    // followed by a ~1px AA ramp.
    midpointFan,

    // Similar to midpointFan, except AA ramps are split down the center and
    // drawn with a ~1/2px outset AA ramp and a ~1/2px inset AA ramp that
    // overlaps the inner tessellation and has negative coverage.
    midpointFanCenterAA,

    // Patches only cover the AA ramps and interiors of bezier curves. The
    // interior path triangles that connect the outer curves are triangulated on
    // the CPU to eliminate overlap, and are drawn in a separate call. AA ramps
    // are split down the center (on the same lines as the interior
    // triangulation), and drawn with a ~1/2px outset AA ramp and a ~1/2px inset
    // AA ramp that overlaps the inner tessellation and has negative coverage. A
    // lone bowtie join is emitted at the end of the patch to tie the outer
    // curves together.
    outerCurves,
};

// When tessellating path vertices, we have the ability to generate the
// triangles wound in forward or reverse order. Depending on the path and the
// rendering algorithm, we will either want the triangles wound forward,
// reverse, or BOTH.
enum class ContourDirections
{
    forward,
    reverse,
    // Generate two tessellations of the contour: reverse first, then forward.
    reverseThenForward,
    // Generate two tessellations of the contour: forward first, then reverse.
    forwardThenReverse,
};
constexpr static bool ContourDirectionsAreDoubleSided(
    ContourDirections contourDirections)
{
    return contourDirections >= ContourDirections::reverseThenForward;
}

struct PatchVertex
{
    void set(float localVertexID_,
             float outset_,
             float fillCoverage_,
             float params_)
    {
        localVertexID = localVertexID_;
        outset = outset_;
        fillCoverage = fillCoverage_;
        params = params_;
        setMirroredPosition(localVertexID_, outset_, fillCoverage_);
    }

    // Patch vertices can have an optional, alternate position when mirrored.
    // This is so we can ensure the diagonals inside the stroke line up on both
    // versions of the patch (mirrored and not).
    void setMirroredPosition(float localVertexID_,
                             float outset_,
                             float fillCoverage_)
    {
        mirroredVertexID = localVertexID_;
        mirroredOutset = outset_;
        mirroredFillCoverage = fillCoverage_;
    }

    float localVertexID; // 0 or 1 -- which tessellated vertex of the two that
                         // we are connecting?
    float outset; // Outset from the tessellated position, in the direction of
                  // the normal.
    float fillCoverage; // 0..1 for the stroke. 1 all around for the triangles.
                        // (Coverage will be negated later for counterclockwise
                        // triangles.)
    int32_t params;     // "(patchSize << 2) | [flags::kStrokeVertex,
                        //                      flags::kFanVertex,
                        //                      flags::kFanMidpointVertex]"
    float mirroredVertexID;
    float mirroredOutset;
    float mirroredFillCoverage;
    int32_t padding = 0;
};
static_assert(sizeof(PatchVertex) == sizeof(float) * 8);

// # of tessellation segments spanned by the midpoint fan patch.
constexpr static uint32_t kMidpointFanPatchSegmentSpan = 8;

// # of tessellation segments spanned by the outer curve patch. (In this
// particular instance, the final segment is a bowtie join with zero length and
// no fan triangle.)
constexpr static uint32_t kOuterCurvePatchSegmentSpan = 17;

// Define vertex and index buffers that contain all the triangles in every
// PatchType.
constexpr static uint32_t kMidpointFanPatchVertexCount =
    kMidpointFanPatchSegmentSpan * 4 /*Stroke and/or AA outer ramp*/ +
    (kMidpointFanPatchSegmentSpan + 1) /*Curve fan*/ +
    1 /*Triangle from path midpoint*/;
constexpr static uint32_t kMidpointFanPatchBorderIndexCount =
    kMidpointFanPatchSegmentSpan * 6 /*Stroke and/or AA outer ramp*/;
constexpr static uint32_t kMidpointFanPatchIndexCount =
    kMidpointFanPatchBorderIndexCount /*Stroke and/or AA outer ramp*/ +
    (kMidpointFanPatchSegmentSpan - 1) * 3 /*Curve fan*/ +
    3 /*Triangle from path midpoint*/;
constexpr static uint32_t kMidpointFanPatchBaseIndex = 0;
static_assert((kMidpointFanPatchBaseIndex * sizeof(uint16_t)) % 4 == 0);

constexpr static uint32_t kMidpointFanCenterAAPatchVertexCount =
    kMidpointFanPatchSegmentSpan * 4 * 2 /*Stroke and/or AA outer ramp*/ +
    (kMidpointFanPatchSegmentSpan + 1) /*Curve fan*/ +
    1 /*Triangle from path midpoint*/;
constexpr static uint32_t kMidpointFanCenterAAPatchBorderIndexCount =
    kMidpointFanPatchSegmentSpan * 12 /*Stroke and/or AA outer ramp*/;
constexpr static uint32_t kMidpointFanCenterAAPatchIndexCount =
    kMidpointFanCenterAAPatchBorderIndexCount /*Stroke and/or AA outer ramp*/ +
    (kMidpointFanPatchSegmentSpan - 1) * 3 /*Curve fan*/ +
    3 /*Triangle from path midpoint*/;
constexpr static uint32_t kMidpointFanCenterAAPatchBaseIndex =
    kMidpointFanPatchBaseIndex + kMidpointFanPatchIndexCount;
static_assert((kMidpointFanCenterAAPatchBaseIndex * sizeof(uint16_t)) % 4 == 0);

constexpr static uint32_t kOuterCurvePatchVertexCount =
    kOuterCurvePatchSegmentSpan * 8 /*AA center ramp with bowtie*/ +
    kOuterCurvePatchSegmentSpan /*Curve fan*/;
constexpr static uint32_t kOuterCurvePatchBorderIndexCount =
    kOuterCurvePatchSegmentSpan * 12 /*AA center ramp with bowtie*/;
constexpr static uint32_t kOuterCurvePatchIndexCount =
    kOuterCurvePatchBorderIndexCount /*AA center ramp with bowtie*/ +
    (kOuterCurvePatchSegmentSpan - 2) * 3 /*Curve fan*/;
constexpr static uint32_t kOuterCurvePatchBaseIndex =
    kMidpointFanCenterAAPatchBaseIndex + kMidpointFanCenterAAPatchIndexCount;
static_assert((kOuterCurvePatchBaseIndex * sizeof(uint16_t)) % 4 == 0);

constexpr static uint32_t kPatchVertexBufferCount =
    kMidpointFanPatchVertexCount + kMidpointFanCenterAAPatchVertexCount +
    kOuterCurvePatchVertexCount;
constexpr static uint32_t kPatchIndexBufferCount =
    kMidpointFanPatchIndexCount + kMidpointFanCenterAAPatchIndexCount +
    kOuterCurvePatchIndexCount;
void GeneratePatchBufferData(PatchVertex[kPatchVertexBufferCount],
                             uint16_t indices[kPatchIndexBufferCount]);

enum class DrawType : uint8_t
{
    midpointFanPatches,         // Fills, strokes, feathered strokes.
    midpointFanCenterAAPatches, // Feathered fills.
    outerCurvePatches, // Just the outer curves of a path; the interior will be
                       // triangulated.
    interiorTriangulation,
    atlasBlit,
    imageRect,
    imageMesh,
    atomicInitialize, // Clear/init PLS data when we can't do it with existing
                      // clear/load APIs.
    atomicResolve, // Resolve PLS data to the final renderTarget color in atomic
                   // mode.
    stencilClipReset, // Clear or intersect (based on DrawContents) the stencil
                      // clip bit.
};

constexpr static bool DrawTypeIsImageDraw(DrawType drawType)
{
    switch (drawType)
    {
        case DrawType::imageRect:
        case DrawType::imageMesh:
            return true;
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
        case DrawType::atomicInitialize:
        case DrawType::atomicResolve:
        case DrawType::stencilClipReset:
            return false;
    }
    RIVE_UNREACHABLE();
}

constexpr static uint32_t PatchIndexCount(DrawType drawType)
{
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
            return kMidpointFanPatchIndexCount;
        case DrawType::midpointFanCenterAAPatches:
            return kMidpointFanCenterAAPatchIndexCount;
        case DrawType::outerCurvePatches:
            return kOuterCurvePatchIndexCount;
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
        case DrawType::imageRect:
        case DrawType::imageMesh:
        case DrawType::atomicInitialize:
        case DrawType::atomicResolve:
        case DrawType::stencilClipReset:
            RIVE_UNREACHABLE();
    }
    RIVE_UNREACHABLE();
}

constexpr static uint32_t PatchBorderIndexCount(DrawType drawType)
{
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
            return kMidpointFanPatchBorderIndexCount;
        case DrawType::midpointFanCenterAAPatches:
            return kMidpointFanCenterAAPatchBorderIndexCount;
        case DrawType::outerCurvePatches:
            return kOuterCurvePatchBorderIndexCount;
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
        case DrawType::imageRect:
        case DrawType::imageMesh:
        case DrawType::atomicInitialize:
        case DrawType::atomicResolve:
        case DrawType::stencilClipReset:
            RIVE_UNREACHABLE();
    }
    RIVE_UNREACHABLE();
}

constexpr static uint32_t PatchFanIndexCount(DrawType drawType)
{
    return PatchIndexCount(drawType) - PatchBorderIndexCount(drawType);
}

constexpr static uint32_t PatchBaseIndex(DrawType drawType)
{
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
            return kMidpointFanPatchBaseIndex;
        case DrawType::midpointFanCenterAAPatches:
            return kMidpointFanCenterAAPatchBaseIndex;
        case DrawType::outerCurvePatches:
            return kOuterCurvePatchBaseIndex;
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
        case DrawType::imageRect:
        case DrawType::imageMesh:
        case DrawType::atomicInitialize:
        case DrawType::atomicResolve:
        case DrawType::stencilClipReset:
            RIVE_UNREACHABLE();
    }
    RIVE_UNREACHABLE();
}

constexpr static uint32_t PatchFanBaseIndex(DrawType drawType)
{
    return PatchBaseIndex(drawType) + PatchBorderIndexCount(drawType);
}

// Specifies what to do with the render target at the beginning of a flush.
enum class LoadAction
{
    clear,
    preserveRenderTarget,
    dontCare,
};

// Synchronization method for pixel local storage with overlapping fragments.
enum class InterlockMode
{
    rasterOrdering,
    atomics,
    // Use an experimental path rendering algorithm that utilizes atomics
    // without barriers. This requires that we override all paths' fill rules
    // (winding or even/odd) with a "clockwise" fill rule, where only regions
    // with a positive winding number get filled.
    clockwiseAtomic,
    msaa,
};
constexpr static size_t kInterlockModeCount = 4;

// Low-level batch of scissored geometry for rendering to the offscreen atlas.
struct AtlasDrawBatch
{
    TAABB<uint16_t> scissor;
    uint32_t patchCount;
    uint32_t basePatch;
};

// "Uber shader" features that can be #defined in a draw shader.
// This set is strictly limited to switches that don't *change* the behavior of
// the shader, i.e., turning them all on will enable all types Rive content, but
// simple content will still draw identically; we can turn a feature off if we
// know a batch doesn't need it for better performance.
enum class ShaderFeatures
{
    NONE = 0,

    // Whole program features.
    ENABLE_CLIPPING = 1 << 0,
    ENABLE_CLIP_RECT = 1 << 1,
    ENABLE_ADVANCED_BLEND = 1 << 2,
    ENABLE_FEATHER = 1 << 3,

    // Fragment-only features.
    ENABLE_EVEN_ODD = 1 << 4,
    ENABLE_NESTED_CLIPPING = 1 << 5,
    ENABLE_HSL_BLEND_MODES = 1 << 6,
};
RIVE_MAKE_ENUM_BITSET(ShaderFeatures)
constexpr static size_t kShaderFeatureCount = 7;
constexpr static ShaderFeatures kAllShaderFeatures =
    static_cast<gpu::ShaderFeatures>((1 << kShaderFeatureCount) - 1);
constexpr static ShaderFeatures kVertexShaderFeaturesMask =
    ShaderFeatures::ENABLE_CLIPPING | ShaderFeatures::ENABLE_CLIP_RECT |
    ShaderFeatures::ENABLE_ADVANCED_BLEND | ShaderFeatures::ENABLE_FEATHER;

constexpr static ShaderFeatures ShaderFeaturesMaskFor(
    InterlockMode interlockMode)
{
    switch (interlockMode)
    {
        case InterlockMode::rasterOrdering:
            return kAllShaderFeatures;
        case InterlockMode::atomics:
            return kAllShaderFeatures & ~ShaderFeatures::ENABLE_NESTED_CLIPPING;
        case InterlockMode::clockwiseAtomic:
            // TODO: shader features aren't fully implemented yet in
            // clockwiseAtomic mode.
            return ShaderFeatures::ENABLE_FEATHER;
        case InterlockMode::msaa:
            return ShaderFeatures::ENABLE_CLIP_RECT |
                   ShaderFeatures::ENABLE_ADVANCED_BLEND |
                   ShaderFeatures::ENABLE_HSL_BLEND_MODES;
    }
    RIVE_UNREACHABLE();
}

// Miscellaneous switches that *do* affect the behavior of the fragment shader.
// The renderContext may add some of these, and a backend may also add them to a
// shader key if it wants to implement the behavior.
enum class ShaderMiscFlags : uint32_t
{
    none = 0,

    // InterlockMode::atomics only (without advanced blend). Render color to a
    // standard attachment instead of PLS. The backend implementation is
    // responsible to turn on src-over blending.
    fixedFunctionColorOutput = 1 << 0,

    // Override all paths' fill rules (winding or even/odd) with an experimental
    // "clockwise" fill rule, where only regions with a positive winding number
    // get filled.
    clockwiseFill = 1 << 1,

    // clockwiseAtomic mode only. This shader is a prepass that only subtracts
    // (counterclockwise) borrowed coverage from the coverage buffer. It doesn't
    // output color or clip.
    borrowedCoveragePrepass = 1 << 2,

    // DrawType::atomicInitialize only. Also store the color clear value to PLS
    // when drawing a clear, in addition to clearing the other PLS planes.
    storeColorClear = 1 << 3,

    // DrawType::atomicInitialize only. Swizzle the existing framebuffer
    // contents from BGRA to RGBA. (For when this data had to get copied from a
    // BGRA target.)
    swizzleColorBGRAToRGBA = 1 << 4,

    // DrawType::atomicResolve only. Optimization for when rendering to an
    // offscreen texture.
    //
    // It renders the final "resolve" operation directly to the renderTarget in
    // a single pass, instead of (1) resolving the offscreen texture, and then
    // (2) copying the offscreen texture to back the renderTarget.
    coalescedResolveAndTransfer = 1 << 5,
};
RIVE_MAKE_ENUM_BITSET(ShaderMiscFlags)

constexpr static ShaderFeatures ShaderFeaturesMaskFor(
    DrawType drawType,
    InterlockMode interlockMode)
{
    ShaderFeatures mask = ShaderFeatures::NONE;
    switch (drawType)
    {
        case DrawType::imageRect:
        case DrawType::imageMesh:
        case DrawType::atlasBlit:
            if (interlockMode != gpu::InterlockMode::atomics)
            {
                mask = ShaderFeatures::ENABLE_CLIPPING |
                       ShaderFeatures::ENABLE_CLIP_RECT |
                       ShaderFeatures::ENABLE_ADVANCED_BLEND |
                       ShaderFeatures::ENABLE_HSL_BLEND_MODES;
                break;
            }
            // Since atomic mode has to resolve previous draws, images need to
            // consider the same shader features for path draws.
            [[fallthrough]];
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
        case DrawType::interiorTriangulation:
        case DrawType::atomicResolve:
            mask = kAllShaderFeatures;
            break;
        case DrawType::atomicInitialize:
            assert(interlockMode == gpu::InterlockMode::atomics);
            mask = ShaderFeatures::ENABLE_CLIPPING |
                   ShaderFeatures::ENABLE_ADVANCED_BLEND;
            break;
        case DrawType::stencilClipReset:
            mask = ShaderFeatures::NONE;
            break;
    }
    return mask & ShaderFeaturesMaskFor(interlockMode);
}

// Returns a unique value that can be used to key a shader.
uint32_t ShaderUniqueKey(DrawType,
                         ShaderFeatures,
                         InterlockMode,
                         ShaderMiscFlags);

extern const char* GetShaderFeatureGLSLName(ShaderFeatures feature);

// Flags indicating the contents of a draw. These don't affect shaders, but in
// msaa mode they are needed to break up batching. (msaa needs different
// stencil/blend state, depending on the DrawContents.)
//
// These also affect the draw sort order, so we attempt associate more expensive
// shader branch misses with higher flags.
enum class DrawContents
{
    none = 0,
    opaquePaint = 1 << 0,
    // Put feathered fills down low because they only need to draw different
    // geometry, which isn't really a context switch at all.
    featheredFill = 1 << 1,
    stroke = 1 << 2,
    clockwiseFill = 1 << 3,
    nonZeroFill = 1 << 4,
    evenOddFill = 1 << 5,
    activeClip = 1 << 6,
    clipUpdate = 1 << 7,
    advancedBlend = 1 << 8,
};
RIVE_MAKE_ENUM_BITSET(DrawContents)

// A nestedClip draw updates the clip buffer while simultaneously clipping
// against the outerClip that is currently in the clip buffer.
constexpr static gpu::DrawContents kNestedClipUpdateMask =
    (gpu::DrawContents::activeClip | gpu::DrawContents::clipUpdate);

// Low-level batch of geometry to submit to the GPU.
struct DrawBatch
{
    DrawBatch(DrawType drawType_,
              gpu::ShaderMiscFlags shaderMiscFlags_,
              uint32_t elementCount_,
              uint32_t baseElement_,
              rive::BlendMode blendMode) :
        drawType(drawType_),
        shaderMiscFlags(shaderMiscFlags_),
        elementCount(elementCount_),
        baseElement(baseElement_),
        firstBlendMode(blendMode)
    {}

    const DrawType drawType;
    const ShaderMiscFlags shaderMiscFlags;
    uint32_t elementCount; // Vertex, index, or instance count.
    uint32_t baseElement;  // Base vertex, index, or instance.
    rive::BlendMode firstBlendMode;
    DrawContents drawContents = DrawContents::none;
    ShaderFeatures shaderFeatures = ShaderFeatures::NONE;
    bool needsBarrier = false; // Pixel-local-storage barrier required after
                               // submitting this batch.

    // DrawType::imageRect and DrawType::imageMesh.
    uint32_t imageDrawDataOffset = 0;
    const Texture* imageTexture = nullptr;

    // DrawType::imageMesh.
    RenderBuffer* vertexBuffer;
    RenderBuffer* uvBuffer;
    RenderBuffer* indexBuffer;

    // When shaders don't have a mechanism to read the framebuffer (e.g.,
    // WebGL msaa), this is a linked list of all the draws in the batch whose
    // bounding boxes needs to be blitted to the "dstRead" texture before
    // drawing.
    const Draw* dstReadList = nullptr;
};

// Simple gradients only have 2 texels, so we write them to mapped texture
// memory from the CPU instead of rendering them.
struct TwoTexelRamp
{
    ColorInt color0, color1;
};
static_assert(sizeof(TwoTexelRamp) == 8 * sizeof(uint8_t));

// Detailed description of exactly how a RenderContextImpl should bind its
// buffers and draw a flush. A typical flush is done in 4 steps:
//
//  1. Render the complex gradients from the gradSpanBuffer to the gradient
//     texture (gradSpanCount, firstComplexGradSpan, complexGradRowsTop,
//     complexGradRowsHeight).
//
//  2. Transfer the simple gradient texels from the simpleColorRampsBuffer to
//     the top of the gradient texture (simpleGradTexelsWidth,
//     simpleGradTexelsHeight, simpleGradDataOffsetInBytes, tessDataHeight).
//
//  3. Render the tessellation texture from the tessVertexSpanBuffer
//     (tessVertexSpanCount, firstTessVertexSpan).
//
//  4. Execute the drawList, reading from the newly rendered resource textures.
//
struct FlushDescriptor
{
    RenderTarget* renderTarget = nullptr;
    ShaderFeatures combinedShaderFeatures = ShaderFeatures::NONE;
    InterlockMode interlockMode = InterlockMode::rasterOrdering;
    int msaaSampleCount = 0; // (0 unless interlockMode is msaa.)

    LoadAction colorLoadAction = LoadAction::clear;
    ColorInt colorClearValue = 0; // When loadAction == LoadAction::clear.
    uint32_t coverageClearValue = 0;
    float depthClearValue = DEPTH_MAX;
    ColorInt stencilClearValue = STENCIL_CLEAR;

    IAABB renderTargetUpdateBounds; // drawBounds, or renderTargetBounds if
                                    // loadAction == LoadAction::clear.

    // Physical size of the atlas texture.
    uint16_t atlasTextureWidth;
    uint16_t atlasTextureHeight;

    // Boundaries of the content for this specific flush within the atlas
    // texture.
    uint16_t atlasContentWidth;
    uint16_t atlasContentHeight;

    // Monotonically increasing prefix that gets appended to the most
    // significant "32 - CLOCKWISE_COVERAGE_BIT_COUNT" bits of coverage buffer
    // values.
    //
    // The coverage buffer is used in clockwiseAtomic mode.
    //
    // Increasing this prefix implicitly clears the entire coverage buffer to
    // zero.
    uint32_t coverageBufferPrefix = 0;

    // (clockwiseAtomic mode only.) We usually don't have to clear the coverage
    // buffer because of coverageBufferPrefix, but when this value is true, the
    // entire coverage buffer must be cleared to zero before rendering.
    bool needsCoverageBufferClear = false;

    size_t flushUniformDataOffsetInBytes = 0;
    uint32_t pathCount = 0;
    size_t firstPath = 0;
    size_t firstPaint = 0;
    size_t firstPaintAux = 0;
    uint32_t contourCount = 0;
    size_t firstContour = 0;
    uint32_t gradSpanCount = 0;
    size_t firstGradSpan = 0;
    uint32_t tessVertexSpanCount = 0;
    size_t firstTessVertexSpan = 0;
    uint32_t gradDataHeight = 0;
    uint32_t tessDataHeight = 0;
    // Override path fill rules with "clockwise".
    bool clockwiseFillOverride = false;
    bool hasTriangleVertices = false;
    bool wireframe = false;
    bool isFinalFlushOfFrame = false;

    // Command buffer that rendering commands will be added to.
    //  - VkCommandBuffer on Vulkan.
    //  - id<MTLCommandBuffer> on Metal.
    //  - Unused otherwise.
    void* externalCommandBuffer = nullptr;

    // List of feathered fills (if any) that must be rendered to the atlas
    // before the main render pass.
    const AtlasDrawBatch* atlasFillBatches = nullptr;
    size_t atlasFillBatchCount = 0;

    // List of feathered strokes (if any) that must be rendered to the atlas
    // before the main render pass.
    const AtlasDrawBatch* atlasStrokeBatches = nullptr;
    size_t atlasStrokeBatchCount = 0;

    // List of draws in the main render pass. These are rendered directly to the
    // renderTarget.
    const BlockAllocatedLinkedList<DrawBatch>* drawList = nullptr;
};

// Returns the smallest number that can be added to 'value', such that 'value %
// alignment' == 0.
template <uint32_t Alignment>
RIVE_ALWAYS_INLINE uint32_t PaddingToAlignUp(size_t value)
{
    constexpr size_t maxMultipleOfAlignment =
        std::numeric_limits<size_t>::max() / Alignment * Alignment;
    uint32_t padding = (maxMultipleOfAlignment - value) % Alignment;
    assert((value + padding) % Alignment == 0);
    return padding;
}

// Returns the area of the (potentially non-rectangular) quadrilateral that
// results from transforming the given bounds by the given matrix.
float find_transformed_area(const AABB& bounds, const Mat2D&);

// Convert a BlendMode to the tightly-packed range used by PLS shaders.
uint32_t ConvertBlendModeToPLSBlendMode(BlendMode riveMode);

// Swizzles the byte order of ColorInt to litte-endian RGBA (the order expected
// by GLSL).
RIVE_ALWAYS_INLINE uint32_t SwizzleRiveColorToRGBA(ColorInt riveColor)
{
    return (riveColor & 0xff00ff00) |
           (math::rotateleft32(riveColor, 16) & 0x00ff00ff);
}

// Swizzles the byte order of ColorInt to litte-endian RGBA (the order expected
// by GLSL), and premultiplies alpha.
uint32_t SwizzleRiveColorToRGBAPremul(ColorInt riveColor);

// Used for fields that are used to layout write-only mapped GPU memory.
// "volatile" to discourage the compiler from generating code that reads these
// values (e.g., don't let the compiler generate "x ^= x" instead of "x = 0").
// "RIVE_MAYBE_UNUSED" to suppress -Wunused-private-field.
#define WRITEONLY RIVE_MAYBE_UNUSED volatile

// Per-flush shared uniforms used by all shaders.
struct FlushUniforms
{
public:
    FlushUniforms(const FlushDescriptor&, const PlatformFeatures&);

    FlushUniforms(const FlushUniforms& other) { *this = other; }

    void operator=(const FlushUniforms& rhs)
    {
        memcpy(this, &rhs, sizeof(*this) - sizeof(m_padTo256Bytes));
    }

    bool operator!=(const FlushUniforms& rhs) const
    {
        return memcmp(this, &rhs, sizeof(*this) - sizeof(m_padTo256Bytes)) != 0;
    }

private:
    class InverseViewports
    {
    public:
        InverseViewports() = default;

        InverseViewports(const FlushDescriptor&, const PlatformFeatures&);

    private:
        // [complexGradientsY, tessDataY, renderTargetX,  renderTargetY]
        WRITEONLY float m_vals[4];
    };

    WRITEONLY InverseViewports m_inverseViewports;
    WRITEONLY uint32_t m_renderTargetWidth;
    WRITEONLY uint32_t m_renderTargetHeight;
    // Only used if clears are implemented as draws.
    WRITEONLY uint32_t m_colorClearValue;
    // Only used if clears are implemented as draws.
    WRITEONLY uint32_t m_coverageClearValue;
    // drawBounds, or renderTargetBounds if there is a clear. (Used by the
    // "@RESOLVE_PLS" step in InterlockMode::atomics.)
    WRITEONLY IAABB m_renderTargetUpdateBounds;
    WRITEONLY Vec2D m_atlasTextureInverseSize; // 1 / [atlasWidth, atlasHeight]
    WRITEONLY Vec2D m_atlasContentInverseViewport; // 2 / atlasContentBounds
    // Monotonically increasing prefix that gets appended to the most
    // significant "32 - CLOCKWISE_COVERAGE_BIT_COUNT" bits of coverage buffer
    // values. (clockwiseAtomic mode only.)
    WRITEONLY uint32_t m_coverageBufferPrefix;
    // Spacing between adjacent path IDs (1 if IEEE compliant).
    WRITEONLY uint32_t m_pathIDGranularity;
    WRITEONLY float m_vertexDiscardValue;
    WRITEONLY uint32_t m_wireframeEnabled; // Forces coverage to solid.
    // Uniform blocks must be multiples of 256 bytes in size.
    WRITEONLY uint8_t m_padTo256Bytes[256 - 80];
};
static_assert(sizeof(FlushUniforms) == 256);

// Storage buffers are logically layed out as arrays of structs on the CPU, but
// the GPU shaders access them as arrays of basic types. We do it this way in
// order to be able to easily polyfill them with textures.
//
// This enum defines the underlying basic type that each storage buffer struct
// is layed on top of.
enum StorageBufferStructure
{
    uint32x4,
    uint32x2,
    float32x4,
};

constexpr static uint32_t StorageBufferElementSizeInBytes(
    StorageBufferStructure bufferStructure)
{
    switch (bufferStructure)
    {
        case StorageBufferStructure::uint32x4:
            return sizeof(uint32_t) * 4;
        case StorageBufferStructure::uint32x2:
            return sizeof(uint32_t) * 2;
        case StorageBufferStructure::float32x4:
            return sizeof(float) * 4;
    }
    RIVE_UNREACHABLE();
}

// Defines a transform from screen space into a region of the atlas.
// The atlas may have a different scale factor than the screen.
struct AtlasTransform
{
    float scaleFactor;
    float translateX;
    float translateY;
};

// Defines a sub-allocation for a path's coverage data within the
// renderContext's coverage buffer. (clockwiseAtomic mode only.)
struct CoverageBufferRange
{
    // Index of the first pixel of this allocation within the coverage buffer.
    // Must be a multiple of 32*32.
    uint32_t offset;
    // Line width in pixels of the image in this coverage allocation.
    // Must be a multiple of 32.
    uint32_t pitch;
    // Offset from screen space to image coords within the coverage allocation.
    float offsetX;
    float offsetY;
};

// High level structure of the "path" storage buffer. Each path has a unique
// data record on the GPU that is accessed from the vertex shader.
struct PathData
{
public:
    constexpr static StorageBufferStructure kBufferStructure =
        StorageBufferStructure::uint32x4;

    void set(const Mat2D&,
             float strokeRadius,
             float featherRadius,
             uint32_t zIndex,
             const AtlasTransform&,
             const CoverageBufferRange&);

private:
    WRITEONLY float m_matrix[6];
    // "0" indicates that the path is filled, not stroked.
    WRITEONLY float m_strokeRadius;
    WRITEONLY float m_featherRadius;
    // InterlockMode::msaa.
    WRITEONLY uint32_t m_zIndex;
    // Only used when rendering coverage via the atlas.
    WRITEONLY AtlasTransform m_atlasTransform;
    // InterlockMode::clockwiseAtomic.
    WRITEONLY CoverageBufferRange m_coverageBufferRange;
};
static_assert(sizeof(PathData) ==
              StorageBufferElementSizeInBytes(PathData::kBufferStructure) * 4);
static_assert(256 % sizeof(PathData) == 0);
constexpr static size_t kPathBufferAlignmentInElements = 256 / sizeof(PathData);

// High level structure of the "paint" storage buffer. Each path also has a data
// small record describing its paint at a high level. Complex paints (gradients,
// images, or any path with a clipRect) store additional rendering info in the
// PaintAuxData buffer.
struct PaintData
{
public:
    constexpr static StorageBufferStructure kBufferStructure =
        StorageBufferStructure::uint32x2;

    void set(DrawContents singleDrawContents,
             PaintType,
             SimplePaintValue,
             GradTextureLayout,
             uint32_t clipID,
             bool hasClipRect,
             BlendMode);

private:
    WRITEONLY uint32_t m_params; // [clipID, flags, paintType]
    union
    {
        WRITEONLY uint32_t m_color;     // PaintType::solidColor
        WRITEONLY float m_gradTextureY; // Paintype::linearGradient,
                                        // Paintype::radialGradient
        WRITEONLY float m_opacity;      // PaintType::image
        WRITEONLY uint32_t m_shiftedClipReplacementID; // PaintType::clipUpdate
    };
};
static_assert(sizeof(PaintData) ==
              StorageBufferElementSizeInBytes(PaintData::kBufferStructure));
static_assert(256 % sizeof(PaintData) == 0);
constexpr static size_t kPaintBufferAlignmentInElements =
    256 / sizeof(PaintData);

// Structure of the "paintAux" storage buffer. Gradients, images, and clipRects
// store their details here, indexed by pathID.
struct PaintAuxData
{
public:
    constexpr static StorageBufferStructure kBufferStructure =
        StorageBufferStructure::float32x4;

    void set(const Mat2D& viewMatrix,
             PaintType,
             SimplePaintValue,
             const Gradient*,
             const Texture*,
             const ClipRectInverseMatrix*,
             const RenderTarget*,
             const gpu::PlatformFeatures&);

private:
    WRITEONLY float m_matrix[6]; // Maps _fragCoord to paint coordinates.
    union
    {
        WRITEONLY float
            m_gradTextureHorizontalSpan[2]; // Paintype::linearGradient,
                                            // Paintype::radialGradient
        WRITEONLY float m_imageTextureLOD;  // PaintType::image
    };

    WRITEONLY float m_clipRectInverseMatrix[6]; // Maps _fragCoord to normalized
                                                // clipRect coords.
    WRITEONLY Vec2D m_inverseFwidth; // -1 / fwidth(matrix * _fragCoord) -- for
                                     // antialiasing.
};
static_assert(sizeof(PaintAuxData) ==
              StorageBufferElementSizeInBytes(PaintAuxData::kBufferStructure) *
                  4);
static_assert(256 % sizeof(PaintAuxData) == 0);
constexpr static size_t kPaintAuxBufferAlignmentInElements =
    256 / sizeof(PaintAuxData);

// High level structure of the "contour" storage buffer. Each contour of every
// path has a data record describing its info.
struct ContourData
{
public:
    constexpr static StorageBufferStructure kBufferStructure =
        StorageBufferStructure::uint32x4;

    ContourData(Vec2D midpoint, uint32_t pathID, uint32_t vertexIndex0) :
        m_midpoint(midpoint), m_pathID(pathID), m_vertexIndex0(vertexIndex0)
    {}

private:
    WRITEONLY Vec2D
        m_midpoint; // Midpoint of the curve endpoints in just this contour.
    WRITEONLY uint32_t m_pathID; // ID of the path this contour belongs to.
    WRITEONLY uint32_t m_vertexIndex0; // Index of the first tessellation vertex
                                       // of the contour.
};
static_assert(sizeof(ContourData) ==
              StorageBufferElementSizeInBytes(ContourData::kBufferStructure));
static_assert(256 % sizeof(ContourData) == 0);
constexpr static size_t kContourBufferAlignmentInElements =
    256 / sizeof(ContourData);

// Per-vertex data for shaders that draw triangles.
struct TriangleVertex
{
public:
    TriangleVertex() = default;
    TriangleVertex(Vec2D point, int16_t weight, uint16_t pathID) :
        m_point(point),
        m_weight_pathID((static_cast<int32_t>(weight) << 16) | pathID)
    {}

#ifdef TESTING
    Vec2D testing_point() const { return {m_point.x, m_point.y}; }
    int32_t testing_weight_pathID() const { return m_weight_pathID; }
#endif

private:
    WRITEONLY Vec2D m_point;
    WRITEONLY int32_t m_weight_pathID; // [(weight << 16 | pathID]
};
static_assert(sizeof(TriangleVertex) == sizeof(float) * 3);

// Per-draw uniforms used by image meshes.
struct ImageDrawUniforms
{
public:
    ImageDrawUniforms() = default;

    ImageDrawUniforms(const Mat2D&,
                      float opacity,
                      const ClipRectInverseMatrix*,
                      uint32_t clipID,
                      BlendMode,
                      uint32_t zIndex);

private:
    WRITEONLY float m_matrix[6];
    WRITEONLY float m_opacity;
    WRITEONLY float m_padding = 0;
    WRITEONLY float m_clipRectInverseMatrix[6];
    WRITEONLY uint32_t m_clipID;
    WRITEONLY uint32_t m_blendMode;
    WRITEONLY uint32_t m_zIndex; // gpu::InterlockMode::msaa only.
    // Uniform blocks must be multiples of 256 bytes in size.
    WRITEONLY uint8_t m_padTo256Bytes[256 - 68];

    constexpr void staticChecks()
    {
        static_assert(offsetof(ImageDrawUniforms, m_matrix) % 16 == 0);
        static_assert(
            offsetof(ImageDrawUniforms, m_clipRectInverseMatrix) % 16 == 0);
        static_assert(sizeof(ImageDrawUniforms) == 256);
    }
};

#undef WRITEONLY

// The maximum number of storage buffers we will ever use in a vertex or
// fragment shader.
constexpr static size_t kMaxStorageBuffers = 4;

// If the backend doesn't support "kMaxStorageBuffers" a shader, we polyfill
// with textures. This function returns the dimensions to use for these
// textures.
std::tuple<uint32_t, uint32_t> StorageTextureSize(size_t bufferSizeInBytes,
                                                  StorageBufferStructure);

// If the backend doesn't support "kMaxStorageBuffers" in a shader, we polyfill
// with textures. The polyfill texture needs to be updated in entire rows at a
// time, meaning, its transfer buffer might need to be larger than requested.
// This function returns a size that is large enough to service a worst-case
// texture update.
size_t StorageTextureBufferSize(size_t bufferSizeInBytes,
                                StorageBufferStructure);

// Should the triangulator emit triangles with negative winding, positive
// winding, or both?
enum class WindingFaces
{
    negative = 1 << 0,
    positive = 1 << 1,
    all = negative | positive,
};
RIVE_MAKE_ENUM_BITSET(WindingFaces)

// Represents a block of mapped GPU memory. Since it can be extremely expensive
// to read mapped memory, we use this class to enforce the write-only nature of
// this memory.
template <typename T> class WriteOnlyMappedMemory
{
public:
    WriteOnlyMappedMemory() { reset(); }
    WriteOnlyMappedMemory(T* ptr, size_t elementCount)
    {
        reset(ptr, elementCount);
    }

    void reset() { reset(nullptr, 0); }

    void reset(T* ptr, size_t elementCount)
    {
        m_mappedMemory = ptr;
        m_nextMappedItem = ptr;
        m_mappingEnd = ptr + elementCount;
    }

    using MapResourceBufferFn =
        void* (RenderContextImpl::*)(size_t mapSizeInBytes);
    void mapElements(RenderContextImpl* impl,
                     MapResourceBufferFn mapFn,
                     size_t elementCount)
    {
        void* ptr = (impl->*mapFn)(elementCount * sizeof(T));
        reset(reinterpret_cast<T*>(ptr), elementCount);
    }

    operator bool() const { return m_mappedMemory; }

    // How many bytes have been written to the buffer?
    size_t bytesWritten() const
    {
        return reinterpret_cast<uintptr_t>(m_nextMappedItem) -
               reinterpret_cast<uintptr_t>(m_mappedMemory);
    }

    size_t elementsWritten() const { return bytesWritten() / sizeof(T); }

    // Is there room to push() itemCount items to the buffer?
    bool hasRoomFor(size_t itemCount)
    {
        return m_nextMappedItem + itemCount <= m_mappingEnd;
    }

    // Append and write a new item to the buffer. In order to enforce the
    // write-only requirement of a mapped buffer, these methods do not return
    // any pointers to the client.
    template <typename... Args>
    RIVE_ALWAYS_INLINE void emplace_back(Args&&... args)
    {
        new (&push()) T(std::forward<Args>(args)...);
    }
    template <typename... Args> RIVE_ALWAYS_INLINE void set_back(Args&&... args)
    {
        push().set(std::forward<Args>(args)...);
    }
    void push_back_n(const T* values, size_t count)
    {
        T* dst = push(count);
        if (values != nullptr)
        {
            memcpy(dst, values, count * sizeof(T));
        }
    }
    void skip_back() { push(); }

private:
    RIVE_ALWAYS_INLINE T& push()
    {
        assert(hasRoomFor(1));
        return *m_nextMappedItem++;
    }
    RIVE_ALWAYS_INLINE T* push(size_t count)
    {
        assert(hasRoomFor(count));
        T* ret = m_nextMappedItem;
        m_nextMappedItem += count;
        return ret;
    }

    T* m_mappedMemory;
    T* m_nextMappedItem;
    const T* m_mappingEnd;
};

// Utility for tracking booleans that may be unknown (e.g., lazily computed
// values, GL state, etc.)
enum class TriState
{
    no,
    yes,
    unknown
};

// These tables integrate the gaussian function, and its inverse, covering a
// spread of -FEATHER_TEXTURE_STDDEVS to +FEATHER_TEXTURE_STDDEVS.
constexpr static uint32_t GAUSSIAN_TABLE_SIZE = 512;
extern const uint16_t g_gaussianIntegralTableF16[GAUSSIAN_TABLE_SIZE];
extern const uint16_t g_inverseGaussianIntegralTableF16[GAUSSIAN_TABLE_SIZE];

float4 cast_f16_to_f32(uint16x4 x);
uint16x4 cast_f32_to_f16(float4);

// Code to generate g_gaussianIntegralTableF16 and
// g_inverseGaussianIntegralTableF32. This is left in the codebase but #ifdef'd
// out in case we ever want to change any parameters of the built-in tables.
#if 0
void generate_gausian_integral_table(float (&)[GAUSSIAN_TABLE_SIZE]);
void generate_inverse_gausian_integral_table(float (&)[GAUSSIAN_TABLE_SIZE]);
#endif
} // namespace rive::gpu

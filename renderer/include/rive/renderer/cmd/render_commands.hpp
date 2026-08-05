/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/render_handle.hpp"
#include <cstddef>
#include <cstdint>

// The 2D command vocabulary recorded into a RenderCommandBuffer. One
// interleaved stream in record order; replay walks it once, so a resource is
// always created and configured before use. All POD, no pointers.
namespace rive::cmd
{

enum class RenderCmd : uint8_t
{
    // ---- resource creation ----
    // Each make* carries an explicit id and generation so ids can be reused
    // without the consumer table growing unboundedly.
    makePath,           // MakePathPOD (+ rawpath blob)
    makeEmptyPath,      // MakeIdPOD
    makePaint,          // MakeIdPOD
    makeLinearGradient, // LinearGradientPOD (+ colors[]+stops[] blob)
    makeRadialGradient, // RadialGradientPOD (+ colors[]+stops[] blob)
    decodeImage,        // DecodeImagePOD (+ encoded-bytes blob)
    makeBuffer,         // MakeBufferPOD
    bufferData,         // BufferDataPOD (+ data blob), a map()/unmap() write

    // The consumer releases table[id] only when the generation matches, so a
    // stale destroy after slot reuse is harmless.
    destroyResource, // DestroyResourcePOD (kind, id, generation)

    // ---- path mutations ----
    // Per verb builder calls accumulate into a scratch RawPath flushed as
    // pathAddRawPath, so there are no per verb commands in the stream.
    pathRewind,        // ResId
    pathFillRule,      // PathFillRulePOD
    pathAddRawPath,    // PathRawPOD (+ rawpath blob)
    pathAddRenderPath, // PathAddPathPOD

    // ---- paint mutations ----
    paintStyle,            // PaintU8POD
    paintColor,            // PaintColorPOD
    paintThickness,        // PaintFloatPOD
    paintJoin,             // PaintU8POD
    paintCap,              // PaintU8POD
    paintFeather,          // PaintFloatPOD
    paintBlendMode,        // PaintU8POD
    paintShader,           // PaintShaderPOD
    paintInvalidateStroke, // ResId

    // ---- renderer draws ----
    save,            // no payload
    restore,         // no payload
    transform,       // TransformPOD
    drawPath,        // DrawPathPOD
    clipPath,        // ClipPathPOD
    drawImage,       // DrawImagePOD
    drawImageMesh,   // DrawImageMeshPOD
    modulateOpacity, // OpacityPOD

    // ---- render target scheduling ----
    // Canvas content records inline between these brackets; replay redirects
    // it into the canvas's own frame since PLS frames cannot nest.
    canvasContentBegin, // CanvasContentPOD (canvas id, clear color)
    canvasContentEnd,   // ResId (canvas id)
                        // draws that sample it. Opens the screen frame.

    // A drawn resource was mutated again this frame: draws pin the version
    // they saw, replay materializes the outgoing version before applying
    // later mutations.
    resourceNewVersion, // ResourceVersionPOD

    // Keep equal to the last real opcode so replay rejects corrupt type bytes
    // instead of desyncing.
    lastRenderCmd = resourceNewVersion,
};

// A bare resource id payload.
struct ResIdPOD
{
    RenderHandle id;
};

// Which id space a destroyed resource belongs to; an id alone is ambiguous.
enum class ResourceKind : uint8_t
{
    path,
    paint,
    shader,
    image,
    buffer,
};

struct DestroyResourcePOD
{
    uint8_t kind; // ResourceKind
    RenderHandle id;
    uint32_t generation;
};

struct ResourceVersionPOD
{
    uint8_t kind; // ResourceKind
    RenderHandle id;
    uint32_t version;
};

// make* that carries only its id.
struct MakeIdPOD
{
    RenderHandle id;
    uint32_t generation;
};

// Wire PODs stream raw through writeRaw, so layouts carry explicit pad
// fields wherever the 8-aligned 64 bit offsets would otherwise introduce
// implicit (uninitialized) padding.
struct MakePathPOD
{
    RenderHandle id;
    uint32_t generation;
    uint64_t blobOffset;   // verbs
    uint64_t pointsOffset; // points (own blob, 8-aligned)
    uint32_t verbCount;
    uint32_t pointCount;
    uint32_t fillRule;
    uint32_t pad;
};
static_assert(sizeof(MakePathPOD) == 10 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

struct LinearGradientPOD
{
    RenderHandle id;
    uint32_t generation;
    float sx, sy, ex, ey;
    uint64_t blobOffset;  // colors
    uint64_t stopsOffset; // stops (own blob, 8-aligned)
    uint32_t count;
    uint32_t pad;
};
static_assert(sizeof(LinearGradientPOD) == 12 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

struct RadialGradientPOD
{
    RenderHandle id;
    uint32_t generation;
    float cx, cy, radius;
    uint32_t count;
    uint64_t blobOffset;  // colors
    uint64_t stopsOffset; // stops (own blob, 8-aligned)
};
static_assert(sizeof(RadialGradientPOD) == 10 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

struct PathFillRulePOD
{
    RenderHandle path;
    uint8_t fillRule;
};

struct PathRawPOD
{
    uint64_t blobOffset;   // verbs
    uint64_t pointsOffset; // points (own blob, 8-aligned)
    RenderHandle path;
    uint32_t verbCount;
    uint32_t pointCount;
    uint32_t pad;
};
static_assert(sizeof(PathRawPOD) == 8 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

struct PathAddPathPOD
{
    RenderHandle path;
    RenderHandle src;
    float xx, xy, yx, yy, tx, ty; // Mat2D
};

struct PaintU8POD
{
    RenderHandle paint;
    uint8_t value;
};

struct PaintColorPOD
{
    RenderHandle paint;
    uint32_t color;
};

struct PaintFloatPOD
{
    RenderHandle paint;
    float value;
};

struct PaintShaderPOD
{
    RenderHandle paint;
    RenderHandle shader; // kInvalidRenderHandle clears the shader
};

struct TransformPOD
{
    float xx, xy, yx, yy, tx, ty; // Mat2D
};

struct DrawPathPOD
{
    RenderHandle path;
    RenderHandle paint;
    uint32_t pathVersion;
    uint32_t paintVersion;
};

struct ClipPathPOD
{
    RenderHandle path;
    uint32_t version;
};

struct DecodeImagePOD
{
    RenderHandle id;
    uint32_t generation;
    uint64_t blobOffset;
    uint32_t byteCount;
    uint32_t width;
    uint32_t height;
    uint32_t pad;
};
static_assert(sizeof(DecodeImagePOD) == 8 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

struct MakeBufferPOD
{
    RenderHandle id;
    uint32_t generation;
    uint8_t bufferType; // RenderBufferType
    uint8_t flags;      // RenderBufferFlags
    uint32_t sizeInBytes;
};

struct BufferDataPOD
{
    uint64_t blobOffset;
    RenderHandle buffer;
    uint32_t size;
};
static_assert(sizeof(BufferDataPOD) == 4 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

struct DrawImagePOD
{
    RenderHandle image;
    uint8_t wrapX, wrapY, filter; // ImageSampler
    uint8_t blendMode;
    float opacity;
};

struct DrawImageMeshPOD
{
    RenderHandle image;
    RenderHandle vertices, uvCoords, indices;
    uint32_t vertexVersion, uvVersion, indexVersion;
    uint32_t vertexCount, indexCount;
    uint8_t wrapX, wrapY, filter; // ImageSampler
    uint8_t blendMode;
    float opacity;
};

struct OpacityPOD
{
    float opacity;
};

struct CanvasContentPOD
{
    RenderHandle
        canvasId; // flagged canvas id (kCanvasHandleFlag), as in drawImage
    uint32_t clearColor; // ARGB; the canvas frame's clear (script-controlled)
};

// Opcode to payload table, one X(opcode, POD) per command; void means no
// payload. Every size a skip or filter walk uses derives from here, so a new
// command cannot desync them. Blobs ride separately and never affect sizes.
#define RIVE_RENDER_CMD_TABLE(X)                                               \
    X(makePath, MakePathPOD)                                                   \
    X(makeEmptyPath, MakeIdPOD)                                                \
    X(makePaint, MakeIdPOD)                                                    \
    X(makeLinearGradient, LinearGradientPOD)                                   \
    X(makeRadialGradient, RadialGradientPOD)                                   \
    X(decodeImage, DecodeImagePOD)                                             \
    X(makeBuffer, MakeBufferPOD)                                               \
    X(bufferData, BufferDataPOD)                                               \
    X(destroyResource, DestroyResourcePOD)                                     \
    X(pathRewind, ResIdPOD)                                                    \
    X(pathFillRule, PathFillRulePOD)                                           \
    X(pathAddRawPath, PathRawPOD)                                              \
    X(pathAddRenderPath, PathAddPathPOD)                                       \
    X(paintStyle, PaintU8POD)                                                  \
    X(paintColor, PaintColorPOD)                                               \
    X(paintThickness, PaintFloatPOD)                                           \
    X(paintJoin, PaintU8POD)                                                   \
    X(paintCap, PaintU8POD)                                                    \
    X(paintFeather, PaintFloatPOD)                                             \
    X(paintBlendMode, PaintU8POD)                                              \
    X(paintShader, PaintShaderPOD)                                             \
    X(paintInvalidateStroke, ResIdPOD)                                         \
    X(save, void)                                                              \
    X(restore, void)                                                           \
    X(transform, TransformPOD)                                                 \
    X(drawPath, DrawPathPOD)                                                   \
    X(clipPath, ClipPathPOD)                                                   \
    X(drawImage, DrawImagePOD)                                                 \
    X(drawImageMesh, DrawImageMeshPOD)                                         \
    X(modulateOpacity, OpacityPOD)                                             \
    X(canvasContentBegin, CanvasContentPOD)                                    \
    X(canvasContentEnd, ResIdPOD)                                              \
    X(resourceNewVersion, ResourceVersionPOD)

namespace detail
{
template <typename POD> constexpr size_t payloadSizeOfPOD()
{
    return sizeof(POD);
}
template <> constexpr size_t payloadSizeOfPOD<void>() { return 0; }
} // namespace detail

constexpr size_t payloadSizeOf(RenderCmd c)
{
    switch (c)
    {
#define RIVE_RENDER_CMD_SIZE_CASE(cmd, POD)                                    \
    case RenderCmd::cmd:                                                       \
        return detail::payloadSizeOfPOD<POD>();
        RIVE_RENDER_CMD_TABLE(RIVE_RENDER_CMD_SIZE_CASE)
#undef RIVE_RENDER_CMD_SIZE_CASE
    }
    return 0;
}

} // namespace rive::cmd

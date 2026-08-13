/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/cmd/ore_handle.hpp"
#include "rive/renderer/ore/cmd/ore_resource_commands.hpp"
#include <cstddef>
#include <cstdint>

// Recorded form of the ore RenderPass interface, written as [opcode][POD]
// into a flat byte stream. Structs hold no pointers, so a recorded buffer is
// movable across threads and doubles as the silver artifact. Resources are
// referenced by ResourceHandle.
namespace rive::ore::cmd
{

enum class CommandType : uint32_t
{
    beginRenderPass,
    setPipeline,
    setVertexBuffer,
    setIndexBuffer,
    setBindGroup,
    setViewport,
    setScissorRect,
    setStencilReference,
    setBlendColor,
    draw,
    drawIndexed,
    finish,

    // Resource lifecycle interleaved in stream order, so a create precedes
    // every use and id reuse is safe on the consumer.
    makeBuffer,
    makeTexture,
    makeSampler,
    makeShaderModule,
    makeBindGroupLayout,
    makeTextureView,
    makePipeline,
    makeBindGroup,
    bufferUpdate,
    textureUpload,
    destroyResource,
    // Reserve a canvas view; the consumer wraps at replay. No device touch on
    // record.
    wrapCanvasView,
};

// Precedes each make descriptor; the consumer stores the real resource at
// {id, generation}.
struct MakeResourcePOD
{
    ResourceHandle id;
    uint32_t generation;
};

struct BufferUpdatePOD
{
    ResourceHandle handle;
    uint32_t offset;
    BlobRef bytes;
};

// Fields are fixed width so the layout is identical on 32 bit wasm and 64 bit
// native.
struct TextureUploadPOD
{
    ResourceHandle handle;
    uint32_t bytesPerRow;
    uint32_t rowsPerImage;
    uint32_t mipLevel;
    uint32_t layer;
    uint32_t x, y, z;
    uint32_t width, height, depth;
    uint32_t pad; // keeps the 8-aligned BlobRef free of implicit padding
    BlobRef bytes;
};
static_assert(sizeof(TextureUploadPOD) == 16 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

// Selects how the consumer wraps a reserved canvas view at replay.
enum class WrapCanvasViewMode : uint32_t
{
    colorView = 0,  // the canvas's own render target view
    sampleView = 1, // sampling wrap, on GL needs the top up mirror
    imageView = 2,  // decoded image, canvasId carries the 2D image id
};

struct WrapCanvasViewPOD
{
    ResourceHandle id;
    uint32_t generation;
    uint32_t canvasId; // canvas id, or the 2D image id for imageView
    uint32_t mode;     // WrapCanvasViewMode
};

// The consumer clears the slot only when the generation matches, so a stale
// destroy for a recycled id is ignored.
struct DestroyResourcePOD
{
    ResourceHandle handle;
    uint32_t generation;
};

// resolveTarget == kInvalidHandle means none.
struct ColorAttachmentPOD
{
    ResourceHandle view;
    ResourceHandle resolveTarget;
    float clearR;
    float clearG;
    float clearB;
    float clearA;
    LoadOp loadOp;
    StoreOp storeOp;
    uint8_t pad[2];
};
static_assert(sizeof(ColorAttachmentPOD) == 7 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

// view == kInvalidHandle means no depth stencil attachment.
struct DepthStencilAttachmentPOD
{
    ResourceHandle view;
    float depthClearValue;
    uint32_t stencilClearValue;
    LoadOp depthLoadOp;
    StoreOp depthStoreOp;
    LoadOp stencilLoadOp;
    StoreOp stencilStoreOp;
};
static_assert(sizeof(DepthStencilAttachmentPOD) == 4 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

// Fixed 4 slot color array keeps the command a flat POD.
struct BeginRenderPassCmd
{
    uint32_t colorCount;
    ColorAttachmentPOD colors[4];
    DepthStencilAttachmentPOD depthStencil;
};
static_assert(sizeof(BeginRenderPassCmd) == 33 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

struct SetPipelineCmd
{
    ResourceHandle pipeline;
};

struct SetVertexBufferCmd
{
    uint32_t slot;
    ResourceHandle buffer;
    uint32_t offset;
};

// pad trails the real fields so a brace init can leave it out.
struct SetIndexBufferCmd
{
    ResourceHandle buffer;
    uint32_t offset;
    IndexFormat format;
    uint8_t pad[3];
};
static_assert(sizeof(SetIndexBufferCmd) == 3 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

// Dynamic offsets live in the blob arena at dynamicOffsetStart.
struct SetBindGroupCmd
{
    uint32_t groupIndex;
    ResourceHandle bindGroup;
    uint64_t dynamicOffsetStart;
    uint32_t dynamicOffsetCount;
    uint32_t pad;
};
static_assert(sizeof(SetBindGroupCmd) == 6 * sizeof(uint32_t),
              "wire POD must be pointer-free and padding-free");

struct SetViewportCmd
{
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
};

struct SetScissorRectCmd
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

struct SetStencilReferenceCmd
{
    uint32_t ref;
};

struct SetBlendColorCmd
{
    float r;
    float g;
    float b;
    float a;
};

struct DrawCmd
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

struct DrawIndexedCmd
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t baseVertex;
    uint32_t firstInstance;
};

// Intentionally no union node type; readers switch on CommandType and memcpy
// the POD, avoiding the union's worst case padding.

// Opcode to payload table, one X(opcode, POD, DescPOD) per command; void
// means no payload in that slot. make* carries MakeResourcePOD then its
// descriptor. Every size a skip walk uses derives from here, so a new command
// cannot desync it. Blobs ride separately and never affect sizes.
#define RIVE_ORE_CMD_TABLE(X)                                                  \
    X(beginRenderPass, BeginRenderPassCmd, void)                               \
    X(setPipeline, SetPipelineCmd, void)                                       \
    X(setVertexBuffer, SetVertexBufferCmd, void)                               \
    X(setIndexBuffer, SetIndexBufferCmd, void)                                 \
    X(setBindGroup, SetBindGroupCmd, void)                                     \
    X(setViewport, SetViewportCmd, void)                                       \
    X(setScissorRect, SetScissorRectCmd, void)                                 \
    X(setStencilReference, SetStencilReferenceCmd, void)                       \
    X(setBlendColor, SetBlendColorCmd, void)                                   \
    X(draw, DrawCmd, void)                                                     \
    X(drawIndexed, DrawIndexedCmd, void)                                       \
    X(finish, void, void)                                                      \
    X(makeBuffer, MakeResourcePOD, BufferDescPOD)                              \
    X(makeTexture, MakeResourcePOD, TextureDescPOD)                            \
    X(makeSampler, MakeResourcePOD, SamplerDescPOD)                            \
    X(makeShaderModule, MakeResourcePOD, ShaderModuleDescPOD)                  \
    X(makeBindGroupLayout, MakeResourcePOD, BindGroupLayoutDescPOD)            \
    X(makeTextureView, MakeResourcePOD, TextureViewDescPOD)                    \
    X(makePipeline, MakeResourcePOD, PipelineDescPOD)                          \
    X(makeBindGroup, MakeResourcePOD, BindGroupDescPOD)                        \
    X(bufferUpdate, BufferUpdatePOD, void)                                     \
    X(textureUpload, TextureUploadPOD, void)                                   \
    X(destroyResource, DestroyResourcePOD, void)                               \
    X(wrapCanvasView, WrapCanvasViewPOD, void)

namespace detail
{
template <typename POD> constexpr size_t orePayloadSizeOfPOD()
{
    return sizeof(POD);
}
template <> constexpr size_t orePayloadSizeOfPOD<void>() { return 0; }
} // namespace detail

constexpr size_t orePayloadSizeOf(CommandType c)
{
    switch (c)
    {
#define RIVE_ORE_CMD_SIZE_CASE(cmd, POD, DESC)                                 \
    case CommandType::cmd:                                                     \
        return detail::orePayloadSizeOfPOD<POD>() +                            \
               detail::orePayloadSizeOfPOD<DESC>();
        RIVE_ORE_CMD_TABLE(RIVE_ORE_CMD_SIZE_CASE)
#undef RIVE_ORE_CMD_SIZE_CASE
    }
    return 0;
}

} // namespace rive::ore::cmd

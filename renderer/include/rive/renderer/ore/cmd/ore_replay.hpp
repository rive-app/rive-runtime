/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_make_replay.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include <cassert>
#include <functional>
#include <memory>

// Replays a recorded ore command stream against a live Context. Replay drives
// the same virtuals as immediate mode, so one implementation covers every
// backend and the pixels match immediate mode by construction.
namespace rive::ore::cmd
{

// Maps a captured resource to the object replay should use. Identity when
// passes captured real resources; deferred objects remap to the real object.
using ResourceRemap =
    std::function<rive::gpu::GPUResource*(rive::gpu::GPUResource*)>;

// Resolves a render command handle to the object replay should use, so the
// deferred objects recorded against can be discarded after recording.
using HandleResolver = std::function<rive::gpu::GPUResource*(ResourceHandle)>;

// Returns false for a lifecycle opcode. dropDraws poisons the open pass when
// a handle fails to resolve so its draws drop instead of using garbage.
inline bool replayPassCommand(Context& ctx,
                              std::unique_ptr<RenderPass>& pass,
                              bool& dropDraws,
                              CommandType type,
                              OreCommandReader& reader,
                              const OreKindResolve& resolve)
{
    auto churned = [&](const char* what, ResourceHandle h) {
        dropDraws = true;
        RIVE_WARN_THROTTLED("rive ore replay: %s handle %u churned, dropping "
                            "pass draws\n",
                            what,
                            h);
    };
    switch (type)
    {
        case CommandType::beginRenderPass:
        {
            auto c = reader.read<BeginRenderPassCmd>();
            RenderPassDesc desc{};
            desc.colorCount = c.colorCount;
            for (uint32_t i = 0; i < c.colorCount && i < 4; ++i)
            {
                const ColorAttachmentPOD& src = c.colors[i];
                ColorAttachment& dst = desc.colorAttachments[i];
                dst.view = static_cast<TextureView*>(
                    resolve(src.view, OreKind::textureView));
                dst.resolveTarget = static_cast<TextureView*>(
                    resolve(src.resolveTarget, OreKind::textureView));
                dst.loadOp = src.loadOp;
                dst.storeOp = src.storeOp;
                dst.clearColor = {src.clearR,
                                  src.clearG,
                                  src.clearB,
                                  src.clearA};
            }
            const DepthStencilAttachmentPOD& ds = c.depthStencil;
            desc.depthStencil.view = static_cast<TextureView*>(
                resolve(ds.view, OreKind::textureView));
            desc.depthStencil.depthLoadOp = ds.depthLoadOp;
            desc.depthStencil.depthStoreOp = ds.depthStoreOp;
            desc.depthStencil.depthClearValue = ds.depthClearValue;
            desc.depthStencil.stencilLoadOp = ds.stencilLoadOp;
            desc.depthStencil.stencilStoreOp = ds.stencilStoreOp;
            desc.depthStencil.stencilClearValue = ds.stencilClearValue;
            dropDraws = false;
            for (uint32_t i = 0; i < c.colorCount && i < 4; ++i)
            {
                if (desc.colorAttachments[i].view == nullptr &&
                    c.colors[i].view != kInvalidHandle)
                {
                    churned("render pass view", c.colors[i].view);
                    break;
                }
            }
            if (dropDraws)
            {
                break; // pass stays null, its commands no-op
            }
            pass = ctx.beginRenderPass(desc);
            break;
        }
        case CommandType::setPipeline:
        {
            auto c = reader.read<SetPipelineCmd>();
            auto* pipeline =
                static_cast<Pipeline*>(resolve(c.pipeline, OreKind::pipeline));
            if (pipeline == nullptr && c.pipeline != kInvalidHandle)
            {
                churned("pipeline", c.pipeline);
            }
            else if (pass)
            {
                pass->setPipeline(pipeline);
            }
            break;
        }
        case CommandType::setVertexBuffer:
        {
            auto c = reader.read<SetVertexBufferCmd>();
            auto* buffer =
                static_cast<Buffer*>(resolve(c.buffer, OreKind::buffer));
            if (buffer == nullptr && c.buffer != kInvalidHandle)
            {
                churned("vertex buffer", c.buffer);
            }
            else if (pass)
            {
                pass->setVertexBuffer(c.slot, buffer, c.offset);
            }
            break;
        }
        case CommandType::setIndexBuffer:
        {
            auto c = reader.read<SetIndexBufferCmd>();
            auto* buffer =
                static_cast<Buffer*>(resolve(c.buffer, OreKind::buffer));
            if (buffer == nullptr && c.buffer != kInvalidHandle)
            {
                churned("index buffer", c.buffer);
            }
            else if (pass)
            {
                pass->setIndexBuffer(buffer, c.format, c.offset);
            }
            break;
        }
        case CommandType::setBindGroup:
        {
            auto c = reader.read<SetBindGroupCmd>();
            const uint32_t* dynamicOffsets = nullptr;
            if (c.dynamicOffsetCount > 0)
            {
                Span<const uint8_t> blob =
                    reader.blobAt(c.dynamicOffsetStart,
                                  c.dynamicOffsetCount * sizeof(uint32_t));
                dynamicOffsets = reinterpret_cast<const uint32_t*>(blob.data());
            }
            auto* bindGroup = static_cast<BindGroup*>(
                resolve(c.bindGroup, OreKind::bindGroup));
            if (bindGroup == nullptr && c.bindGroup != kInvalidHandle)
            {
                churned("bind group", c.bindGroup);
            }
            else if (pass)
            {
                pass->setBindGroup(c.groupIndex,
                                   bindGroup,
                                   dynamicOffsets,
                                   c.dynamicOffsetCount);
            }
            break;
        }
        case CommandType::setViewport:
        {
            auto c = reader.read<SetViewportCmd>();
            if (pass)
            {
                pass->setViewport(c.x,
                                  c.y,
                                  c.width,
                                  c.height,
                                  c.minDepth,
                                  c.maxDepth);
            }
            break;
        }
        case CommandType::setScissorRect:
        {
            auto c = reader.read<SetScissorRectCmd>();
            if (pass)
            {
                pass->setScissorRect(c.x, c.y, c.width, c.height);
            }
            break;
        }
        case CommandType::setStencilReference:
        {
            auto c = reader.read<SetStencilReferenceCmd>();
            if (pass)
            {
                pass->setStencilReference(c.ref);
            }
            break;
        }
        case CommandType::setBlendColor:
        {
            auto c = reader.read<SetBlendColorCmd>();
            if (pass)
            {
                pass->setBlendColor(c.r, c.g, c.b, c.a);
            }
            break;
        }
        case CommandType::draw:
        {
            auto c = reader.read<DrawCmd>();
            if (pass && !dropDraws)
            {
                pass->draw(c.vertexCount,
                           c.instanceCount,
                           c.firstVertex,
                           c.firstInstance);
            }
            break;
        }
        case CommandType::drawIndexed:
        {
            auto c = reader.read<DrawIndexedCmd>();
            if (pass && !dropDraws)
            {
                pass->drawIndexed(c.indexCount,
                                  c.instanceCount,
                                  c.firstIndex,
                                  c.baseVertex,
                                  c.firstInstance);
            }
            break;
        }
        case CommandType::finish:
        {
            if (pass)
            {
                pass->finish();
                pass.reset();
            }
            break;
        }
        default:
            return false; // lifecycle opcode, handled elsewhere
    }
    return true;
}

// Replays a passes only stream, resolving every handle via resolveHandle.
inline void replayCommandBufferResolved(Context& ctx,
                                        const OreCommandBuffer& cmd,
                                        const HandleResolver& resolveHandle)
{
    // Handles here index the buffer's own typed keep alive table, so the
    // kind is already guaranteed and only the null check applies.
    auto resolve = [&](ResourceHandle h, OreKind) -> rive::gpu::GPUResource* {
        return h == kInvalidHandle ? nullptr : resolveHandle(h);
    };
    OreCommandReader reader(cmd.commandBytes(), cmd.blobBytes());
    std::unique_ptr<RenderPass> pass;
    bool dropDraws = false;
    CommandType type;
    while (reader.next(type))
    {
        if (!replayPassCommand(ctx, pass, dropDraws, type, reader, resolve))
        {
            // The payload was not consumed, so later reads would desync.
            assert(false);
            break;
        }
    }
}

// Replays the single ordered stream. Flagged ids resolve via real; id reuse
// is safe because destroys are consumed in stream order.
inline void replayOreStream(Context& ctx,
                            Span<const uint8_t> commands,
                            Span<const uint8_t> blobs,
                            OreResident& table,
                            const OreHandleResolve& real = nullptr,
                            const OreCanvasResolve& canvasAt = {},
                            const OreImageResolve& imageAt = {})
{
    auto resolve = [&](ResourceHandle h,
                       OreKind kind) -> rive::gpu::GPUResource* {
        return resolveOre(table, real, h, kind);
    };
    OreCommandReader reader(commands, blobs);
    std::unique_ptr<RenderPass> pass;
    bool dropDraws = false;
    CommandType type;
    while (reader.next(type))
    {
        if (!replayOreLifecycle(ctx,
                                table,
                                type,
                                reader,
                                resolve,
                                canvasAt,
                                imageAt) &&
            !replayPassCommand(ctx, pass, dropDraws, type, reader, resolve))
        {
            // The payload was not consumed, so later reads would desync.
            assert(false);
            break;
        }
    }
}

inline void replayOreStream(Context& ctx,
                            const OreCommandBuffer& cmd,
                            OreResident& table,
                            const OreHandleResolve& real = nullptr,
                            const OreCanvasResolve& canvasAt = {},
                            const OreImageResolve& imageAt = {})
{
    replayOreStream(ctx,
                    cmd.commandBytes(),
                    cmd.blobBytes(),
                    table,
                    real,
                    canvasAt,
                    imageAt);
}

// Standalone buffer: handles index the buffer's own keep alive table, then
// run through an optional remap.
inline void replayCommandBuffer(Context& ctx,
                                const OreCommandBuffer& cmd,
                                const ResourceRemap& remap = nullptr)
{
    const std::vector<rcp<rive::gpu::GPUResource>>& keep = cmd.keepAlive();
    replayCommandBufferResolved(
        ctx,
        cmd,
        [&keep, &remap](ResourceHandle h) -> rive::gpu::GPUResource* {
            rive::gpu::GPUResource* r =
                h < keep.size() ? keep[h].get() : nullptr;
            return (remap && r) ? remap(r) : r;
        });
}

} // namespace rive::ore::cmd

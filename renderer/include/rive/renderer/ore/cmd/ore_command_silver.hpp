/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// Portable field wise silver form of a recorded ore command stream.
// serializeSilver emits ints as varuints and floats via writeFloat, so the
// layout is independent of POD padding and arch. silverMatch compares two
// streams, the GPU free regression guard for recording fidelity.
namespace rive::ore::cmd
{

// "ORES" + version. Distinct from the in-memory "ORECMD" stream.
constexpr uint8_t kSilverMagic[4] = {'O', 'R', 'E', 'S'};
constexpr uint64_t kSilverVersion = 1;
constexpr float kSilverEpsilon = 0.0001f;

inline void serializeSilver(const OreCommandBuffer& buffer,
                            std::vector<uint8_t>& out)
{
    VectorBinaryWriter writer(&out);
    writer.write(kSilverMagic, sizeof(kSilverMagic));
    writer.writeVarUint(kSilverVersion);

    OreCommandReader reader(buffer.commandBytes(), buffer.blobBytes());
    CommandType type;
    while (reader.next(type))
    {
        writer.writeVarUint(static_cast<uint32_t>(type));
        switch (type)
        {
            case CommandType::beginRenderPass:
            {
                auto cmd = reader.read<BeginRenderPassCmd>();
                writer.writeVarUint(cmd.colorCount);
                for (uint32_t i = 0; i < cmd.colorCount; ++i)
                {
                    const ColorAttachmentPOD& c = cmd.colors[i];
                    writer.writeVarUint(c.view);
                    writer.writeVarUint(c.resolveTarget);
                    writer.writeVarUint(static_cast<uint32_t>(c.loadOp));
                    writer.writeVarUint(static_cast<uint32_t>(c.storeOp));
                    writer.writeFloat(c.clearR);
                    writer.writeFloat(c.clearG);
                    writer.writeFloat(c.clearB);
                    writer.writeFloat(c.clearA);
                }
                const DepthStencilAttachmentPOD& d = cmd.depthStencil;
                writer.writeVarUint(d.view);
                writer.writeVarUint(static_cast<uint32_t>(d.depthLoadOp));
                writer.writeVarUint(static_cast<uint32_t>(d.depthStoreOp));
                writer.writeFloat(d.depthClearValue);
                writer.writeVarUint(static_cast<uint32_t>(d.stencilLoadOp));
                writer.writeVarUint(static_cast<uint32_t>(d.stencilStoreOp));
                writer.writeVarUint(d.stencilClearValue);
                break;
            }
            case CommandType::setPipeline:
            {
                auto cmd = reader.read<SetPipelineCmd>();
                writer.writeVarUint(cmd.pipeline);
                break;
            }
            case CommandType::setVertexBuffer:
            {
                auto cmd = reader.read<SetVertexBufferCmd>();
                writer.writeVarUint(cmd.slot);
                writer.writeVarUint(cmd.buffer);
                writer.writeVarUint(cmd.offset);
                break;
            }
            case CommandType::setIndexBuffer:
            {
                auto cmd = reader.read<SetIndexBufferCmd>();
                writer.writeVarUint(cmd.buffer);
                writer.writeVarUint(static_cast<uint32_t>(cmd.format));
                writer.writeVarUint(cmd.offset);
                break;
            }
            case CommandType::setBindGroup:
            {
                auto cmd = reader.read<SetBindGroupCmd>();
                writer.writeVarUint(cmd.groupIndex);
                writer.writeVarUint(cmd.bindGroup);
                writer.writeVarUint(cmd.dynamicOffsetCount);
                Span<const uint8_t> blob =
                    reader.blobAt(cmd.dynamicOffsetStart,
                                  cmd.dynamicOffsetCount * sizeof(uint32_t));
                for (uint32_t i = 0; i < cmd.dynamicOffsetCount; ++i)
                {
                    uint32_t off;
                    std::memcpy(&off,
                                blob.data() + i * sizeof(uint32_t),
                                sizeof(uint32_t));
                    writer.writeVarUint(off);
                }
                break;
            }
            case CommandType::setViewport:
            {
                auto cmd = reader.read<SetViewportCmd>();
                writer.writeFloat(cmd.x);
                writer.writeFloat(cmd.y);
                writer.writeFloat(cmd.width);
                writer.writeFloat(cmd.height);
                writer.writeFloat(cmd.minDepth);
                writer.writeFloat(cmd.maxDepth);
                break;
            }
            case CommandType::setScissorRect:
            {
                auto cmd = reader.read<SetScissorRectCmd>();
                writer.writeVarUint(cmd.x);
                writer.writeVarUint(cmd.y);
                writer.writeVarUint(cmd.width);
                writer.writeVarUint(cmd.height);
                break;
            }
            case CommandType::setStencilReference:
            {
                auto cmd = reader.read<SetStencilReferenceCmd>();
                writer.writeVarUint(cmd.ref);
                break;
            }
            case CommandType::setBlendColor:
            {
                auto cmd = reader.read<SetBlendColorCmd>();
                writer.writeFloat(cmd.r);
                writer.writeFloat(cmd.g);
                writer.writeFloat(cmd.b);
                writer.writeFloat(cmd.a);
                break;
            }
            case CommandType::draw:
            {
                auto cmd = reader.read<DrawCmd>();
                writer.writeVarUint(cmd.vertexCount);
                writer.writeVarUint(cmd.instanceCount);
                writer.writeVarUint(cmd.firstVertex);
                writer.writeVarUint(cmd.firstInstance);
                break;
            }
            case CommandType::drawIndexed:
            {
                auto cmd = reader.read<DrawIndexedCmd>();
                writer.writeVarUint(cmd.indexCount);
                writer.writeVarUint(cmd.instanceCount);
                writer.writeVarUint(cmd.firstIndex);
                // baseVertex is signed, round trip its bit pattern.
                writer.writeVarUint(static_cast<uint32_t>(cmd.baseVertex));
                writer.writeVarUint(cmd.firstInstance);
                break;
            }
            case CommandType::finish:
                break;

            // Lifecycle opcodes do not appear in the pass only streams compared
            // today, but the reader must still advance; emit only the identity.
            case CommandType::makeBuffer:
            case CommandType::makeTexture:
            case CommandType::makeSampler:
            case CommandType::makeShaderModule:
            case CommandType::makeBindGroupLayout:
            case CommandType::makeTextureView:
            case CommandType::makePipeline:
            case CommandType::makeBindGroup:
            {
                auto m = reader.read<MakeResourcePOD>();
                reader.skip(orePayloadSizeOf(type) - sizeof(MakeResourcePOD));
                writer.writeVarUint(m.id);
                writer.writeVarUint(m.generation);
                break;
            }
            case CommandType::bufferUpdate:
            {
                auto cmd = reader.read<BufferUpdatePOD>();
                writer.writeVarUint(cmd.handle);
                writer.writeVarUint(cmd.offset);
                writer.writeVarUint(cmd.bytes.size);
                break;
            }
            case CommandType::textureUpload:
            {
                auto cmd = reader.read<TextureUploadPOD>();
                writer.writeVarUint(cmd.handle);
                writer.writeVarUint(cmd.bytes.size);
                break;
            }
            case CommandType::destroyResource:
            {
                auto cmd = reader.read<DestroyResourcePOD>();
                writer.writeVarUint(cmd.handle);
                writer.writeVarUint(cmd.generation);
                break;
            }
            case CommandType::wrapCanvasView:
            {
                auto cmd = reader.read<WrapCanvasViewPOD>();
                writer.writeVarUint(cmd.id);
                writer.writeVarUint(cmd.generation);
                writer.writeVarUint(cmd.canvasId);
                break;
            }
        }
    }
}

namespace silver_detail
{
inline bool varMatch(const char* field,
                     BinaryReader& a,
                     BinaryReader& b,
                     uint64_t* out = nullptr)
{
    uint64_t va = a.readVarUint64();
    uint64_t vb = b.readVarUint64();
    if (va != vb)
    {
        fprintf(stderr,
                "ore silver: %s differs %llu != %llu\n",
                field,
                static_cast<unsigned long long>(va),
                static_cast<unsigned long long>(vb));
        return false;
    }
    if (out != nullptr)
    {
        *out = va;
    }
    return true;
}

inline bool floatMatch(const char* field, BinaryReader& a, BinaryReader& b)
{
    float va = a.readFloat32();
    float vb = b.readFloat32();
    if (std::fabs(va - vb) > kSilverEpsilon)
    {
        fprintf(stderr, "ore silver: %s differs %f != %f\n", field, va, vb);
        return false;
    }
    return true;
}
} // namespace silver_detail

// Compares within the float epsilon and reports the first divergence.
inline bool silverMatch(const std::vector<uint8_t>& expected,
                        const std::vector<uint8_t>& actual)
{
    using namespace silver_detail;
    BinaryReader a(Span<const uint8_t>(expected.data(), expected.size()));
    BinaryReader b(Span<const uint8_t>(actual.data(), actual.size()));

    for (uint32_t i = 0; i < sizeof(kSilverMagic); ++i)
    {
        if (a.readByte() != kSilverMagic[i] || b.readByte() != kSilverMagic[i])
        {
            fprintf(stderr, "ore silver: bad magic\n");
            return false;
        }
    }
    if (!varMatch("version", a, b))
    {
        return false;
    }

    while (!a.reachedEnd())
    {
        if (b.reachedEnd())
        {
            fprintf(stderr, "ore silver: actual stream is shorter\n");
            return false;
        }
        uint64_t op = 0;
        if (!varMatch("opcode", a, b, &op))
        {
            return false;
        }
        switch (static_cast<CommandType>(op))
        {
            case CommandType::beginRenderPass:
            {
                uint64_t colorCount = 0;
                if (!varMatch("colorCount", a, b, &colorCount))
                {
                    return false;
                }
                for (uint64_t i = 0; i < colorCount; ++i)
                {
                    if (!varMatch("color.view", a, b) ||
                        !varMatch("color.resolveTarget", a, b) ||
                        !varMatch("color.loadOp", a, b) ||
                        !varMatch("color.storeOp", a, b) ||
                        !floatMatch("color.clearR", a, b) ||
                        !floatMatch("color.clearG", a, b) ||
                        !floatMatch("color.clearB", a, b) ||
                        !floatMatch("color.clearA", a, b))
                    {
                        return false;
                    }
                }
                if (!varMatch("ds.view", a, b) ||
                    !varMatch("ds.depthLoadOp", a, b) ||
                    !varMatch("ds.depthStoreOp", a, b) ||
                    !floatMatch("ds.depthClearValue", a, b) ||
                    !varMatch("ds.stencilLoadOp", a, b) ||
                    !varMatch("ds.stencilStoreOp", a, b) ||
                    !varMatch("ds.stencilClearValue", a, b))
                {
                    return false;
                }
                break;
            }
            case CommandType::setPipeline:
                if (!varMatch("pipeline", a, b))
                {
                    return false;
                }
                break;
            case CommandType::setVertexBuffer:
                if (!varMatch("vb.slot", a, b) ||
                    !varMatch("vb.buffer", a, b) ||
                    !varMatch("vb.offset", a, b))
                {
                    return false;
                }
                break;
            case CommandType::setIndexBuffer:
                if (!varMatch("ib.buffer", a, b) ||
                    !varMatch("ib.format", a, b) ||
                    !varMatch("ib.offset", a, b))
                {
                    return false;
                }
                break;
            case CommandType::setBindGroup:
            {
                uint64_t count = 0;
                if (!varMatch("bg.groupIndex", a, b) ||
                    !varMatch("bg.bindGroup", a, b) ||
                    !varMatch("bg.dynamicOffsetCount", a, b, &count))
                {
                    return false;
                }
                for (uint64_t i = 0; i < count; ++i)
                {
                    if (!varMatch("bg.dynamicOffset", a, b))
                    {
                        return false;
                    }
                }
                break;
            }
            case CommandType::setViewport:
                if (!floatMatch("vp.x", a, b) || !floatMatch("vp.y", a, b) ||
                    !floatMatch("vp.width", a, b) ||
                    !floatMatch("vp.height", a, b) ||
                    !floatMatch("vp.minDepth", a, b) ||
                    !floatMatch("vp.maxDepth", a, b))
                {
                    return false;
                }
                break;
            case CommandType::setScissorRect:
                if (!varMatch("sc.x", a, b) || !varMatch("sc.y", a, b) ||
                    !varMatch("sc.width", a, b) || !varMatch("sc.height", a, b))
                {
                    return false;
                }
                break;
            case CommandType::setStencilReference:
                if (!varMatch("stencilRef", a, b))
                {
                    return false;
                }
                break;
            case CommandType::setBlendColor:
                if (!floatMatch("blend.r", a, b) ||
                    !floatMatch("blend.g", a, b) ||
                    !floatMatch("blend.b", a, b) ||
                    !floatMatch("blend.a", a, b))
                {
                    return false;
                }
                break;
            case CommandType::draw:
                if (!varMatch("draw.vertexCount", a, b) ||
                    !varMatch("draw.instanceCount", a, b) ||
                    !varMatch("draw.firstVertex", a, b) ||
                    !varMatch("draw.firstInstance", a, b))
                {
                    return false;
                }
                break;
            case CommandType::drawIndexed:
                if (!varMatch("drawIndexed.indexCount", a, b) ||
                    !varMatch("drawIndexed.instanceCount", a, b) ||
                    !varMatch("drawIndexed.firstIndex", a, b) ||
                    !varMatch("drawIndexed.baseVertex", a, b) ||
                    !varMatch("drawIndexed.firstInstance", a, b))
                {
                    return false;
                }
                break;
            case CommandType::finish:
                break;
            default:
                fprintf(stderr,
                        "ore silver: unknown opcode %llu\n",
                        static_cast<unsigned long long>(op));
                return false;
        }
    }
    if (!b.reachedEnd())
    {
        fprintf(stderr, "ore silver: actual stream is longer\n");
        return false;
    }
    return true;
}

} // namespace rive::ore::cmd

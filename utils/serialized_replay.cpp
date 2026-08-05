/*
 * Copyright 2026 Rive
 */

#include "utils/serialized_replay.hpp"
#include "utils/serialize_ops.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/math/mat2d.hpp"
#include <unordered_map>
#include <vector>

using namespace rive;

namespace
{
// Absent ids mean a truncated or corrupt stream; callers fail instead of
// dereferencing a null resource.
template <typename T>
T* find(std::unordered_map<uint64_t, rcp<T>>& map, uint64_t id)
{
    auto it = map.find(id);
    return it != map.end() ? it->second.get() : nullptr;
}
} // namespace

bool rive::replaySerializedCommands(Span<const uint8_t> stream,
                                    Factory* factory,
                                    Renderer* renderer,
                                    const SerializedReplayHooks& hooks)
{
    BinaryReader reader(stream);
    if (reader.readByte() != 'S' || reader.readByte() != 'R' ||
        reader.readByte() != 'I' || reader.readByte() != 'V')
    {
        return false;
    }
    if (reader.readVarUint64() != 1)
    {
        return false;
    }

    std::unordered_map<uint64_t, rcp<RenderPath>> paths;
    std::unordered_map<uint64_t, rcp<RenderPaint>> paints;
    std::unordered_map<uint64_t, rcp<RenderShader>> shaders;
    std::unordered_map<uint64_t, rcp<RenderImage>> images;
    std::unordered_map<uint64_t, rcp<RenderBuffer>> buffers;

    while (!reader.reachedEnd())
    {
        SerializeOp op = static_cast<SerializeOp>(reader.readVarUint64());
        if (reader.hasError())
            return false;
        switch (op)
        {
            case SerializeOp::makeRenderPath:
            {
                uint64_t id = reader.readVarUint64();
                // Geometry and fill rule arrive as later ops.
                paths[id] = factory->makeEmptyRenderPath();
                break;
            }
            case SerializeOp::makeRenderPaint:
            {
                uint64_t id = reader.readVarUint64();
                paints[id] = factory->makeRenderPaint();
                break;
            }
            case SerializeOp::rewind:
            {
                uint64_t id = reader.readVarUint64();
                RenderPath* path = find(paths, id);
                if (path == nullptr)
                    return false;
                path->rewind();
                break;
            }
            case SerializeOp::fillRule:
            {
                uint64_t id = reader.readVarUint64();
                RenderPath* path = find(paths, id);
                if (path == nullptr)
                    return false;
                path->fillRule(static_cast<FillRule>(reader.readVarUint64()));
                break;
            }
            case SerializeOp::addRawPath:
            {
                uint64_t id = reader.readVarUint64();
                RenderPath* path = find(paths, id);
                if (path == nullptr)
                    return false;
                RawPath rp = deserializeRawPath(reader);
                path->addRawPath(rp);
                break;
            }
            case SerializeOp::color:
            {
                uint64_t id = reader.readVarUint64();
                RenderPaint* paint = find(paints, id);
                if (paint == nullptr)
                    return false;
                paint->color(static_cast<unsigned int>(reader.readVarUint64()));
                break;
            }
            case SerializeOp::style:
            {
                uint64_t id = reader.readVarUint64();
                RenderPaint* paint = find(paints, id);
                if (paint == nullptr)
                    return false;
                // The stream writes 0 for stroke and 1 for fill.
                bool stroked = reader.readVarUint64() == 0;
                paint->style(stroked ? RenderPaintStyle::stroke
                                     : RenderPaintStyle::fill);
                break;
            }
            case SerializeOp::thickness:
            {
                uint64_t id = reader.readVarUint64();
                RenderPaint* paint = find(paints, id);
                if (paint == nullptr)
                    return false;
                paint->thickness(reader.readFloat32());
                break;
            }
            case SerializeOp::join:
            {
                uint64_t id = reader.readVarUint64();
                RenderPaint* paint = find(paints, id);
                if (paint == nullptr)
                    return false;
                paint->join(static_cast<StrokeJoin>(reader.readVarUint64()));
                break;
            }
            case SerializeOp::cap:
            {
                uint64_t id = reader.readVarUint64();
                RenderPaint* paint = find(paints, id);
                if (paint == nullptr)
                    return false;
                paint->cap(static_cast<StrokeCap>(reader.readVarUint64()));
                break;
            }
            case SerializeOp::feather:
            {
                uint64_t id = reader.readVarUint64();
                RenderPaint* paint = find(paints, id);
                if (paint == nullptr)
                    return false;
                paint->feather(reader.readFloat32());
                break;
            }
            case SerializeOp::blendMode:
            {
                uint64_t id = reader.readVarUint64();
                RenderPaint* paint = find(paints, id);
                if (paint == nullptr)
                    return false;
                paint->blendMode(
                    static_cast<BlendMode>(reader.readVarUint64()));
                break;
            }
            case SerializeOp::shader:
            {
                uint64_t id = reader.readVarUint64();
                uint64_t shaderId = reader.readVarUint64();
                RenderPaint* paint = find(paints, id);
                if (paint == nullptr)
                    return false;
                // The stream writes 0 for both nullptr and shader id 0. The map
                // resolves it since a missing entry yields a null rcp.
                paint->shader(shaders[shaderId]);
                break;
            }
            case SerializeOp::makeLinearGradient:
            case SerializeOp::makeRadialGradient:
            {
                uint64_t id = reader.readVarUint64();
                size_t count = static_cast<size_t>(reader.readVarUint64());
                std::vector<ColorInt> colors(count);
                std::vector<float> stops(count);
                for (size_t i = 0; i < count; ++i)
                {
                    colors[i] = static_cast<ColorInt>(reader.readVarUint64());
                    stops[i] = reader.readFloat32();
                }
                float a = reader.readFloat32();
                float b = reader.readFloat32();
                float c = reader.readFloat32();
                if (op == SerializeOp::makeLinearGradient)
                {
                    float d = reader.readFloat32();
                    shaders[id] = factory->makeLinearGradient(a,
                                                              b,
                                                              c,
                                                              d,
                                                              colors.data(),
                                                              stops.data(),
                                                              count);
                }
                else
                {
                    shaders[id] = factory->makeRadialGradient(a,
                                                              b,
                                                              c,
                                                              colors.data(),
                                                              stops.data(),
                                                              count);
                }
                break;
            }
            case SerializeOp::decodeImage:
            {
                uint64_t id = reader.readVarUint64();
                size_t size = static_cast<size_t>(reader.readVarUint64());
                Span<const uint8_t> data = reader.readBytes(size);
                images[id] = factory->decodeImage(data);
                break;
            }
            case SerializeOp::makeRenderBuffer:
            {
                uint64_t id = reader.readVarUint64();
                size_t size = static_cast<size_t>(reader.readVarUint64());
                auto type =
                    static_cast<RenderBufferType>(reader.readVarUint64());
                auto flags =
                    static_cast<RenderBufferFlags>(reader.readVarUint64());
                buffers[id] = factory->makeRenderBuffer(type, flags, size);
                break;
            }
            case SerializeOp::setVertexBufferData:
            case SerializeOp::setIndexBufferData:
            {
                uint64_t id = reader.readVarUint64();
                RenderBuffer* buf = find(buffers, id);
                if (buf == nullptr)
                    return false;
                void* mapped = buf->map();
                if (op == SerializeOp::setVertexBufferData)
                {
                    size_t n = buf->sizeInBytes() / sizeof(float);
                    float* out = static_cast<float*>(mapped);
                    for (size_t i = 0; i < n; ++i)
                        out[i] = reader.readFloat32();
                }
                else
                {
                    size_t n = buf->sizeInBytes() / sizeof(uint16_t);
                    uint16_t* out = static_cast<uint16_t*>(mapped);
                    for (size_t i = 0; i < n; ++i)
                        out[i] = static_cast<uint16_t>(reader.readVarUint64());
                }
                buf->unmap();
                break;
            }
            case SerializeOp::save:
                renderer->save();
                break;
            case SerializeOp::restore:
                renderer->restore();
                break;
            case SerializeOp::transform:
            {
                float m[6];
                for (int i = 0; i < 6; ++i)
                    m[i] = reader.readFloat32();
                renderer->transform(Mat2D(m[0], m[1], m[2], m[3], m[4], m[5]));
                break;
            }
            case SerializeOp::modulateOpacity:
                renderer->modulateOpacity(reader.readFloat32());
                break;
            case SerializeOp::drawPath:
            {
                uint64_t pathId = reader.readVarUint64();
                uint64_t paintId = reader.readVarUint64();
                RenderPath* path = find(paths, pathId);
                RenderPaint* paint = find(paints, paintId);
                if (path == nullptr || paint == nullptr)
                    return false;
                renderer->drawPath(path, paint);
                break;
            }
            case SerializeOp::clipPath:
            {
                uint64_t pathId = reader.readVarUint64();
                RenderPath* path = find(paths, pathId);
                if (path == nullptr)
                    return false;
                renderer->clipPath(path);
                break;
            }
            case SerializeOp::drawImage:
            {
                uint64_t imageId = reader.readVarUint64();
                auto blend = static_cast<BlendMode>(reader.readVarUint64());
                float opacity = reader.readFloat32();
                renderer->drawImage(images[imageId].get(),
                                    ImageSampler::LinearClamp(),
                                    blend,
                                    opacity);
                break;
            }
            case SerializeOp::drawImageMesh:
            {
                uint64_t imageId = reader.readVarUint64();
                auto blend = static_cast<BlendMode>(reader.readVarUint64());
                float opacity = reader.readFloat32();
                rcp<RenderBuffer> pos = buffers[reader.readVarUint64()];
                rcp<RenderBuffer> uvs = buffers[reader.readVarUint64()];
                rcp<RenderBuffer> idx = buffers[reader.readVarUint64()];
                uint32_t vertexCount =
                    pos ? static_cast<uint32_t>(pos->sizeInBytes() /
                                                (2 * sizeof(float)))
                        : 0;
                uint32_t indexCount =
                    idx ? static_cast<uint32_t>(idx->sizeInBytes() /
                                                sizeof(uint16_t))
                        : 0;
                renderer->drawImageMesh(images[imageId].get(),
                                        ImageSampler::LinearClamp(),
                                        pos,
                                        uvs,
                                        idx,
                                        vertexCount,
                                        indexCount,
                                        blend,
                                        opacity);
                break;
            }
            case SerializeOp::frame:
                if (hooks.onFrame)
                    hooks.onFrame();
                break;
            case SerializeOp::frameSize:
            {
                uint32_t w = static_cast<uint32_t>(reader.readVarUint64());
                uint32_t h = static_cast<uint32_t>(reader.readVarUint64());
                if (hooks.onFrameSize)
                    hooks.onFrameSize(w, h);
                break;
            }
            default:
                return false; // unknown opcode
        }
        if (reader.hasError())
            return false;
    }
    return true;
}

/*
 * Copyright 2026 Rive
 */

#include "utils/serialized_replay.hpp"
#include "utils/serialize_ops.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/math/mat2d.hpp"
#include "utils/no_op_renderer.hpp"
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

    // Cache-as-bitmap. A canvas is declared by makeRenderCanvas, its content
    // arrives inline between canvasContentBegin/End, and the composite that
    // samples it is an ordinary drawImage of the same id -- so a canvas image
    // lands in `images` alongside the decoded ones.
    struct CanvasSize
    {
        uint32_t width;
        uint32_t height;
    };
    std::unordered_map<uint64_t, CanvasSize> canvasSizes;
    // Where the ops currently being read draw. The screen renderer at the
    // bottom, a host-supplied one for each open canvas, and the null sink for
    // a canvas the host declined -- its content still has to parse (later ops
    // reference resources declared inside it) but must not reach the screen.
    NoOpRenderer droppedContent;
    Renderer* active = renderer;
    // One entry per open canvas bracket. The id rides along with the renderer
    // it interrupted so an end can be checked against the begin that opened
    // it; without it the stack depth is the only thing validated, and a
    // mismatched end would close the wrong canvas and still report success.
    struct OpenCanvas
    {
        uint64_t id;
        Renderer* interruptedRenderer;
    };
    std::vector<OpenCanvas> interrupted;

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
            case SerializeOp::additiveness:
            {
                uint64_t id = reader.readVarUint64();
                RenderPaint* paint = find(paints, id);
                if (paint == nullptr)
                    return false;
                paint->additiveness(reader.readFloat32());
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
            case SerializeOp::paintModulatedImage:
            {
                uint64_t id = reader.readVarUint64();
                uint64_t rawImageId = reader.readVarUint64();
                auto filter = static_cast<ImageFilter>(reader.readVarUint64());
                auto wrapX = static_cast<ImageWrap>(reader.readVarUint64());
                auto wrapY = static_cast<ImageWrap>(reader.readVarUint64());
                float xx = reader.readFloat32();
                float xy = reader.readFloat32();
                float yx = reader.readFloat32();
                float yy = reader.readFloat32();
                float tx = reader.readFloat32();
                float ty = reader.readFloat32();
                RenderPaint* paint = find(paints, id);
                if (paint == nullptr)
                    return false;
                // Image id is offset by one; 0 means no image.
                RenderImage* image =
                    rawImageId == 0 ? nullptr : images[rawImageId - 1].get();
                ImageSampler sampler;
                sampler.filter = filter;
                sampler.wrapX = wrapX;
                sampler.wrapY = wrapY;
                paint->modulatedImage(image,
                                      sampler,
                                      Mat2D(xx, xy, yx, yy, tx, ty));
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
                active->save();
                break;
            case SerializeOp::restore:
                active->restore();
                break;
            case SerializeOp::transform:
            {
                float m[6];
                for (int i = 0; i < 6; ++i)
                    m[i] = reader.readFloat32();
                active->transform(Mat2D(m[0], m[1], m[2], m[3], m[4], m[5]));
                break;
            }
            case SerializeOp::modulateOpacity:
                active->modulateOpacity(reader.readFloat32());
                break;
            case SerializeOp::modulateColor:
            {
                ColorInt color = (ColorInt)reader.readVarUint64();
                active->modulateColor(color, reader.readVarUint64() != 0);
                break;
            }
            case SerializeOp::drawPath:
            {
                uint64_t pathId = reader.readVarUint64();
                uint64_t paintId = reader.readVarUint64();
                RenderPath* path = find(paths, pathId);
                RenderPaint* paint = find(paints, paintId);
                if (path == nullptr || paint == nullptr)
                    return false;
                active->drawPath(path, paint);
                break;
            }
            case SerializeOp::clipPath:
            {
                uint64_t pathId = reader.readVarUint64();
                RenderPath* path = find(paths, pathId);
                if (path == nullptr)
                    return false;
                active->clipPath(path);
                break;
            }
            case SerializeOp::drawImage:
            case SerializeOp::drawImageAdditive:
            {
                uint64_t imageId = reader.readVarUint64();
                auto blend = static_cast<BlendMode>(reader.readVarUint64());
                float opacity = reader.readFloat32();
                // Only the additive variant carries the trailing float; the
                // plain op means an additiveness of 0.
                float additiveness = op == SerializeOp::drawImageAdditive
                                         ? reader.readFloat32()
                                         : 0.0f;
                // Null when a decode failed or when the host declined the
                // canvas this id names; either way there is nothing to
                // composite and the rest of the frame still replays.
                if (RenderImage* image = find(images, imageId))
                {
                    active->drawImage(image,
                                      ImageSampler::LinearClamp(),
                                      blend,
                                      opacity,
                                      additiveness);
                }
                break;
            }
            case SerializeOp::drawImageMesh:
            case SerializeOp::drawImageMeshAdditive:
            {
                uint64_t imageId = reader.readVarUint64();
                auto blend = static_cast<BlendMode>(reader.readVarUint64());
                float opacity = reader.readFloat32();
                rcp<RenderBuffer> pos = buffers[reader.readVarUint64()];
                rcp<RenderBuffer> uvs = buffers[reader.readVarUint64()];
                rcp<RenderBuffer> idx = buffers[reader.readVarUint64()];
                float additiveness = op == SerializeOp::drawImageMeshAdditive
                                         ? reader.readFloat32()
                                         : 0.0f;
                uint32_t vertexCount =
                    pos ? static_cast<uint32_t>(pos->sizeInBytes() /
                                                (2 * sizeof(float)))
                        : 0;
                uint32_t indexCount =
                    idx ? static_cast<uint32_t>(idx->sizeInBytes() /
                                                sizeof(uint16_t))
                        : 0;
                // Same rule as drawImage: the id can name a decode that
                // failed or a canvas the host declined, and there is nothing
                // to draw either way. Looked up rather than indexed so a
                // missing id does not insert a null entry that a later
                // composite would then find.
                if (RenderImage* image = find(images, imageId))
                {
                    active->drawImageMesh(image,
                                          ImageSampler::LinearClamp(),
                                          pos,
                                          uvs,
                                          idx,
                                          vertexCount,
                                          indexCount,
                                          blend,
                                          opacity,
                                          additiveness);
                }
                break;
            }
            case SerializeOp::makeRenderCanvas:
            {
                uint64_t id = reader.readVarUint64();
                uint32_t w = static_cast<uint32_t>(reader.readVarUint64());
                uint32_t h = static_cast<uint32_t>(reader.readVarUint64());
                // No allocation here: the host mints the target when the
                // content actually opens, on whatever device it replays
                // against.
                canvasSizes[id] = {w, h};
                break;
            }
            case SerializeOp::canvasContentBegin:
            {
                uint64_t id = reader.readVarUint64();
                uint32_t clearColor =
                    static_cast<uint32_t>(reader.readVarUint64());
                auto it = canvasSizes.find(id);
                if (it == canvasSizes.end())
                {
                    return false; // content for a canvas never declared
                }
                interrupted.push_back({id, active});
                rcp<RenderImage> image;
                Renderer* content =
                    hooks.onCanvasContentBegin
                        ? hooks.onCanvasContentBegin(id,
                                                     it->second.width,
                                                     it->second.height,
                                                     clearColor,
                                                     &image)
                        : nullptr;
                // A host with no offscreen target returns null. Its content is
                // parsed and dropped, and the composite that names this id
                // finds no image and draws nothing.
                if (content != nullptr)
                {
                    images[id] = std::move(image);
                }
                else
                {
                    // The raster was declined, so whatever the host may have
                    // allocated holds no content for this bracket. Drop any
                    // image it set, and any image a previous bracket left under
                    // this id, so the composite draws nothing rather than stale
                    // or uninitialized pixels.
                    images.erase(id);
                }
                active = content != nullptr ? content : &droppedContent;
                break;
            }
            case SerializeOp::canvasContentEnd:
            {
                uint64_t id = reader.readVarUint64();
                if (interrupted.empty())
                {
                    return false; // end without a matching begin
                }
                if (interrupted.back().id != id)
                {
                    // Ending a canvas the innermost bracket never opened.
                    // Honoring it would tell the host to close an unrelated
                    // canvas and hand the wrong renderer back to the ops that
                    // follow, so treat it as a malformed stream.
                    return false;
                }
                if (hooks.onCanvasContentEnd)
                {
                    hooks.onCanvasContentEnd(id);
                }
                active = interrupted.back().interruptedRenderer;
                interrupted.pop_back();
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
    // A canvas left open means the stream was cut mid-bracket.
    return interrupted.empty();
}

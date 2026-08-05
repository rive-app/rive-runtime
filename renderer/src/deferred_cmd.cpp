/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/cmd/deferred_render_factory.hpp"
#include "rive/renderer/cmd/render_replay.hpp"

// Out of line home for the large deferred stream bodies, so every TU that
// touches the deferred headers does not recompile them.
namespace rive::cmd
{

bool sniffImageSize(Span<const uint8_t> b, int& w, int& h)
{
    const uint8_t* d = b.data();
    size_t n = b.size();
    auto be32 = [&](size_t i) {
        return (d[i] << 24) | (d[i + 1] << 16) | (d[i + 2] << 8) | d[i + 3];
    };
    // PNG: 8-byte sig, then IHDR with width/height as big-endian u32 at 16/20.
    if (n >= 24 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G')
    {
        w = static_cast<int>(be32(16));
        h = static_cast<int>(be32(20));
        return true;
    }
    // GIF: "GIF87a"/"GIF89a", then width/height little-endian u16 at 6/8.
    if (n >= 10 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F')
    {
        w = d[6] | (d[7] << 8);
        h = d[8] | (d[9] << 8);
        return true;
    }
    // WEBP: RIFF....WEBP; VP8 / VP8L / VP8X carry dims at known offsets.
    if (n >= 30 && d[0] == 'R' && d[1] == 'I' && d[2] == 'F' && d[3] == 'F' &&
        d[8] == 'W' && d[9] == 'E' && d[10] == 'B' && d[11] == 'P')
    {
        if (d[12] == 'V' && d[13] == 'P' && d[14] == '8' && d[15] == ' ')
        {
            w = (d[26] | (d[27] << 8)) & 0x3fff;
            h = (d[28] | (d[29] << 8)) & 0x3fff;
            return true;
        }
        if (d[12] == 'V' && d[13] == 'P' && d[14] == '8' && d[15] == 'L')
        {
            uint32_t bits =
                d[21] | (d[22] << 8) | (d[23] << 16) | (d[24] << 24);
            w = static_cast<int>((bits & 0x3fff) + 1);
            h = static_cast<int>(((bits >> 14) & 0x3fff) + 1);
            return true;
        }
        if (d[12] == 'V' && d[13] == 'P' && d[14] == '8' && d[15] == 'X')
        {
            w = (d[24] | (d[25] << 8) | (d[26] << 16)) + 1;
            h = (d[27] | (d[28] << 8) | (d[29] << 16)) + 1;
            return true;
        }
    }
    // JPEG: walk markers to an SOF (0xC0..0xCF, excluding non-SOF) for dims.
    if (n >= 4 && d[0] == 0xff && d[1] == 0xd8)
    {
        size_t i = 2;
        while (i + 9 < n)
        {
            if (d[i] != 0xff)
            {
                i++;
                continue;
            }
            uint8_t marker = d[i + 1];
            // SOF0..SOF15 carry dims; skip DHT(C4)/DAA(C8)/DAC(CC) which don't.
            if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 &&
                marker != 0xc8 && marker != 0xcc)
            {
                h = (d[i + 5] << 8) | d[i + 6];
                w = (d[i + 7] << 8) | d[i + 8];
                return true;
            }
            uint32_t seg = (d[i + 2] << 8) | d[i + 3];
            i += 2 + seg;
        }
    }
    return false;
}

void replayRenderCommands(Factory* factory,
                          Renderer* renderer,
                          Span<const uint8_t> commands,
                          Span<const uint8_t> blobs,
                          ResourceTable& table,
                          const ReplayHooks& hooks)
{
    auto& paths = table.paths;
    auto& paints = table.paints;
    auto& shaders = table.shaders;
    auto& images = table.images;
    auto& buffers = table.buffers;

    auto filterAllows = [](ReplayFilter f, RenderCmd c) {
        if (f == ReplayFilter::all)
        {
            return true;
        }
        switch (c)
        {
            case RenderCmd::save:
            case RenderCmd::restore:
            case RenderCmd::transform:
            case RenderCmd::drawPath:
            case RenderCmd::clipPath:
            case RenderCmd::drawImage:
            case RenderCmd::drawImageMesh:
            case RenderCmd::modulateOpacity:
            case RenderCmd::canvasContentBegin:
            case RenderCmd::canvasContentEnd:
                return f == ReplayFilter::draws;
            case RenderCmd::destroyResource:
                return f == ReplayFilter::destroys;
            default:
                return f == ReplayFilter::resources;
        }
    };

    RenderCommandReader reader(commands, blobs);
    auto path = [&](RenderHandle h) -> RenderPath* { return paths.get(h); };
    auto paint = [&](RenderHandle h) -> RenderPaint* { return paints.get(h); };
    auto image = [&](RenderHandle h) -> RenderImage* { return images.get(h); };
    auto sampler = [](uint8_t wx, uint8_t wy, uint8_t f) {
        return ImageSampler{static_cast<ImageWrap>(wx),
                            static_cast<ImageWrap>(wy),
                            static_cast<ImageFilter>(f)};
    };

    // Draws route into cur: the screen by default, or the active canvas
    // between content brackets. Null drops the draw.
    Renderer* cur = renderer;

    uint8_t type;
    uint8_t prevType = 255;
    size_t prevPos = 0;
    while (reader.next(type))
    {
        if (type > static_cast<uint8_t>(RenderCmd::lastRenderCmd))
        {
            // Unknown opcode: its payload was not consumed, so every later
            // read would desync. Stop.
            fprintf(stderr,
                    "rive replay ABORT: opcode %u at byte %zu of %zu, last "
                    "good opcode %u at byte %zu\n",
                    type,
                    reader.position() - 1,
                    commands.size(),
                    prevType,
                    prevPos);
            assert(false);
            break;
        }
        prevType = type;
        prevPos = reader.position() - 1;
        const auto cmd = static_cast<RenderCmd>(type);
        if (!filterAllows(hooks.filter, cmd))
        {
            reader.skip(payloadSizeOf(cmd));
            continue;
        }
        switch (cmd)
        {
            case RenderCmd::makePath:
            {
                auto c = reader.read<MakePathPOD>();
                RawPath raw = rebuildRawPath(
                    reader.blobAt(c.blobOffset,
                                  c.verbCount *
                                      static_cast<uint32_t>(sizeof(PathVerb))),
                    reader.blobAt(c.pointsOffset,
                                  c.pointCount *
                                      static_cast<uint32_t>(sizeof(Vec2D))));
                paths.set(
                    c.id,
                    factory->makeRenderPath(raw,
                                            static_cast<FillRule>(c.fillRule)),
                    c.generation);
                if (c.id >= table.pathFillRules.size())
                {
                    table.pathFillRules.resize(c.id + 1);
                }
                table.pathFillRules[c.id] = c.fillRule;
                break;
            }
            case RenderCmd::makeEmptyPath:
            {
                auto c = reader.read<MakeIdPOD>();
                paths.set(c.id, factory->makeEmptyRenderPath(), c.generation);
                if (c.id >= table.pathFillRules.size())
                {
                    table.pathFillRules.resize(c.id + 1);
                }
                table.pathFillRules[c.id] = 0;
                break;
            }
            case RenderCmd::makePaint:
            {
                auto c = reader.read<MakeIdPOD>();
                paints.set(c.id, factory->makeRenderPaint(), c.generation);
                if (c.id >= table.paintShadows.size())
                {
                    table.paintShadows.resize(c.id + 1);
                }
                table.paintShadows[c.id] = PaintShadow{};
                break;
            }
            case RenderCmd::makeLinearGradient:
            {
                auto c = reader.read<LinearGradientPOD>();
                const ColorInt* colors = reinterpret_cast<const ColorInt*>(
                    reader
                        .blobAt(c.blobOffset,
                                c.count *
                                    static_cast<uint32_t>(sizeof(ColorInt)))
                        .data());
                const float* stops = reinterpret_cast<const float*>(
                    reader
                        .blobAt(c.stopsOffset,
                                c.count * static_cast<uint32_t>(sizeof(float)))
                        .data());
                if (colors == nullptr || stops == nullptr)
                {
                    break; // blob out of range (corrupt stream)
                }
                shaders.set(c.id,
                            factory->makeLinearGradient(c.sx,
                                                        c.sy,
                                                        c.ex,
                                                        c.ey,
                                                        colors,
                                                        stops,
                                                        c.count),
                            c.generation);
                break;
            }
            case RenderCmd::makeRadialGradient:
            {
                auto c = reader.read<RadialGradientPOD>();
                const ColorInt* colors = reinterpret_cast<const ColorInt*>(
                    reader
                        .blobAt(c.blobOffset,
                                c.count *
                                    static_cast<uint32_t>(sizeof(ColorInt)))
                        .data());
                const float* stops = reinterpret_cast<const float*>(
                    reader
                        .blobAt(c.stopsOffset,
                                c.count * static_cast<uint32_t>(sizeof(float)))
                        .data());
                if (colors == nullptr || stops == nullptr)
                {
                    break; // blob out of range (corrupt stream)
                }
                shaders.set(c.id,
                            factory->makeRadialGradient(c.cx,
                                                        c.cy,
                                                        c.radius,
                                                        colors,
                                                        stops,
                                                        c.count),
                            c.generation);
                break;
            }
            case RenderCmd::decodeImage:
            {
                auto c = reader.read<DecodeImagePOD>();
                images.set(c.id,
                           factory->decodeImage(
                               reader.blobAt(c.blobOffset, c.byteCount)),
                           c.generation);
                break;
            }
            case RenderCmd::makeBuffer:
            {
                auto c = reader.read<MakeBufferPOD>();
                buffers.set(c.id,
                            factory->makeRenderBuffer(
                                static_cast<RenderBufferType>(c.bufferType),
                                static_cast<RenderBufferFlags>(c.flags),
                                c.sizeInBytes),
                            c.generation);
                if (c.id >= table.bufferShadows.size())
                {
                    table.bufferShadows.resize(c.id + 1);
                }
                table.bufferShadows[c.id] = {static_cast<uint8_t>(c.bufferType),
                                             static_cast<uint16_t>(c.flags),
                                             c.sizeInBytes};
                break;
            }
            case RenderCmd::bufferData:
            {
                auto c = reader.read<BufferDataPOD>();
                Span<const uint8_t> src = reader.blobAt(c.blobOffset, c.size);
                if (auto* b = buffers.get(c.buffer))
                {
                    if (src.size() == c.size)
                    {
                        void* dst = b->map();
                        if (dst)
                        {
                            std::memcpy(dst, src.data(), c.size);
                        }
                        b->unmap();
                    }
                }
                break;
            }
            case RenderCmd::destroyResource:
            {
                auto c = reader.read<DestroyResourcePOD>();
                table.destroy(static_cast<ResourceKind>(c.kind),
                              c.id,
                              c.generation);
                break;
            }
            case RenderCmd::resourceNewVersion:
            {
                // A drawn resource was mutated again: alias the outgoing
                // version for the draws that pinned it and continue on a
                // fresh object carrying the shadowed state.
                auto c = reader.read<ResourceVersionPOD>();
                switch (static_cast<ResourceKind>(c.kind))
                {
                    case ResourceKind::paint:
                    {
                        auto fresh = factory->makeRenderPaint();
                        if (fresh != nullptr &&
                            c.id < table.paintShadows.size())
                        {
                            const PaintShadow& sh = table.paintShadows[c.id];
                            fresh->style(
                                static_cast<RenderPaintStyle>(sh.style));
                            fresh->color(sh.color);
                            fresh->thickness(sh.thickness);
                            fresh->join(static_cast<StrokeJoin>(sh.join));
                            fresh->cap(static_cast<StrokeCap>(sh.cap));
                            fresh->feather(sh.feather);
                            fresh->blendMode(
                                static_cast<BlendMode>(sh.blendMode));
                            if (sh.shader != kInvalidRenderHandle)
                            {
                                fresh->shader(shaders.shared(sh.shader));
                            }
                        }
                        paints.newVersion(c.id, c.version, std::move(fresh));
                        break;
                    }
                    case ResourceKind::path:
                    {
                        // Seed from the outgoing version so a non rewind
                        // mutation appends onto prior geometry; a rewind bump
                        // clears the seed via its own recorded command.
                        auto fresh = factory->makeEmptyRenderPath();
                        if (fresh != nullptr)
                        {
                            if (auto* outgoing = paths.get(c.id))
                            {
                                fresh->addRenderPath(outgoing, Mat2D());
                            }
                            if (c.id < table.pathFillRules.size())
                            {
                                fresh->fillRule(static_cast<FillRule>(
                                    table.pathFillRules[c.id]));
                            }
                        }
                        paths.newVersion(c.id, c.version, std::move(fresh));
                        break;
                    }
                    case ResourceKind::buffer:
                    {
                        rcp<RenderBuffer> fresh;
                        if (c.id < table.bufferShadows.size())
                        {
                            const BufferShadow& sh = table.bufferShadows[c.id];
                            fresh = factory->makeRenderBuffer(
                                static_cast<RenderBufferType>(sh.type),
                                static_cast<RenderBufferFlags>(sh.flags),
                                sh.size);
                        }
                        buffers.newVersion(c.id, c.version, std::move(fresh));
                        break;
                    }
                    default:
                        break; // shaders and images never mutate
                }
                break;
            }

            case RenderCmd::pathRewind:
            {
                auto c = reader.read<ResIdPOD>();
                if (auto* p = path(c.id))
                {
                    p->rewind();
                }
                break;
            }
            case RenderCmd::pathFillRule:
            {
                auto c = reader.read<PathFillRulePOD>();
                if (auto* p = path(c.path))
                {
                    p->fillRule(static_cast<FillRule>(c.fillRule));
                    table.pathFillRules[c.path] = c.fillRule;
                }
                break;
            }
            case RenderCmd::pathAddRawPath:
            {
                auto c = reader.read<PathRawPOD>();
                RawPath raw = rebuildRawPath(
                    reader.blobAt(c.blobOffset,
                                  c.verbCount *
                                      static_cast<uint32_t>(sizeof(PathVerb))),
                    reader.blobAt(c.pointsOffset,
                                  c.pointCount *
                                      static_cast<uint32_t>(sizeof(Vec2D))));
                if (auto* p = path(c.path))
                {
                    p->addRawPath(raw);
                }
                break;
            }
            case RenderCmd::pathAddRenderPath:
            {
                auto c = reader.read<PathAddPathPOD>();
                RenderPath* src = paths.get(c.src);
                if (auto* p = path(c.path))
                {
                    if (src)
                        p->addRenderPath(
                            src,
                            Mat2D(c.xx, c.xy, c.yx, c.yy, c.tx, c.ty));
                }
                break;
            }

            case RenderCmd::paintStyle:
            {
                auto c = reader.read<PaintU8POD>();
                if (auto* pt = paint(c.paint))
                {
                    pt->style(static_cast<RenderPaintStyle>(c.value));
                    table.paintShadows[c.paint].style = c.value;
                }
                break;
            }
            case RenderCmd::paintColor:
            {
                auto c = reader.read<PaintColorPOD>();
                if (auto* pt = paint(c.paint))
                {
                    pt->color(c.color);
                    table.paintShadows[c.paint].color = c.color;
                }
                break;
            }
            case RenderCmd::paintThickness:
            {
                auto c = reader.read<PaintFloatPOD>();
                if (auto* pt = paint(c.paint))
                {
                    pt->thickness(c.value);
                    table.paintShadows[c.paint].thickness = c.value;
                }
                break;
            }
            case RenderCmd::paintJoin:
            {
                auto c = reader.read<PaintU8POD>();
                if (auto* pt = paint(c.paint))
                {
                    pt->join(static_cast<StrokeJoin>(c.value));
                    table.paintShadows[c.paint].join = c.value;
                }
                break;
            }
            case RenderCmd::paintCap:
            {
                auto c = reader.read<PaintU8POD>();
                if (auto* pt = paint(c.paint))
                {
                    pt->cap(static_cast<StrokeCap>(c.value));
                    table.paintShadows[c.paint].cap = c.value;
                }
                break;
            }
            case RenderCmd::paintFeather:
            {
                auto c = reader.read<PaintFloatPOD>();
                if (auto* pt = paint(c.paint))
                {
                    pt->feather(c.value);
                    table.paintShadows[c.paint].feather = c.value;
                }
                break;
            }
            case RenderCmd::paintBlendMode:
            {
                auto c = reader.read<PaintU8POD>();
                if (auto* pt = paint(c.paint))
                {
                    pt->blendMode(static_cast<BlendMode>(c.value));
                    table.paintShadows[c.paint].blendMode = c.value;
                }
                break;
            }
            case RenderCmd::paintShader:
            {
                auto c = reader.read<PaintShaderPOD>();
                if (auto* pt = paint(c.paint))
                {
                    pt->shader(shaders.shared(c.shader));
                    table.paintShadows[c.paint].shader = c.shader;
                }
                break;
            }
            case RenderCmd::paintInvalidateStroke:
            {
                auto c = reader.read<ResIdPOD>();
                if (auto* pt = paint(c.id))
                {
                    pt->invalidateStroke();
                }
                break;
            }

            case RenderCmd::save:
                if (cur)
                {
                    cur->save();
                }
                break;
            case RenderCmd::restore:
                if (cur)
                {
                    cur->restore();
                }
                break;
            case RenderCmd::transform:
            {
                auto c = reader.read<TransformPOD>();
                if (cur)
                {
                    cur->transform(Mat2D(c.xx, c.xy, c.yx, c.yy, c.tx, c.ty));
                }
                break;
            }
            case RenderCmd::drawPath:
            {
                auto c = reader.read<DrawPathPOD>();
                RenderPath* p = paths.get(c.path, c.pathVersion);
                RenderPaint* pt = paints.get(c.paint, c.paintVersion);
                if (cur && p && pt)
                {
                    cur->drawPath(p, pt);
                }
                else if (cur != nullptr && hooks.stats != nullptr)
                {
                    hooks.stats->droppedDraws = hooks.stats->droppedDraws + 1;
                    replay_detail::logDroppedDraw(type, c.path, c.paint);
                }
                break;
            }
            case RenderCmd::clipPath:
            {
                auto c = reader.read<ClipPathPOD>();
                if (cur)
                {
                    if (auto* p = paths.get(c.path, c.version))
                        cur->clipPath(p);
                }
                break;
            }
            case RenderCmd::drawImage:
            {
                auto c = reader.read<DrawImagePOD>();
                RenderImage* im =
                    (c.image & kCanvasHandleFlag)
                        ? (hooks.canvasImage
                               ? hooks.canvasImage(c.image & kCanvasHandleMask)
                               : nullptr)
                        : image(c.image);
                if (cur && im)
                {
                    cur->drawImage(im,
                                   sampler(c.wrapX, c.wrapY, c.filter),
                                   static_cast<BlendMode>(c.blendMode),
                                   c.opacity);
                }
                else if (cur != nullptr && hooks.stats != nullptr)
                {
                    hooks.stats->droppedDraws = hooks.stats->droppedDraws + 1;
                    replay_detail::logDroppedDraw(type, c.image, 0);
                }
                break;
            }
            case RenderCmd::drawImageMesh:
            {
                auto c = reader.read<DrawImageMeshPOD>();
                RenderImage* im =
                    (c.image & kCanvasHandleFlag)
                        ? (hooks.canvasImage
                               ? hooks.canvasImage(c.image & kCanvasHandleMask)
                               : nullptr)
                        : image(c.image);
                rcp<RenderBuffer> vb =
                    buffers.shared(c.vertices, c.vertexVersion);
                rcp<RenderBuffer> uv = buffers.shared(c.uvCoords, c.uvVersion);
                rcp<RenderBuffer> ib =
                    buffers.shared(c.indices, c.indexVersion);
                if (cur && im && vb && uv && ib)
                {
                    cur->drawImageMesh(im,
                                       sampler(c.wrapX, c.wrapY, c.filter),
                                       vb,
                                       uv,
                                       ib,
                                       c.vertexCount,
                                       c.indexCount,
                                       static_cast<BlendMode>(c.blendMode),
                                       c.opacity);
                }
                else if (cur != nullptr && hooks.stats != nullptr)
                {
                    hooks.stats->droppedDraws = hooks.stats->droppedDraws + 1;
                    replay_detail::logDroppedDraw(type, c.image, 0);
                }
                break;
            }
            case RenderCmd::modulateOpacity:
            {
                auto c = reader.read<OpacityPOD>();
                if (cur)
                {
                    cur->modulateOpacity(c.opacity);
                }
                break;
            }

            case RenderCmd::canvasContentBegin:
            {
                auto c = reader.read<CanvasContentPOD>();
                cur = hooks.beginCanvasContent
                          ? hooks.beginCanvasContent(c.canvasId &
                                                         kCanvasHandleMask,
                                                     c.clearColor)
                          : nullptr;
                break;
            }
            case RenderCmd::canvasContentEnd:
            {
                reader.read<ResIdPOD>(); // advance past the canvas id
                cur = renderer;          // back to the screen; null drops draws
                break;
            }
        }
    }
    if (reader.overrun())
    {
        fprintf(stderr,
                "rive replay ABORT: payload overrun at byte %zu of %zu\n",
                reader.position(),
                commands.size());
        assert(false);
    }
}

} // namespace rive::cmd

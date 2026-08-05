/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/render_replay.hpp"
#include "rive/renderer/ore/cmd/ore_make_replay.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_types.hpp"

// What deferred replay is holding resident on the GPU side, walked out of the
// resident tables on demand.
//
// This is a levels counter, not an events counter: nothing is accumulated
// while recording or replaying, so the record and replay paths pay nothing at
// all for it and it needs no build flag. The cost is one linear walk of the
// resident tables, at the moment a caller asks. Callers must walk while the
// tables are quiescent - see DeferredConsumer::gpuCensus, which drains first.
namespace rive::cmd
{

// Bytes are the nominal footprint of the resource as declared: texels times
// bytes per texel, buffer sizes as requested. Driver padding, alignment and
// any backend side scratch are not visible from here and are excluded, so a
// total is a floor on real GPU residency. It is the right shape for asking
// whether two arrangements hold the same resources, which is what it is for.
struct GpuCensus
{
    // 2D resources.
    uint64_t imageBytes = 0;  // RenderImage, assumed 4 bytes per texel
    uint64_t bufferBytes = 0; // RenderBuffer, exact
    // Ore (scripting GPU) resources.
    uint64_t oreTextureBytes = 0; // ore::Texture, exact for uncompressed
    uint64_t oreBufferBytes = 0;  // ore::Buffer, exact

    // Live objects per table, so table shape is visible next to the bytes.
    // Paths, paints and shaders carry GPU cost that is not a declared
    // allocation (tessellation, gradient ramps), so they are counted but not
    // sized.
    uint32_t images = 0;
    uint32_t buffers = 0;
    uint32_t paths = 0;
    uint32_t paints = 0;
    uint32_t shaders = 0;
    uint32_t oreTextures = 0;
    uint32_t oreBuffers = 0;
    uint32_t oreOther = 0; // views, samplers, pipelines, bind groups

    // Slots ever minted, live or freed. slots - live is the hole count the
    // never-compacting tables carry.
    uint32_t slots2d = 0;
    uint32_t slotsOre = 0;

    uint64_t totalBytes() const
    {
        return imageBytes + bufferBytes + oreTextureBytes + oreBufferBytes;
    }

    uint32_t liveObjects() const
    {
        return images + buffers + paths + paints + shaders + oreTextures +
               oreBuffers + oreOther;
    }
};

// Texels across every mip level, array layer and MSAA sample. Returns 0 for a
// block compressed format rather than guessing a block size.
inline uint64_t oreTextureNominalBytes(const ore::Texture& t)
{
    uint32_t bpt = ore::textureFormatBytesPerTexel(t.format());
    if (bpt == 0)
    {
        return 0;
    }
    uint64_t texels = 0;
    uint32_t w = t.width(), h = t.height();
    // numMipmaps counts the full chain including level 0.
    for (uint32_t level = 0; level < std::max<uint32_t>(t.numMipmaps(), 1);
         ++level)
    {
        texels += uint64_t(w) * h;
        if (w == 1 && h == 1)
        {
            break;
        }
        w = std::max<uint32_t>(w >> 1, 1);
        h = std::max<uint32_t>(h >> 1, 1);
    }
    return texels * std::max<uint32_t>(t.depthOrArrayLayers(), 1) *
           std::max<uint32_t>(t.sampleCount(), 1) * bpt;
}

template <typename T>
static uint32_t countLive(const Resident<T>& r, uint32_t& slots)
{
    slots += static_cast<uint32_t>(r.objects.size());
    uint32_t live = 0;
    for (const rcp<T>& o : r.objects)
    {
        live += (o != nullptr);
    }
    return live;
}

inline GpuCensus takeGpuCensus(const ResourceTable& t2d,
                               const ore::cmd::OreResident& ore)
{
    GpuCensus c;
    c.paths = countLive(t2d.paths, c.slots2d);
    c.paints = countLive(t2d.paints, c.slots2d);
    c.shaders = countLive(t2d.shaders, c.slots2d);
    c.buffers = countLive(t2d.buffers, c.slots2d);
    c.images = countLive(t2d.images, c.slots2d);

    for (const rcp<RenderImage>& img : t2d.images.objects)
    {
        if (img != nullptr)
        {
            // No format on the 2D interface; every backend path here is
            // 32 bit color.
            c.imageBytes += uint64_t(img->width()) * img->height() * 4;
        }
    }
    for (const rcp<RenderBuffer>& buf : t2d.buffers.objects)
    {
        if (buf != nullptr)
        {
            c.bufferBytes += buf->sizeInBytes();
        }
    }

    c.slotsOre = static_cast<uint32_t>(ore.objects.size());
    for (size_t i = 0; i < ore.objects.size(); ++i)
    {
        rive::gpu::GPUResource* o = ore.objects[i].get();
        if (o == nullptr)
        {
            continue;
        }
        switch (ore.kinds[i])
        {
            case ore::cmd::OreKind::texture:
                c.oreTextures++;
                c.oreTextureBytes +=
                    oreTextureNominalBytes(*static_cast<ore::Texture*>(o));
                break;
            case ore::cmd::OreKind::buffer:
                c.oreBuffers++;
                c.oreBufferBytes += static_cast<ore::Buffer*>(o)->size();
                break;
            default:
                c.oreOther++;
                break;
        }
    }
    return c;
}

} // namespace rive::cmd

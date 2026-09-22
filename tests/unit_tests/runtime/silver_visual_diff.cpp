/*
 * Copyright 2026 Rive
 */

// Renders a tarnished SRIV next to its baseline and reports whether the two
// actually differ on screen.
//
// A silver test fails on any byte difference in the recorded command stream,
// which is stricter than "the frame changed": dropping a redundant rewind and
// addRawPath pair, or reordering equivalent calls, moves those bytes without
// moving a pixel. This replays both streams through the same renderer and
// compares the frames, so a rebaseline can be justified rather than trusted.
//
// Hidden by default (the leading dot in the tag). Run it directly against the
// built binary, NOT through test.sh, which wipes silvers/tarnished on startup:
//
//     ./out/release/unit_tests "[.silverdiff]"
//
// SILVER_DIFF_NAME=<silver name> limits it to one. Differing frames are
// written to silvers/tarnished/diff/ as baseline, tarnished and a difference
// image.

#include "rive/rive_types.hpp"
#include "rive_testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(RIVE_MACOSX) && !defined(RIVE_NO_FILESYSTEM)

#include "common/write_png_file.hpp"
#include "cg_factory.hpp"
#include "cg_renderer.hpp"
#include "utils/serialized_replay.hpp"

#include <CoreGraphics/CoreGraphics.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <set>

using namespace rive;

namespace
{
constexpr uint32_t kClearColor = 0x00000000;

std::vector<uint8_t> readFile(const std::string& path)
{
    std::vector<uint8_t> bytes;
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp == nullptr)
    {
        return bytes;
    }
    fseek(fp, 0, SEEK_END);
    bytes.resize((size_t)ftell(fp));
    fseek(fp, 0, SEEK_SET);
    if (fread(bytes.data(), 1, bytes.size(), fp) != bytes.size())
    {
        bytes.clear();
    }
    fclose(fp);
    return bytes;
}

uint64_t hashPixels(const std::vector<uint32_t>& pixels)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (uint32_t pixel : pixels)
    {
        hash = (hash ^ pixel) * 0x100000001b3ull;
    }
    return hash;
}

// One replay of a stream into a CoreGraphics bitmap. The renderer is CPU and
// deterministic, which is all this needs: both sides go through it, so the
// comparison is exact even where the rasterizer is approximate.
struct Replay
{
    // Frames whose pixels the caller wants kept. Everything else is hashed and
    // dropped -- a 2370x1680 stream is 16MB a frame.
    std::set<int> keep;

    std::vector<uint64_t> hashes;
    std::vector<std::vector<uint32_t>> kept;
    uint32_t width = 0;
    uint32_t height = 0;
    int resizes = 0;
    bool ok = false;

    bool run(const std::vector<uint8_t>& stream)
    {
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = nullptr;
        std::vector<uint32_t> pixels;
        std::unique_ptr<CGRenderer> renderer;
        CGFactory factory;

        auto clear = [&pixels]() {
            std::fill(pixels.begin(), pixels.end(), kClearColor);
        };

        SerializedReplayHooks hooks;
        hooks.onFrameSize = [&](uint32_t w, uint32_t h) {
            if (w == width && h == height && ctx != nullptr)
            {
                return;
            }
            resizes++;
            width = w;
            height = h;
            pixels.assign((size_t)w * h, kClearColor);
            // ~CGRenderer restores a graphics state on its context, so the
            // renderer has to go first or it restores through a freed one.
            renderer.reset();
            if (ctx != nullptr)
            {
                CGContextRelease(ctx);
            }
            const uint32_t info =
                static_cast<uint32_t>(kCGBitmapByteOrder32Big) |
                static_cast<uint32_t>(kCGImageAlphaPremultipliedLast);
            ctx = CGBitmapContextCreate(pixels.data(),
                                        w,
                                        h,
                                        8,
                                        w * 4,
                                        space,
                                        info);
            renderer = std::make_unique<CGRenderer>(ctx, (int)w, (int)h);
        };
        hooks.onFrame = [&]() {
            if (ctx == nullptr)
            {
                return;
            }
            CGContextFlush(ctx);
            const int index = (int)hashes.size();
            hashes.push_back(hashPixels(pixels));
            if (keep.count(index) != 0)
            {
                kept.push_back(pixels);
            }
            clear();
        };

        // The replayer takes one renderer for the whole stream, so the first
        // frameSize has to arrive before any drawing. Silvers emit it up front.
        Replay* self = this;
        (void)self;
        bool result = false;
        {
            // A renderer is needed up front; the stream's first op is the
            // frame size, which builds it. Until then draws have nowhere to
            // go, so replay in two steps: peek the size, then replay for real.
            std::vector<uint32_t> probePixels(1);
            const uint32_t info =
                static_cast<uint32_t>(kCGBitmapByteOrder32Big) |
                static_cast<uint32_t>(kCGImageAlphaPremultipliedLast);
            CGContextRef probeCtx = CGBitmapContextCreate(probePixels.data(),
                                                          1,
                                                          1,
                                                          8,
                                                          4,
                                                          space,
                                                          info);
            uint32_t w = 0, h = 0;
            {
                // Scoped so the probe renderer is destroyed, and restores its
                // graphics state, before the context it holds is released.
                CGRenderer probeRenderer(probeCtx, 1, 1);
                SerializedReplayHooks sizeOnly;
                sizeOnly.onFrameSize = [&w, &h](uint32_t width,
                                                uint32_t height) {
                    if (w == 0)
                    {
                        w = width;
                        h = height;
                    }
                };
                replaySerializedCommands(
                    Span<const uint8_t>(stream.data(), stream.size()),
                    &factory,
                    &probeRenderer,
                    sizeOnly);
            }
            CGContextRelease(probeCtx);
            if (w == 0 || h == 0)
            {
                CGColorSpaceRelease(space);
                return false;
            }
            hooks.onFrameSize(w, h);
            result = replaySerializedCommands(
                Span<const uint8_t>(stream.data(), stream.size()),
                &factory,
                renderer.get(),
                hooks);
        }

        renderer.reset();
        if (ctx != nullptr)
        {
            CGContextRelease(ctx);
        }
        CGColorSpaceRelease(space);
        ok = result;
        return result;
    }
};

void writePNG(const std::string& path,
              std::vector<uint32_t> pixels,
              uint32_t w,
              uint32_t h)
{
    WritePNGFile(reinterpret_cast<uint8_t*>(pixels.data()),
                 (int)w,
                 (int)h,
                 false,
                 path.c_str(),
                 PNGCompression::fast_rle);
}

// A raw difference of a few units out of 255 is invisible, and these are
// needles in a 4 megapixel haystack, so mark every differing pixel in red at
// an intensity that says how far off it was. Black means identical.
std::vector<uint32_t> differenceImage(const std::vector<uint32_t>& a,
                                      const std::vector<uint32_t>& b)
{
    std::vector<uint32_t> out(a.size());
    for (size_t i = 0; i < a.size(); i++)
    {
        const uint8_t* pa = reinterpret_cast<const uint8_t*>(&a[i]);
        const uint8_t* pb = reinterpret_cast<const uint8_t*>(&b[i]);
        uint8_t* po = reinterpret_cast<uint8_t*>(&out[i]);
        int worst = 0;
        for (int c = 0; c < 4; c++)
        {
            worst = std::max(worst, std::abs((int)pa[c] - (int)pb[c]));
        }
        po[0] = worst == 0 ? 0 : (uint8_t)std::min(255, 96 + worst * 8);
        po[1] = 0;
        po[2] = 0;
        po[3] = 0xff;
    }
    return out;
}

struct FrameDiff
{
    size_t differingPixels = 0;
    int maxChannelDelta = 0;
    // Bounding box of the differing pixels, so the report says where.
    uint32_t minX = UINT32_MAX, minY = UINT32_MAX, maxX = 0, maxY = 0;
    bool any() const { return differingPixels != 0; }
};

FrameDiff compareFrames(const std::vector<uint32_t>& a,
                        const std::vector<uint32_t>& b,
                        uint32_t width)
{
    FrameDiff diff;
    for (size_t i = 0; i < a.size(); i++)
    {
        if (a[i] == b[i])
        {
            continue;
        }
        diff.differingPixels++;
        const uint32_t x = (uint32_t)(i % width);
        const uint32_t y = (uint32_t)(i / width);
        diff.minX = std::min(diff.minX, x);
        diff.minY = std::min(diff.minY, y);
        diff.maxX = std::max(diff.maxX, x);
        diff.maxY = std::max(diff.maxY, y);
        const uint8_t* pa = reinterpret_cast<const uint8_t*>(&a[i]);
        const uint8_t* pb = reinterpret_cast<const uint8_t*>(&b[i]);
        for (int c = 0; c < 4; c++)
        {
            diff.maxChannelDelta = std::max(diff.maxChannelDelta,
                                            std::abs((int)pa[c] - (int)pb[c]));
        }
    }
    return diff;
}

// A close-up of the region that differs, padded so there is context around it.
// Without this the evidence is a handful of pixels in a 4 megapixel frame.
std::vector<uint32_t> crop(const std::vector<uint32_t>& pixels,
                           uint32_t width,
                           uint32_t height,
                           uint32_t x0,
                           uint32_t y0,
                           uint32_t w,
                           uint32_t h)
{
    std::vector<uint32_t> out((size_t)w * h, 0xff000000);
    for (uint32_t y = 0; y < h; y++)
    {
        if (y0 + y >= height)
        {
            break;
        }
        for (uint32_t x = 0; x < w; x++)
        {
            if (x0 + x >= width)
            {
                break;
            }
            out[(size_t)y * w + x] =
                pixels[(size_t)(y0 + y) * width + (x0 + x)];
        }
    }
    return out;
}

} // namespace

TEST_CASE("tarnished silvers render the same as their baselines",
          "[.silverdiff]")
{
    const std::string tarnishedDir = "silvers/tarnished/";
    const std::string outDir = tarnishedDir + "diff/";
    if (!std::filesystem::exists(tarnishedDir))
    {
        WARN("no silvers/tarnished directory -- run the suite first, and note "
             "that test.sh wipes it on startup");
        return;
    }

    const char* only = getenv("SILVER_DIFF_NAME");
    std::vector<std::string> names;
    for (auto& entry : std::filesystem::directory_iterator(tarnishedDir))
    {
        if (entry.path().extension() != ".sriv")
        {
            continue;
        }
        const std::string name = entry.path().stem().string();
        if (only == nullptr || name == only)
        {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    if (names.empty())
    {
        WARN("no tarnished silvers to compare");
        return;
    }

    size_t identical = 0, differing = 0, unreadable = 0;
    size_t worstPixelsOverall = 0;
    int worstChannelOverall = 0;
    for (const auto& name : names)
    {
        auto baselineBytes = readFile("silvers/" + name + ".sriv");
        auto tarnishedBytes = readFile(tarnishedDir + name + ".sriv");
        if (baselineBytes.empty() || tarnishedBytes.empty())
        {
            fprintf(stderr,
                    "[silverdiff] %-48s MISSING BASELINE\n",
                    name.c_str());
            unreadable++;
            // Nothing was compared, so this cannot count as a pass.
            CHECK(false);
            continue;
        }

        Replay baseline, tarnished;
        const bool replayed =
            baseline.run(baselineBytes) && tarnished.run(tarnishedBytes);
        if (!replayed)
        {
            fprintf(stderr, "[silverdiff] %-48s REPLAY FAILED\n", name.c_str());
            unreadable++;
            CHECK(replayed);
            continue;
        }

        if (baseline.width != tarnished.width ||
            baseline.height != tarnished.height ||
            baseline.hashes.size() != tarnished.hashes.size())
        {
            fprintf(stderr,
                    "[silverdiff] %-48s SHAPE DIFFERS %ux%u/%zu frames vs "
                    "%ux%u/%zu\n",
                    name.c_str(),
                    baseline.width,
                    baseline.height,
                    baseline.hashes.size(),
                    tarnished.width,
                    tarnished.height,
                    tarnished.hashes.size());
            differing++;
            CHECK(false);
            continue;
        }

        std::set<int> mismatched;
        for (size_t i = 0; i < baseline.hashes.size(); i++)
        {
            if (baseline.hashes[i] != tarnished.hashes[i])
            {
                mismatched.insert((int)i);
            }
        }
        if (mismatched.empty())
        {
            fprintf(stderr,
                    "[silverdiff] %-48s identical (%zu frames, %ux%u)\n",
                    name.c_str(),
                    baseline.hashes.size(),
                    baseline.width,
                    baseline.height);
            identical++;
            continue;
        }

        // Second pass over the frames that differ, this time keeping pixels.
        std::set<int> keep;
        for (int index : mismatched)
        {
            if (keep.size() >= 3)
            {
                break;
            }
            keep.insert(index);
        }
        Replay baselinePixels, tarnishedPixels;
        baselinePixels.keep = keep;
        tarnishedPixels.keep = keep;
        baselinePixels.run(baselineBytes);
        tarnishedPixels.run(tarnishedBytes);

        std::filesystem::create_directories(outDir);
        size_t worstPixels = 0;
        int worstChannel = 0;
        size_t k = 0;
        for (int index : keep)
        {
            if (k >= baselinePixels.kept.size() ||
                k >= tarnishedPixels.kept.size())
            {
                break;
            }
            const auto& a = baselinePixels.kept[k];
            const auto& b = tarnishedPixels.kept[k];
            const FrameDiff diff = compareFrames(a, b, baseline.width);
            worstPixels = std::max(worstPixels, diff.differingPixels);
            worstChannel = std::max(worstChannel, diff.maxChannelDelta);

            const std::string stem =
                outDir + name + "-frame" + std::to_string(index);
            writePNG(stem + "-baseline.png",
                     a,
                     baseline.width,
                     baseline.height);
            writePNG(stem + "-tarnished.png",
                     b,
                     baseline.width,
                     baseline.height);
            writePNG(stem + "-diff.png",
                     differenceImage(a, b),
                     baseline.width,
                     baseline.height);

            if (diff.any())
            {
                // Pad the differing region out to at least 192px so there is
                // something recognizable around it.
                constexpr uint32_t kMin = 192;
                uint32_t w = std::max(kMin, diff.maxX - diff.minX + 1);
                uint32_t h = std::max(kMin, diff.maxY - diff.minY + 1);
                uint32_t cx = (diff.minX + diff.maxX) / 2;
                uint32_t cy = (diff.minY + diff.maxY) / 2;
                uint32_t x0 = cx > w / 2 ? cx - w / 2 : 0;
                uint32_t y0 = cy > h / 2 ? cy - h / 2 : 0;
                writePNG(stem + "-crop-baseline.png",
                         crop(a, baseline.width, baseline.height, x0, y0, w, h),
                         w,
                         h);
                writePNG(stem + "-crop-tarnished.png",
                         crop(b, baseline.width, baseline.height, x0, y0, w, h),
                         w,
                         h);
                fprintf(stderr,
                        "               frame %d: %zu pixels differ, max "
                        "channel delta %d, within (%u,%u)-(%u,%u)\n",
                        index,
                        diff.differingPixels,
                        diff.maxChannelDelta,
                        diff.minX,
                        diff.minY,
                        diff.maxX,
                        diff.maxY);
            }
            k++;
        }

        fprintf(stderr,
                "[silverdiff] %-48s DIFFERS on %zu/%zu frames, worst frame %zu "
                "pixels, max channel delta %d -> %s\n",
                name.c_str(),
                mismatched.size(),
                baseline.hashes.size(),
                worstPixels,
                worstChannel,
                outDir.c_str());
        worstPixelsOverall = std::max(worstPixelsOverall, worstPixels);
        worstChannelOverall = std::max(worstChannelOverall, worstChannel);
        differing++;
        CHECK(mismatched.empty());
    }

    fprintf(stderr,
            "[silverdiff] %zu identical, %zu differing, %zu unreadable. Worst "
            "frame anywhere: %zu pixels, max channel delta %d/255.\n",
            identical,
            differing,
            unreadable,
            worstPixelsOverall,
            worstChannelOverall);
}

#else

TEST_CASE("tarnished silvers render the same as their baselines",
          "[.silverdiff]")
{
    WARN("silver visual diff needs macOS (CoreGraphics) and std::filesystem");
}

#endif

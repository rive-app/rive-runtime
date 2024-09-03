#include "SkImage.h"
#include "SkPixmap.h"
#include "SkStream.h"
#include "image_cmp.hpp"

#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <vector>

static int byte0(uint32_t p) { return (p >> 0) & 0xFF; }
static int byte1(uint32_t p) { return (p >> 8) & 0xFF; }
static int byte2(uint32_t p) { return (p >> 16) & 0xFF; }
static int byte3(uint32_t p) { return (p >> 24) & 0xFF; }

static uint32_t pack(unsigned a, unsigned b, unsigned c, unsigned d)
{
    return (a << 24) | (b << 16) | (c << 8) | (d << 0);
}

static ImageCmp pixmap_cmp(const SkPixmap& pa, const SkPixmap& pb)
{
    assert(pa.width() == pb.width());
    assert(pa.height() == pb.height());

    size_t pixcount = pa.width() * pa.height();
    int maxdiff = 0;
    size_t rgb_diffcount = 0;
    size_t pixel_diffcount = 0;

    for (int y = 0; y < pa.height(); ++y)
    {
        const uint32_t* row0 = pa.addr32(0, y);
        const uint32_t* row1 = pb.addr32(0, y);

        for (int x = 0; x < pa.width(); ++x)
        {
            const uint32_t p0 = row0[x];
            const uint32_t p1 = row1[x];
            if (p0 == p1)
            {
                continue;
            }
            // Find the max diff in this pixel's r,g,b,a.
            int rgb_diff = std::abs(byte0(p0) - byte0(p1));
            rgb_diff = std::max(rgb_diff, std::abs(byte1(p0) - byte1(p1)));
            rgb_diff = std::max(rgb_diff, std::abs(byte2(p0) - byte2(p1)));
            rgb_diff = std::max(rgb_diff, std::abs(byte3(p0) - byte3(p1)));
            maxdiff = std::max(maxdiff, rgb_diff);
            rgb_diffcount += rgb_diff;
            if (rgb_diff != 0)
            {
                ++pixel_diffcount;
            }
        }
    }

    return ImageCmp::diff(maxdiff, (float)rgb_diffcount / (pixcount * 255), pixel_diffcount);
}

ImageCmp image_cmp(sk_sp<SkImage> a, sk_sp<SkImage> b)
{
    if (a->width() != b->width() || a->height() != b->height())
    {
        return ImageCmp::dimensionMismatch();
    }

    // ensure we can peek at the pixels
    a = a->makeRasterImage();
    b = b->makeRasterImage();

    SkPixmap pa, pb;
    if (!a->peekPixels(&pa) || !b->peekPixels(&pb))
    {
        return ImageCmp::failedToLoad();
    }

    return pixmap_cmp(pa, pb);
}

uint32_t compute_diff0(uint32_t p0, uint32_t p1)
{
    auto a = std::abs(byte0(p0) - byte0(p1));
    auto b = std::abs(byte1(p0) - byte1(p1));
    auto c = std::abs(byte2(p0) - byte2(p1));
    return pack(0xFF, a, b, c);
}

uint32_t compute_diff1(uint32_t p0, uint32_t p1)
{
    auto d = p0 == p1 ? 0 : 0xFF;
    return pack(0xFF, d, d, d);
}

sk_sp<SkImage> make_diff_image(sk_sp<SkImage> a,
                               sk_sp<SkImage> b,
                               uint32_t (*compute_diff)(uint32_t a, uint32_t b))
{
    // ensure we can peek at the pixels
    a = a->makeRasterImage();
    b = b->makeRasterImage();

    SkPixmap pa, pb;
    if (!a->peekPixels(&pa) || !b->peekPixels(&pb))
    {
        throw "failed to peek";
    }

    const auto info = SkImageInfo::MakeN32Premul(pa.width(), pa.height());
    std::vector<uint32_t> pixels(pa.width() * pa.height());
    auto df = SkPixmap(info, pixels.data(), pa.width() * sizeof(uint32_t));

    for (int y = 0; y < df.height(); ++y)
    {
        for (int x = 0; x < df.width(); ++x)
        {
            *df.writable_addr32(x, y) = compute_diff(*pa.addr32(x, y), *pb.addr32(x, y));
        }
    }

    return SkImage::MakeRasterCopy(df);
}

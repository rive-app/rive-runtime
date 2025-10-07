#ifndef IMAGE_SAMPLER
#define IMAGE_SAMPLER

#include <stdint.h>

namespace rive
{
enum class ImageFilter : uint8_t
{
    // High fidelity linear filter in all 2 directions: x, y
    bilinear = 0,
    // Sample with low fidelity, good for things like pixel art.
    nearest = 1,
};

constexpr size_t NUM_IMAGE_FILTERS = 2;

enum class ImageWrap : uint8_t
{
    // Clamp to the color of the nearest edge when a texture sample falls
    // outside 0..1.
    clamp = 0,
    // Repeat when a texture sample falls outside 0..1 (e.g., fmod(coord, 1)).
    repeat = 1,
    // Similar to repeat, but also mirror the coordinate with each repeat.
    mirror = 2,
};

constexpr size_t NUM_IMAGE_WRAP = 3;

struct ImageSampler
{
    static constexpr ImageSampler LinearClamp() { return {}; }

    constexpr static uint8_t LINEAR_CLAMP_SAMPLER_KEY = 0;

    ImageWrap wrapX = ImageWrap::clamp;
    ImageWrap wrapY = ImageWrap::clamp;
    // How to sample the texture, this will be for both MIN and MAG filtering.
    ImageFilter filter = ImageFilter::bilinear;

    bool operator==(const ImageSampler other) const
    {
        return other.wrapX == wrapX && other.wrapY == wrapY &&
               other.filter == filter;
    }

    bool operator!=(const ImageSampler other) const
    {
        return !(*this == other);
    }

    // The maximum number of possible combinations of sampler options. Used for
    // array length in implementations.
    static constexpr size_t MAX_SAMPLER_PERMUTATIONS =
        NUM_IMAGE_FILTERS * NUM_IMAGE_WRAP * NUM_IMAGE_WRAP;

    // Convert struct to a key that can be used to index an array to get a
    // unique sampler that represents these options.
    const uint8_t asKey() const
    {
        return static_cast<int>(wrapX) +
               (static_cast<int>(wrapY) * NUM_IMAGE_WRAP) +
               (static_cast<int>(filter) * NUM_IMAGE_WRAP * NUM_IMAGE_WRAP);
    }

    static ImageSampler SamplerFromKey(uint8_t key)
    {
        // Android wouldn't compile with {} style initialization so do it this
        // way instead.
        ImageSampler sampler;

        sampler.wrapX = GetWrapXOptionFromKey(key);
        sampler.wrapY = GetWrapYOptionFromKey(key);
        sampler.filter = GetFilterOptionFromKey(key);

        return sampler;
    }

    static ImageWrap GetWrapXOptionFromKey(uint8_t key)
    {
        return static_cast<ImageWrap>(key % NUM_IMAGE_WRAP);
    }

    static ImageWrap GetWrapYOptionFromKey(uint8_t key)
    {
        return static_cast<ImageWrap>((key / NUM_IMAGE_WRAP) % NUM_IMAGE_WRAP);
    }

    static ImageFilter GetFilterOptionFromKey(uint8_t key)
    {
        return static_cast<ImageFilter>(key /
                                        (NUM_IMAGE_WRAP * NUM_IMAGE_WRAP));
    }
};
} // namespace rive
#endif

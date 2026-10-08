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

constexpr size_t ImageFilterCount = 2;

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

constexpr size_t ImageWrapCount = 3;

struct ImageSampler
{
    static constexpr ImageSampler LinearClamp() { return {}; }
    static constexpr ImageSampler LinearWrap()
    {
        return {ImageWrap::repeat, ImageWrap::repeat};
    }

    static constexpr uint8_t makeKey(ImageFilter filter,
                                     ImageWrap wrapX,
                                     ImageWrap wrapY)
    {
        return uint8_t(int(wrapX) + (int(wrapY) * ImageWrapCount) +
                       (int(filter) * ImageWrapCount * ImageWrapCount));
    }

    static constexpr uint8_t makeKey(ImageFilter filter, ImageWrap wrap)
    {
        return makeKey(filter, wrap, wrap);
    }

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
        ImageFilterCount * ImageWrapCount * ImageWrapCount;

    // Convert struct to a key that can be used to index an array to get a
    // unique sampler that represents these options.
    constexpr uint8_t asKey() const { return makeKey(filter, wrapX, wrapY); }

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
        return static_cast<ImageWrap>(key % ImageWrapCount);
    }

    static ImageWrap GetWrapYOptionFromKey(uint8_t key)
    {
        return static_cast<ImageWrap>((key / ImageWrapCount) % ImageWrapCount);
    }

    static ImageFilter GetFilterOptionFromKey(uint8_t key)
    {
        return static_cast<ImageFilter>(key /
                                        (ImageWrapCount * ImageWrapCount));
    }
};

// These two keys are used frequently, so give them names.
// Note: Ideally these would be ImageSampler::BilinearClampKey etc, but there is
// no way to call makeKey from within the class at static constexpr time, as the
// class is not yet a complete type, so they're free declarations instead.
constexpr auto BilinearClampImageSamplerKey =
    ImageSampler::makeKey(ImageFilter::bilinear, ImageWrap::clamp);
constexpr auto BilinearRepeatImageSamplerKey =
    ImageSampler::makeKey(ImageFilter::bilinear, ImageWrap::repeat);
} // namespace rive
#endif

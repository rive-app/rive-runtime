#ifdef RIVE_RENDERER_TESS
#include "viewer/sample_tools/sample_atlas_packer.hpp"
#include "utils/no_op_factory.hpp"
#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/file.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/tess/sokol/sokol_tess_renderer.hpp"
#include <algorithm>

using namespace rive;

class AtlasRenderImage : public lite_rtti_override<RenderImage, AtlasRenderImage>
{
private:
    std::vector<uint8_t> m_Pixels;

public:
    AtlasRenderImage(const uint8_t* pixels, uint32_t width, uint32_t height) :
        m_Pixels(pixels, pixels + (width * height * 4))
    {
        m_Width = width;
        m_Height = height;
    }

    Span<const uint8_t> pixels() { return m_Pixels; }
};

class AtlasPackerFactory : public NoOpFactory
{
    rcp<RenderImage> decodeImage(Span<const uint8_t> bytes) override
    {
        auto bitmap = Bitmap::decode(bytes.data(), bytes.size());
        if (bitmap)
        {
            // We have a bitmap, let's make an image.

            // For now only deal with RGBA.
            if (bitmap->pixelFormat() != Bitmap::PixelFormat::RGBA)
            {
                bitmap->pixelFormat(Bitmap::PixelFormat::RGBA);
            }

            return make_rcp<AtlasRenderImage>(bitmap->bytes(), bitmap->width(), bitmap->height());
        }
        return nullptr;
    }
};

SampleAtlasPacker::SampleAtlasPacker(uint32_t maxWidth, uint32_t maxHeight) :
    m_maxWidth(maxWidth), m_maxHeight(maxHeight)
{}

void SampleAtlasPacker::pack(Span<const uint8_t> rivBytes)
{
    AtlasPackerFactory factory;
    if (auto file = rive::File::import(rivBytes, &factory))
    {
        for (auto asset : file->assets())
        {
            if (asset->is<ImageAsset>())
            {
                Mat2D uvTransform;

                auto imageAsset = asset->as<ImageAsset>();
                LITE_RTTI_CAST_OR_CONTINUE(renderImage,
                                           AtlasRenderImage*,
                                           imageAsset->renderImage());

                if (m_atlases.empty())
                {
                    // Make the first atlas.
                    m_atlases.push_back(new SampleAtlas(m_maxWidth, m_maxHeight));
                }

                // Pack into the current atlas.
                if (!m_atlases.back()->pack(renderImage->pixels().data(),
                                            renderImage->width(),
                                            renderImage->height(),
                                            uvTransform))
                {
                    // Didn't fit in previous atlas, make a new one.
                    auto nextAtlas = new SampleAtlas(m_maxWidth, m_maxHeight);

                    // Try to pack into the new one.
                    if (!nextAtlas->pack(renderImage->pixels().data(),
                                         renderImage->width(),
                                         renderImage->height(),
                                         uvTransform))
                    {
                        // Still failed. Image must be larger than max atlas. Just push the whole
                        // image as an atlas.
                        m_atlases.push_back(new SampleAtlas(renderImage->pixels().data(),
                                                            renderImage->width(),
                                                            renderImage->height()));

                        // Clean up unused next atlas.
                        delete nextAtlas;
                    }
                    else
                    {
                        m_atlases.push_back(nextAtlas);
                    }
                }

                // Store where the image ended up.

                m_lookup[asset->assetId()] = {
                    .atlasIndex = m_atlases.size() - 1,
                    .transform = uvTransform,
                    .width = (uint32_t)renderImage->width(),
                    .height = (uint32_t)renderImage->height(),
                };
            }
        }
    }
}

SampleAtlasPacker::~SampleAtlasPacker()
{
    for (auto atlas : m_atlases)
    {
        delete atlas;
    }
}

SampleAtlas* SampleAtlasPacker::atlas(std::size_t index)
{
    assert(index < m_atlases.size());
    return m_atlases[index];
}

SampleAtlas::SampleAtlas(const uint8_t* pixels, uint32_t width, uint32_t height) :
    m_width(width), m_height(height), m_pixels(pixels, pixels + width * height * 4)
{}

SampleAtlas::SampleAtlas(uint32_t width, uint32_t height) :
    m_width(width), m_height(height), m_pixels(width * height * 4)
{}

bool SampleAtlas::pack(const uint8_t* sourcePixels,
                       uint32_t width,
                       uint32_t height,
                       Mat2D& packTransform)
{
    if (m_x + width >= m_width)
    {
        m_x = 0;
        m_y = m_nextY;
    }
    // Check if we overflow vertically, we're done.
    if (m_y + height >= m_height)
    {
        return false;
    }

    // We fit, pack into m_x, m_y.
    for (uint32_t sy = 0; sy < height; sy++)
    {
        for (uint32_t sx = 0; sx < width; sx++)
        {
            for (uint8_t channel = 0; channel < 4; channel++)
            {
                m_pixels[((m_y + sy) * m_width + (m_x + sx)) * 4 + channel] =
                    sourcePixels[(sy * width + sx) * 4 + channel];
            }
        }
    }

    // Set the UV transform.
    packTransform = {
        width / (float)m_width,
        0,
        0,
        height / (float)m_height,
        m_x / (float)m_width,
        m_y / (float)m_height,
    };

    // Increment internal positions.
    m_x += width;
    if (m_y + height > m_nextY)
    {
        m_nextY = m_y + height;
    }

    return true;
}

bool SampleAtlasPacker::find(const ImageAsset& asset, SampleAtlasLocation* location)
{
    auto assetId = asset.assetId();
    auto result = m_lookup.find(assetId);
    if (result != m_lookup.end())
    {
        *location = result->second;
        return true;
    }
    return false;
}

SampleAtlasLoader::SampleAtlasLoader(SampleAtlasPacker* packer) : m_packer(packer) {}

bool SampleAtlasLoader::loadContents(FileAsset& asset, Span<const uint8_t> inBandBytes, Factory*)
{
    if (asset.is<ImageAsset>())
    {
        SampleAtlasLocation location;
        auto imageAsset = asset.as<ImageAsset>();

        // Find which location this image got packed into.
        if (m_packer->find(*imageAsset, &location))
        {
            // Determine if we've already loaded the a render image.
            auto sharedItr = m_sharedImageResources.find(location.atlasIndex);

            rive::rcp<rive::SokolRenderImageResource> imageResource;
            if (sharedItr != m_sharedImageResources.end())
            {
                imageResource = sharedItr->second;
            }
            else
            {
                auto atlas = m_packer->atlas(location.atlasIndex);
                imageResource = rive::rcp<rive::SokolRenderImageResource>(
                    new SokolRenderImageResource(atlas->pixels().data(),
                                                 atlas->width(),
                                                 atlas->height()));
                m_sharedImageResources[location.atlasIndex] = imageResource;
            }

            // Make a render image from the atlas if we haven't yet. Our Factory
            // doesn't have an abstraction for creating a RenderImage from a
            // decoded set of pixels, we should either add that or live with/be
            // ok with the fact that most people writing a resolver doing
            // something like this will likely also have a custom/specific
            // renderer (and hence will know which RenderImage they need to
            // make).

            imageAsset->renderImage(make_rcp<SokolRenderImage>(imageResource,
                                                               location.width,
                                                               location.height,
                                                               location.transform));
            return true;
        }
    }
    return false;
}
#endif

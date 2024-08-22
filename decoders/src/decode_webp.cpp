#include "rive/decoders/bitmap_decoder.hpp"
#include "webp/decode.h"
#include "webp/demux.h"
#include <stdio.h>
#include <vector>
#include <memory>

std::unique_ptr<Bitmap> DecodeWebP(const uint8_t bytes[], size_t byteCount)
{
    WebPDecoderConfig config;
    if (!WebPInitDecoderConfig(&config))
    {
        fprintf(stderr, "DecodeWebP - Library version mismatch!\n");
        return nullptr;
    }
    config.options.dithering_strength = 50;
    config.options.alpha_dithering_strength = 100;

    if (!WebPGetInfo(bytes, byteCount, nullptr, nullptr))
    {
        fprintf(stderr, "DecodeWebP - Input file doesn't appear to be WebP format.\n");
    }

    WebPData data = {bytes, byteCount};
    WebPDemuxer* demuxer = WebPDemux(&data);
    if (demuxer == nullptr)
    {
        fprintf(stderr, "DecodeWebP - Could not create demuxer.\n");
    }

    WebPIterator currentFrame;
    if (!WebPDemuxGetFrame(demuxer, 1, &currentFrame))
    {
        fprintf(stderr, "DecodeWebP - WebPDemuxGetFrame couldn't get frame.\n");
        WebPDemuxDelete(demuxer);
        return nullptr;
    }
    config.output.colorspace = MODE_RGBA;

    uint32_t width = WebPDemuxGetI(demuxer, WEBP_FF_CANVAS_WIDTH);
    uint32_t height = WebPDemuxGetI(demuxer, WEBP_FF_CANVAS_HEIGHT);

    size_t pixelBufferSize =
        static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(4);
    std::unique_ptr<uint8_t[]> pixelBuffer = std::make_unique<uint8_t[]>(pixelBufferSize);

    config.output.u.RGBA.rgba = (uint8_t*)pixelBuffer.get();
    config.output.u.RGBA.stride = (int)(width * 4);
    config.output.u.RGBA.size = pixelBufferSize;
    config.output.is_external_memory = 1;

    if (WebPDecode(currentFrame.fragment.bytes, currentFrame.fragment.size, &config) !=
        VP8_STATUS_OK)
    {
        fprintf(stderr, "DecodeWebP - WebPDemuxGetFrame couldn't decode.\n");
        WebPDemuxReleaseIterator(&currentFrame);
        WebPDemuxDelete(demuxer);
        return nullptr;
    }

    WebPDemuxReleaseIterator(&currentFrame);
    WebPDemuxDelete(demuxer);

    return std::make_unique<Bitmap>(width,
                                    height,
                                    Bitmap::PixelFormat::RGBA,
                                    std::move(pixelBuffer));
}
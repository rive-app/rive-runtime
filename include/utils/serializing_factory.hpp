/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_SERIALIZING_FACTORY_HPP_
#define _RIVE_SERIALIZING_FACTORY_HPP_

#include "rive/factory.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include <vector>

namespace rive
{
// A factory that generates render objects which serialize their rendering
// commands into one buffer that can then be used to replay the commands in a
// viewer app or compare them to ensure that subsequent runs generate the same
// commands.
class SerializingFactory : public Factory
{
public:
    SerializingFactory();
    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                       RenderBufferFlags,
                                       size_t) override;

    rcp<RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override;

    rcp<RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override;

    rcp<RenderPath> makeRenderPath(RawPath&, FillRule) override;

    rcp<RenderPath> makeEmptyRenderPath() override;

    rcp<RenderPaint> makeRenderPaint() override;

    rcp<RenderImage> decodeImage(Span<const uint8_t>) override;

    std::unique_ptr<Renderer> makeRenderer();

    void addFrame();
    void frameSize(uint32_t width, uint32_t height);

    void save(const char* filename);
    bool matches(const char* filename);

private:
    void saveTarnished(const char* filename);

    std::vector<uint8_t> m_buffer;
    VectorBinaryWriter m_writer;

    uint64_t m_renderImageId = 0;
    uint64_t m_renderPaintId = 0;
    uint64_t m_renderPathId = 0;
    uint64_t m_renderBufferId = 0;
    uint64_t m_renderShaderId = 0;
};
} // namespace rive
#endif

/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "common/rand.hpp"
#include "common/write_png_file.hpp"

#ifndef RIVE_TOOLS_NO_GL
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#endif

using namespace rivegm;

constexpr static size_t kRows = 100;
constexpr static size_t kCols = 100;

// Base class for stress tests that draw huge amounts of something.
class LotsOfSquaresGM : public GM
{
public:
    LotsOfSquaresGM(const char* name) : GM(1701, 1701, name) {}

    rive::ColorInt clearColor() const override { return 0xff000000; }

    void onDraw(rive::Renderer* renderer) override
    {
        preDraw(renderer);
        renderer->translate(0, 1);
        for (size_t j = 0; j < kRows; ++j)
        {
            renderer->save();
            for (size_t i = 0; i < kCols; ++i)
            {
                drawSquare(renderer, 16, i, j);
                renderer->translate(17, 0);
            }
            renderer->restore();
            renderer->translate(0, 17);
        }
    }

    virtual void preDraw(rive::Renderer*) {}
    virtual void drawSquare(rive::Renderer*,
                            float size,
                            size_t i,
                            size_t j) = 0;

protected:
    rive::ColorInt randColor()
    {
        return static_cast<rive::ColorInt>(m_rand.u64()) | 0x80000000;
    }

    Rand m_rand;
};

// Stress-test the gradient library by drawing more color ramps than fit in the
// gradient texture.
class LotsOfGradsGM : public LotsOfSquaresGM
{
public:
    LotsOfGradsGM(const char* name) : LotsOfSquaresGM(name) {}

    void drawSquare(rive::Renderer* renderer,
                    float size,
                    size_t i,
                    size_t j) override
    {
        rive::Factory* factory = TestingWindow::Get()->factory();
        auto paint = factory->makeRenderPaint();
        paint->shader(makeGradient(factory, i, j));
        rivegm::draw_rect(renderer, {0, 0, size, size}, paint.get());
    }

    virtual rive::rcp<rive::RenderShader> makeGradient(rive::Factory*,
                                                       size_t i,
                                                       size_t j) = 0;
};

// Run out of "simple" texels in the gradient texture.
class LotsOfGradsSimpleGM : public LotsOfGradsGM
{
public:
    LotsOfGradsSimpleGM() : LotsOfGradsGM("lots_of_grads_simple") {}

    rive::rcp<rive::RenderShader> makeGradient(rive::Factory* factory,
                                               size_t i,
                                               size_t j) override
    {
        float stops[2] = {0, 1};
        rive::ColorInt colors[2] = {randColor(), randColor()};
        return factory->makeLinearGradient((i & 1) ? 16 : 0,
                                           (j & 1) ? 16 : 0,
                                           (i & 1) ? 0 : 16,
                                           (j & 1) ? 0 : 16,
                                           colors,
                                           stops,
                                           2);
    }
};
GMREGISTER(return new LotsOfGradsSimpleGM)

// Run out of "complex" texel rows in the gradient texture.
class LotsOfGradsComplexGM : public LotsOfGradsGM
{
public:
    LotsOfGradsComplexGM() : LotsOfGradsGM("lots_of_grads_complex") {}

    rive::rcp<rive::RenderShader> makeGradient(rive::Factory* factory,
                                               size_t i,
                                               size_t j) override
    {
        float stops[3] = {.05f, .147f, 1};
        rive::ColorInt colors[3] = {randColor(), randColor(), randColor()};
        return factory->makeRadialGradient((i & 1) ? 0 : 16,
                                           (j & 1) ? 2 : 22,
                                           22,
                                           colors,
                                           stops,
                                           3);
    }
};
GMREGISTER(return new LotsOfGradsComplexGM)

// Run out of "GradSpan" instances for rendering color ramps.
class LotsOfGradSpansGM : public LotsOfGradsGM
{
public:
    LotsOfGradSpansGM() : LotsOfGradsGM("lots_of_grad_spans") {}
    LotsOfGradSpansGM(const char* name) : LotsOfGradsGM(name) {}

    rive::rcp<rive::RenderShader> makeGradient(rive::Factory* factory,
                                               size_t i,
                                               size_t j) override
    {
        float stops[7] = {0, .147f, .148f, .23f, .67f, .8f, 1};
        rive::ColorInt colors[7] = {randColor(),
                                    randColor(),
                                    randColor(),
                                    randColor(),
                                    randColor(),
                                    randColor(),
                                    randColor()};
        return factory->makeLinearGradient((i & 1) ? 16 : 0,
                                           (j & 1) ? 16 : 0,
                                           (i & 1) ? 0 : 16,
                                           (j & 1) ? 0 : 16,
                                           colors,
                                           stops,
                                           7);
    }
};
GMREGISTER(return new LotsOfGradSpansGM)

// Ensure that an intermediate flush does not invalidate the clip buffer
// contents.
class LotsOfGradsClippedGM : public LotsOfGradSpansGM
{
public:
    LotsOfGradsClippedGM() : LotsOfGradSpansGM("lots_of_grads_clipped") {}

    void preDraw(rive::Renderer* renderer) override
    {
        Path circle;
        path_addOval(circle, rive::AABB(-1700, -1700, 3400, 3400));
        renderer->clipPath(circle);
    }
};
GMREGISTER(return new LotsOfGradsClippedGM)

// Run out of complex and simple gradients both.
class LotsOfGradsMixedGM : public LotsOfGradsGM
{
public:
    LotsOfGradsMixedGM() : LotsOfGradsGM("lots_of_grads_mixed") {}

    rive::rcp<rive::RenderShader> makeGradient(rive::Factory* factory,
                                               size_t i,
                                               size_t j) override
    {
        if (m_rand.boolean())
        {
            float stops[3] = {.05f, .147f, 1};
            rive::ColorInt colors[3] = {randColor(), randColor(), randColor()};
            return factory->makeRadialGradient((i & 1) ? 0 : 16,
                                               (j & 1) ? 2 : 22,
                                               22,
                                               colors,
                                               stops,
                                               3);
        }
        else
        {
            float stops[2] = {0, 1};
            rive::ColorInt colors[2] = {randColor(), randColor()};
            return factory->makeLinearGradient((i & 1) ? 16 : 0,
                                               (j & 1) ? 16 : 0,
                                               (i & 1) ? 0 : 16,
                                               (j & 1) ? 0 : 16,
                                               colors,
                                               stops,
                                               2);
        }
    }
};
GMREGISTER(return new LotsOfGradsMixedGM)

// Run out of VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE descriptors, requiring the Vulkan
// backend to allocate a new VkDescriptorPool during flush.
class LotsOfImagesGM : public LotsOfSquaresGM
{
public:
    LotsOfImagesGM() : LotsOfSquaresGM("lots_of_images") {}

    void drawSquare(rive::Renderer* renderer,
                    float size,
                    size_t i,
                    size_t j) override
    {
        size_t imageCount = std::size(m_images);
#ifndef RIVE_TOOLS_NO_GL
        if (auto* plsGLImpl = TestingWindow::Get()->renderContextGLImpl())
        {
            if (plsGLImpl->capabilities().isANGLEOrWebGL)
            {
                // This test takes too long on ANGLE with all the images. Draw
                // fewer images instead.
                imageCount = 16;
            }
        }
#endif
        auto& image = m_images[(j * kCols + i) % imageCount];
        if (image == nullptr)
        {
            constexpr static size_t pngSize = 4;
            rive::Factory* factory = TestingWindow::Get()->factory();
            auto color = randColor();
            std::vector<uint8_t> imagePixels(pngSize * pngSize * 4);
            for (auto it = imagePixels.begin(); it != imagePixels.end();
                 it += 4)
            {
                rive::UnpackColorToRGBA8(color, &(*it));
            }
            std::vector<uint8_t> encodedData =
                EncodePNGToBuffer(pngSize,
                                  pngSize,
                                  imagePixels.data(),
                                  PNGCompression::fast_rle);
            image = factory->decodeImage(rive::Span<uint8_t>(encodedData));
        }

        AutoRestore ar(renderer, /*doSave=*/true);
        renderer->scale(size / image->width(), size / image->height());
        renderer->drawImage(image.get(), rive::BlendMode::srcOver, 1);
    }

private:
    rive::rcp<rive::RenderImage> m_images[512];
};
GMREGISTER(return new LotsOfImagesGM)

/*
 * Copyright 2022 Rive
 */
#include "testing_window.hpp"

#if !defined(RIVE_SVG) || !defined(__EMSCRIPTEN__)

TestingWindow* TestingWindow::MakeSVG() { return nullptr; }

#else

#include "rive/decoders/bitmap_decoder.hpp"
#include "utils/svg_factory.hpp"
#include "utils/svg_renderer.hpp"

#include <limits>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

namespace rive::gpu
{
EM_JS(void, testingWindowSVGResetCanvas, (), {
    var canvas = document["getElementById"]("canvas");
    var ctx = canvas["getContext"]("2d");
    ctx["reset"]();
});

EM_JS(bool,
      testingWindowSVGGetCanvasPixels,
      (uint8_t* outBuffer, int width, int height),
      {
          var canvas = document["getElementById"]("canvas");
          var ctx = canvas["getContext"]("2d");
          var imageData =
              ctx["getImageData"](0, 0, canvas.width, canvas.height);
          var pixelArray = imageData["data"];
          if (pixelArray.length != width * height * 4)
          {
              return false;
          }

          // TestHarness::savePNG() flips vertically on the way out, since every
          // other backend fills this buffer bottom-up (the GL convention).
          // getImageData() hands back rows top-down, so reverse them here and
          // let the two cancel.
          var stride = width * 4;
          for (var y = 0; y < height; ++y)
          {
              Module["HEAPU8"]["set"](
                  pixelArray["subarray"]((height - 1 - y) * stride,
                                         (height - y) * stride),
                  outBuffer + y * stride);
          }
          return true;
      });

// Rasterizing SVG means handing it to an <img> and blitting that; the browser
// offers no synchronous path. EM_ASYNC_JS relies on the ASYNCIFY that the wasm
// test targets already link with.
EM_ASYNC_JS(bool,
            testingWindowSVGDrawSVGToCanvas,
            (const char* svg, int width, int height),
            {
                // clang-format off
    var url = URL["createObjectURL"](new Blob([UTF8ToString(svg)], {
        type: "image/svg+xml"
    }));
    try
    {
        var img = new Image();
        img["src"] = url;
        await img["decode"]();
        var ctx = document["getElementById"]("canvas")["getContext"]("2d");
        ctx["drawImage"](img, 0, 0, width, height);
        return true;
    }
    catch (e)
    {
        console["error"](e);
        return false;
    }
    finally
    {
        URL["revokeObjectURL"](url);
    }
                // clang-format on
            });

// This wrapper exists so that we can return a std::unique_ptr<Renderer> from
// beginFrame() while still holding a pointer to the returned object for use in
// endFrame().
class SVGRendererWrapper : public rive::Renderer
{
public:
    SVGRendererWrapper(std::shared_ptr<rive::SVGRenderer>& renderer) :
        m_renderer(renderer)
    {}

    void save() override { m_renderer->save(); }
    void restore() override { m_renderer->restore(); }
    void transform(const Mat2D& transform) override
    {
        m_renderer->transform(transform);
    }
    void drawPath(RenderPath* path, RenderPaint* paint) override
    {
        m_renderer->drawPath(path, paint);
    }
    void clipPath(RenderPath* path) override { m_renderer->clipPath(path); }
    void drawImage(const RenderImage* image,
                   ImageSampler sampler,
                   BlendMode blendMode,
                   float opacity) override
    {
        m_renderer->drawImage(image, sampler, blendMode, opacity);
    }
    void drawImageMesh(const RenderImage* image,
                       ImageSampler sampler,
                       rcp<RenderBuffer> vertices_f32,
                       rcp<RenderBuffer> uvCoords_f32,
                       rcp<RenderBuffer> indices_u16,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode blendMode,
                       float opacity) override
    {
        m_renderer->drawImageMesh(image,
                                  sampler,
                                  vertices_f32,
                                  uvCoords_f32,
                                  indices_u16,
                                  vertexCount,
                                  indexCount,
                                  blendMode,
                                  opacity);
    }
    void modulateOpacity(float opacity) override
    {
        m_renderer->modulateOpacity(opacity);
    }

private:
    std::shared_ptr<rive::SVGRenderer> m_renderer;
};

// SVGFactory embeds encoded images verbatim as data URIs, so it only needs
// their dimensions. rive_decoders has no header-only sizing entry point, so
// pay for a full decode and throw the pixels away (fine since these are just
// tests).
static bool decodeImageSize(rive::Span<const uint8_t> encodedBytes,
                            uint32_t* width,
                            uint32_t* height)
{
    auto bitmap = Bitmap::decode(encodedBytes.data(), encodedBytes.size());
    if (bitmap == nullptr)
    {
        return false;
    }
    *width = bitmap->width();
    *height = bitmap->height();
    return true;
}

class TestingWindowSVG : public TestingWindow
{
public:
    TestingWindowSVG()
    {
        int w, h;
        emscripten_get_canvas_element_size("#canvas", &w, &h);
        m_width = w;
        m_height = h;

        m_factory = std::make_unique<SVGFactory>(decodeImageSize);
    }

    rive::Factory* factory() override { return m_factory.get(); }

    void resize(int width, int height) override
    {
        if (m_width != (uint32_t)width || m_height != (uint32_t)height)
        {
            TestingWindow::resize(width, height);
            emscripten_set_canvas_element_size("#canvas", width, height);
        }
    }

    std::unique_ptr<rive::Renderer> beginFrame(
        const FrameOptions& options) override
    {
        // Because the SVGRenderer is a stateful object that accumulates all
        // draws, we treat doCleaer as a "reset" flag. If doClear is true, we
        // reset the renderer to start fresh; if false, we keep the existing
        // state and continue drawing on top of it.

        // For the above reason, we always reset the canvas at the start of a
        // frame; endFrame will include previous frames' contents if doClear was
        // not set.
        testingWindowSVGResetCanvas();

        if (options.doClear)
        {
            m_renderer.reset();
        }

        if (!m_renderer)
        {
            // Goldens compare pixels, so emit enough digits to round-trip
            // a float rather than the renderer's compact default.
            m_renderer = std::make_shared<SVGRenderer>(
                std::numeric_limits<float>::max_digits10);
        }

        if (options.doClear)
        {
            RawPath rawPath;
            rawPath.addRect(AABB(0, 0, (float)m_width, (float)m_height));
            auto path = m_factory->makeRenderPath(rawPath, FillRule::nonZero);
            auto paint = m_factory->makeRenderPaint();
            paint->style(RenderPaintStyle::fill);
            paint->color(options.clearColor);
            m_renderer->drawPath(path.get(), paint.get());
        }

        return std::make_unique<SVGRendererWrapper>(m_renderer);
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        std::string svg = m_renderer->finalize(m_width, m_height);
        if (!testingWindowSVGDrawSVGToCanvas(svg.c_str(), m_width, m_height))
        {
            printf("Failed to rasterize SVG\n");
        }

        if (!pixelData)
        {
            return;
        }

        pixelData->resize(m_width * m_height * 4);
        if (!testingWindowSVGGetCanvasPixels(pixelData->data(),
                                             m_width,
                                             m_height))
        {
            printf("Canvas size mismatch, read failed\n");
            pixelData->assign(pixelData->size(), 0);
        }

        m_renderer.reset();
    }

private:
    std::unique_ptr<SVGFactory> m_factory;
    std::shared_ptr<SVGRenderer> m_renderer;
};
}; // namespace rive::gpu

TestingWindow* TestingWindow::MakeSVG()
{
    return new rive::gpu::TestingWindowSVG();
}

#endif

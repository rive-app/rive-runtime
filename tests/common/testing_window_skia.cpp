/*
 * Copyright 2022 Rive
 */
#include "testing_window.hpp"

#if defined(TESTING) || !defined(RIVE_SKIA)

TestingWindow* TestingWindow::MakeSkia() { return nullptr; }

#else

#include "skia_factory.hpp"
#include "skia_renderer.hpp"
#include "skia/include/core/SkCanvas.h"
#include "skia/include/core/SkSurface.h"
#include "skia/include/gpu/GrDirectContext.h"
#include "skia/include/gpu/gl/GrGLAssembleInterface.h"

TestingWindow* TestingWindow::MakeSkia()
{
    class TestingWindowSkiaRaster : public TestingWindow
    {
    public:
        rive::Factory* factory() override { return &m_factory; }

        void resize(int width, int height) override
        {
            auto info = SkImageInfo::MakeN32Premul(width, height);
            m_surface = SkSurface::MakeRaster(info);
        }

        std::unique_ptr<rive::Renderer> beginFrame(
            const FrameOptions& options) override
        {
            if (m_surface)
            {
                SkCanvas* canvas = m_surface->getCanvas();
                if (options.doClear)
                {
                    canvas->clear(options.clearColor);
                }
                return std::make_unique<rive::SkiaRenderer>(canvas);
            }
            return nullptr;
        }

        void endFrame(std::vector<uint8_t>* pixelData) override
        {
            if (!m_surface)
            {
                return;
            }
            m_surface->getCanvas()->flush();
            if (pixelData)
            {
                auto img = m_surface->makeImageSnapshot();
                int w = img->width();
                int h = img->height();
                pixelData->resize(h * w * 4);
                SkColorInfo colorInfo(kRGBA_8888_SkColorType,
                                      kPremul_SkAlphaType,
                                      nullptr);
                // Read the canvas back upside down to match GL's orientation.
                for (int y = 0; y < h; ++y)
                {
                    img->readPixels(
                        nullptr,
                        SkPixmap(SkImageInfo::Make({w, 1}, colorInfo),
                                 pixelData->data() + y * w * 4,
                                 img->width() * 4),
                        0,
                        h - y - 1);
                }
            }
        }

    private:
        rive::SkiaFactory m_factory;
        sk_sp<SkSurface> m_surface;
    };

    return new TestingWindowSkiaRaster();
}

#endif

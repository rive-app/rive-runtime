/*
 * Copyright 2022 Rive
 */

#include "testing_window.hpp"
#include "rive/rive_types.hpp"

#if !defined(RIVE_MACOSX) || defined(RIVE_UNREAL)

TestingWindow* TestingWindow::MakeCoreGraphics() { return nullptr; }

#else

#include "cg_factory.hpp"
#include "cg_renderer.hpp"
#include "mac_utils.hpp"

#include <algorithm>
#include <cstring>

class TestingWindowCoreGraphics : public TestingWindow
{
public:
    TestingWindowCoreGraphics()
    {
        m_space.reset(CGColorSpaceCreateDeviceRGB());
    }

    rive::Factory* factory() override { return &m_factory; }

    void resize(int w, int h) override
    {
        m_pixels.resize(w * h);

        // They must be static casted individually otherwise the compiler
        // complains that we are using bitwise operations on different enum
        // types
        auto info = static_cast<uint32_t>(kCGBitmapByteOrder32Big) |
                    static_cast<uint32_t>(kCGImageAlphaPremultipliedLast);
        m_ctx.reset(CGBitmapContextCreate(m_pixels.data(),
                                          w,
                                          h,
                                          8,
                                          w * 4,
                                          m_space,
                                          info));
        m_width = w;
        m_height = h;
    }

    std::unique_ptr<rive::Renderer> beginFrame(
        const FrameOptions& options) override
    {
        CGContextFlush(m_ctx);
        if (options.doClear)
        {
            std::fill(m_pixels.begin(), m_pixels.end(), options.clearColor);
        }
        return std::make_unique<rive::CGRenderer>(m_ctx, m_width, m_height);
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        CGContextFlush(m_ctx);
        if (pixelData)
        {
            const auto size = m_pixels.size() * sizeof(uint32_t);
            pixelData->resize(size);
            // copy scanlines backwards to match GL's convention
            const auto w = CGBitmapContextGetWidth(m_ctx);
            const auto h = CGBitmapContextGetHeight(m_ctx);
            const uint32_t* src = m_pixels.data() + m_pixels.size();
            uint8_t* dst = pixelData->data();
            for (size_t y = 0; y < h; ++y)
            {
                src -= w;
                memcpy(dst, src, w * sizeof(uint32_t));
                dst += w * sizeof(uint32_t);
            }
            assert(src == m_pixels.data());
            assert(dst == pixelData->data() + pixelData->size());
        }
    }

private:
    rive::CGFactory m_factory;
    std::vector<uint32_t> m_pixels;
    AutoCF<CGContextRef> m_ctx;
    AutoCF<CGColorSpaceRef> m_space;
};

TestingWindow* TestingWindow::MakeCoreGraphics()
{
    return new TestingWindowCoreGraphics;
}

#endif

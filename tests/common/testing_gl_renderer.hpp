/*
 * Copyright 2022 Rive
 */

#ifndef RENDER_CONTEXT_HPP
#define RENDER_CONTEXT_HPP

#include "testing_window.hpp"
#include <memory>

namespace rive
{
class Renderer;
class Factory;
}; // namespace rive

// Provides a rive::Factory and rive::Renderer, with a few extra testing and
// GL-specific functions.
class TestingGLRenderer
{
public:
    static std::unique_ptr<TestingGLRenderer> Make(
        const TestingWindow::BackendParams&);

    virtual void init(void* getGLProcAddress) = 0;
    virtual rive::Factory* factory() = 0;
    virtual std::unique_ptr<rive::Renderer> reset(
        int width,
        int height,
        uint32_t targetTextureID = 0) = 0;
    virtual void beginFrame(const TestingWindow::FrameOptions&) = 0;
    virtual void flush(int dpiScale = 1) = 0;

    // For testing directly on RenderContext.
    virtual rive::gpu::RenderContext* renderContext() const { return nullptr; }
    virtual rive::gpu::RenderTarget* renderTarget() const { return nullptr; }

    // For testing render pass breaks. Caller must call
    // renderContext()->beginFrame() again.
    virtual void flushPLSContext(rive::gpu::RenderTarget* offscreenRenderTarget)
    {}

    virtual ~TestingGLRenderer();
};

#endif

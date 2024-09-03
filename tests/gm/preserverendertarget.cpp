/*
 * Copyright 2023 Rive
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "common/testing_window.hpp"
#include "rive/renderer/render_context.hpp"

using namespace rivegm;
using namespace rive;

// Ensures the PLS render target does not get cleared when using LoadAction::preserveRenderTarget.
class PreserveRenderTargetBase : public GM
{
protected:
    PreserveRenderTargetBase(const char* name, bool empty, BlendMode blendMode) :
        GM(64, 64, name), m_empty(empty), m_blendMode(blendMode)
    {}

    void run(std::vector<uint8_t>* pixels) override
    {
        Paint yellow;
        yellow->color(0xffffff00);

        // Set the render target to a cyan background with a yellow circle.
        auto renderer = TestingWindow::Get()->beginFrame(0xff008080);
        renderer->drawPath(PathBuilder::Circle(32, 32, 20), yellow);

        // Don't clear to red!
        if (auto* renderContext = TestingWindow::Get()->renderContext())
        {
            rive::gpu::RenderContext::FrameDescriptor frameDescriptor =
                renderContext->frameDescriptor();
            frameDescriptor.loadAction = rive::gpu::LoadAction::preserveRenderTarget;
            frameDescriptor.clearColor = 0xffff0000;

            TestingWindow::Get()->flushPLSContext();
            renderContext->beginFrame(std::move(frameDescriptor));
        }
        else
        {
            TestingWindow::Get()->endFrame();
            renderer = TestingWindow::Get()->beginFrame(0xffff0000, false /*doClear*/);
        }

        if (!m_empty)
        {
            // Add a black circle on top of the preserved yellow one.
            Paint black;
            black->color(0xff000000);
            black->blendMode(m_blendMode);
            renderer->drawPath(PathBuilder::Circle(32, 32, 8), black);
        }

        TestingWindow::Get()->endFrame(pixels);
    }

    void onDraw(rive::Renderer*) override { RIVE_UNREACHABLE(); }

    const bool m_empty;
    const BlendMode m_blendMode;
};

class PreserveRenderTargetGM : public PreserveRenderTargetBase
{
public:
    PreserveRenderTargetGM() :
        PreserveRenderTargetBase("preserverendertarget", false, BlendMode::srcOver)
    {}
};
GMREGISTER(return new PreserveRenderTargetGM)

class PreserveRenderTargetBlendModeGM : public PreserveRenderTargetBase
{
public:
    PreserveRenderTargetBlendModeGM() :
        PreserveRenderTargetBase("preserverendertarget_blendmode", false, BlendMode::darken)
    {}
};
GMREGISTER(return new PreserveRenderTargetBlendModeGM)

class PreserveRenderTargetEmptyGM : public PreserveRenderTargetBase
{
public:
    PreserveRenderTargetEmptyGM() :
        PreserveRenderTargetBase("preserverendertarget_empty", true, BlendMode::srcOver)
    {}
};
GMREGISTER(return new PreserveRenderTargetEmptyGM)

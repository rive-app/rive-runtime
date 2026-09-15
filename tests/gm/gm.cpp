/*
 * Copyright 2022 Rive
 */

#include "gm.hpp"

#include "common/testing_window.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_context_impl.hpp"

using namespace rivegm;

void GM::run(const char* name, std::vector<uint8_t>* pixels)
{
    TestingWindow::FrameOptions frameOptions = {
        .name = name,
        .clearColor = clearColor(),
        .triangulationThresholds = DeterministicTriangulationThresholds,
    };
    updateFrameOptions(&frameOptions);

    rive::gpu::RenderContext* renderContext = nullptr;
    auto previousMode = rive::gpu::ShaderCompilationMode::standard;
    if (frameOptions.shaderCompilationMode !=
        rive::gpu::ShaderCompilationMode::standard)
    {
        renderContext = TestingWindow::Get()->renderContext();
        if (renderContext != nullptr)
        {
            previousMode =
                renderContext->impl()->testingOnly_setShaderCompilationMode(
                    rive::gpu::ShaderCompilationMode::onlyUbershaders);
        }
    }

    auto renderer = TestingWindow::Get()->beginFrame(frameOptions);
    draw(renderer.get());
    TestingWindow::Get()->endFrame(pixels);

    if ((frameOptions.shaderCompilationMode !=
         rive::gpu::ShaderCompilationMode::standard) &&
        renderContext != nullptr)
    {
        renderContext->impl()->testingOnly_setShaderCompilationMode(
            previousMode);
    }
}

void GM::draw(rive::Renderer* renderer)
{
    renderer->save();
    this->onDraw(renderer);
    renderer->restore();
}

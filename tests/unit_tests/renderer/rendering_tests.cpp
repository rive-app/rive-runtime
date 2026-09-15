/*
 * Copyright 2025 Rive
 */

#include "common/testing_window.hpp"
#include "rive/renderer.hpp"
#include "rive/factory.hpp"

#include <catch.hpp>

using namespace rive;
using namespace rive::gpu;

// Factories to manually instantiate real rendering contexts, for unit testing
// the full pipeline.
struct FactoryWrapper
{
    const char* displayName;
    std::function<std::unique_ptr<TestingWindow>()> function;
};
static FactoryWrapper testingWindowFactories[] = {
    {"Vulkan",
     []() {
         return std::unique_ptr<TestingWindow>(
             TestingWindow::MakeVulkanTexture({
#ifdef RIVE_ANDROID
                 // Android doesn't support validation layers for command line
                 // apps like the unit_tests.
                 .disableValidationLayers = true,
                 // The OnePlus7 doesn't support debug callbacks either for
                 // command line apps.
                 .disableDebugCallbacks = true,
#endif
             }));
     }},
#if defined(__APPLE__)
    {"Metal",
     []() {
         return std::unique_ptr<TestingWindow>(
             TestingWindow::MakeMetalTexture({}));
     }},
#endif
#ifdef _WIN32
    {"D3D12",
     []() {
         return std::unique_ptr<TestingWindow>(TestingWindow::MakeFiddleContext(
             TestingWindow::Backend::d3d12,
             {},
             TestingWindow::Visibility::headless,
             nullptr));
     }},
    {"D3D12 atomic",
     []() {
         return std::unique_ptr<TestingWindow>(TestingWindow::MakeFiddleContext(
             TestingWindow::Backend::d3d12,
             {.atomic = true},
             TestingWindow::Visibility::headless,
             nullptr));
     }},
    {"D3D11",
     []() {
         return std::unique_ptr<TestingWindow>(TestingWindow::MakeFiddleContext(
             TestingWindow::Backend::d3d,
             {},
             TestingWindow::Visibility::headless,
             nullptr));
     }},
    {"D3D11 atomic",
     []() {
         return std::unique_ptr<TestingWindow>(TestingWindow::MakeFiddleContext(
             TestingWindow::Backend::d3d,
             {.atomic = true},
             TestingWindow::Visibility::headless,
             nullptr));
     }},
    {"OpenGL",
     []() {
         return std::unique_ptr<TestingWindow>(TestingWindow::MakeFiddleContext(
             TestingWindow::Backend::gl,
             {},
             TestingWindow::Visibility::headless,
             nullptr));
     }},
    {"OpenGL atomic",
     []() {
         return std::unique_ptr<TestingWindow>(TestingWindow::MakeFiddleContext(
             TestingWindow::Backend::gl,
             {.atomic = true},
             TestingWindow::Visibility::headless,
             nullptr));
     }},
#endif
#ifdef RIVE_ANDROID
    {"EGL (GL backend)",
     []() {
         return std::unique_ptr<TestingWindow>(
             TestingWindow::MakeEGL(TestingWindow::Backend::gl, {}, nullptr));
     }},
#endif
};

// Ensure that rendering still succeeds when compilations fail (e.g., by falling
// back on an uber shader or at least not crashing). Valid compilations may fail
// in the real world if the device is pressed for resources or in a bad state.
TEST_CASE("synthesizedFailureType", "[rendering]")
{
    // TODO: There are potentially stronger ways to build some of these
    // synthesized failures if we were to pass SynthesizedFailureType as a
    // creation option instead of on beginFrame
    for (auto failureType : {SynthesizedFailureType::shaderCompilation,
                             SynthesizedFailureType::ubershaderLoad,
                             SynthesizedFailureType::pipelineCreation})
    {
        switch (failureType)
        {
            case SynthesizedFailureType::shaderCompilation:
                printf("testing synthesied shader compilation failure\n");
                break;
            case SynthesizedFailureType::ubershaderLoad:
                printf("testing synthesized ubershader load failure\n");
                break;
            case SynthesizedFailureType::pipelineCreation:
                printf("testing synthesized pipeline creation failure\n");
                break;
            case SynthesizedFailureType::none:
                // android compiler complains (rightly) if this case isn't here.
                RIVE_UNREACHABLE();
        }
        for (auto& testingWindowFactory : testingWindowFactories)
        {
            printf("  testing with '%s' factory\n",
                   testingWindowFactory.displayName);
            std::unique_ptr<TestingWindow> window =
                testingWindowFactory.function();
            if (window == nullptr)
            {
                continue;
            }
            Factory* factory = window->factory();

            window->resize(32, 32);

            // Expected colors after we draw a cyan rectangle.
            std::vector<uint8_t> drawColors;
            drawColors.reserve(32 * 32 * 4);
            for (size_t i = 0; i < 32 * 32; ++i)
                drawColors.insert(drawColors.end(), {0x00, 0xff, 0xff, 0xff});

            // Expected colors when only the clear happens (because even the
            // uber shader failed to compile).
            std::vector<uint8_t> clearColors;
            clearColors.reserve(32 * 32 * 4);
            for (size_t i = 0; i < 32 * 32; ++i)
                clearColors.insert(clearColors.end(), {0xff, 0x00, 0x00, 0xff});

            for (bool disableRasterOrdering : {false, true})
            {
                auto renderer = window->beginFrame({
                    .clearColor = 0xffff0000,
                    .doClear = true,
                    .disableRasterOrdering = disableRasterOrdering,
                    .synthesizedFailureType = failureType,
                });

                rcp<RenderPath> path =
                    factory->makeRenderPath(AABB{0, 0, 32, 32});
                rcp<RenderPaint> paint = factory->makeRenderPaint();
                paint->color(0xff00ffff);
                renderer->drawPath(path.get(), paint.get());

                std::vector<uint8_t> pixels;
                window->endFrame(&pixels);

                // There are two acceptable results to this test:
                //
                // 1) The draw happens anyway because we fell back on a
                //    precompiled uber shader.
                //
                // 2) The uber shader also synthesizes a compilation faiulre, so
                //    only the clear color makes it through.
                if (pixels != drawColors && pixels != clearColors)
                {
                    printf("Expected {%02x, %02x, %02x, %02x} or {%02x, %02x, "
                           "%02x, %02x}, got {%02x, %02x, %02x, %02x}",
                           drawColors[0],
                           drawColors[1],
                           drawColors[2],
                           drawColors[3],
                           clearColors[0],
                           clearColors[1],
                           clearColors[2],
                           clearColors[3],
                           pixels[0],
                           pixels[1],
                           pixels[2],
                           pixels[3]);
                }
                CHECK((pixels == drawColors || pixels == clearColors));
            }
        }
    }
}

#if !defined(RIVE_TOOLS_NO_GL) && (defined(_WIN32) || defined(RIVE_ANDROID))
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"

// A headless GL window, or null if this device has no GL driver.
static std::unique_ptr<TestingWindow> makeGLTestingWindow()
{
#ifdef RIVE_ANDROID
    // Android has no GLFW, so it goes through EGL instead.
    return std::unique_ptr<TestingWindow>(
        TestingWindow::MakeEGL(TestingWindow::Backend::gl, {}, nullptr));
#else
    return std::unique_ptr<TestingWindow>(
        TestingWindow::MakeFiddleContext(TestingWindow::Backend::gl,
                                         {},
                                         TestingWindow::Visibility::headless,
                                         nullptr));
#endif
}

// TextureRenderTargetGL caches the framebuffer attachment for its target
// texture. setTargetTexture() has to invalidate that cache for the depthStencil
// framebuffer as well as the plain one -- otherwise a render target that is
// reused with a new texture, at an unchanged sample count, keeps drawing into
// the texture it was last attached to.
TEST_CASE("TextureRenderTargetGL_retargetsDepthStencilFramebuffer",
          "[rendering]")
{
    constexpr static uint32_t Width = 32, Height = 32;
    // 0xAARRGGBB.
    constexpr static ColorInt Red = 0xffff0000;
    constexpr static ColorInt Cyan = 0xff00ffff;

    std::unique_ptr<TestingWindow> window = makeGLTestingWindow();
    if (window == nullptr)
    {
        return;
    }
    RenderContext* renderContext = window->renderContext();
    REQUIRE(renderContext != nullptr);
    Factory* factory = window->factory();

    // Two interchangeable target textures, each seeded with a sentinel so an
    // untouched texture is distinguishable from a correctly drawn one.
    GLuint textures[2];
    glGenTextures(2, textures);
    for (GLuint tex : textures)
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, Width, Height);
    }

    GLuint readbackFBO;
    glGenFramebuffers(1, &readbackFBO);
    auto readback = [&](GLuint tex) {
        std::vector<uint8_t> pixels(Width * Height * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, readbackFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               tex,
                               0);
        glReadPixels(0,
                     0,
                     Width,
                     Height,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     pixels.data());
        return pixels;
    };
    auto solidColor = [](ColorInt color) {
        std::vector<uint8_t> pixels;
        pixels.reserve(Width * Height * 4);
        for (size_t i = 0; i < Width * Height; ++i)
        {
            pixels.insert(pixels.end(),
                          {static_cast<uint8_t>(colorRed(color)),
                           static_cast<uint8_t>(colorGreen(color)),
                           static_cast<uint8_t>(colorBlue(color)),
                           static_cast<uint8_t>(colorAlpha(color))});
        }
        return pixels;
    };

    // One render target, reused across every sample count and both textures.
    auto renderTarget = make_rcp<TextureRenderTargetGL>(Width, Height);

    // 4 -> 1 exercises the sample count changing, and drawing twice at the same
    // count exercises the case the cache invalidation exists for.
    for (uint32_t msaaSampleCount : {4u, 1u, 4u})
    {
        for (int i = 0; i < 2; ++i)
        {
            const ColorInt color = (i == 0) ? Red : Cyan;
            renderTarget->setTargetTexture(textures[i]);
            renderContext->beginFrame({
                .renderTargetWidth = Width,
                .renderTargetHeight = Height,
                .loadAction = gpu::LoadAction::clear,
                .clearColor = color,
                .msaaSampleCount = msaaSampleCount,
                .clockwiseFillOverride = true,
            });
            RiveRenderer renderer(renderContext);
            rcp<RenderPath> path =
                factory->makeRenderPath(AABB{0, 0, Width, Height});
            rcp<RenderPaint> paint = factory->makeRenderPaint();
            paint->color(color);
            renderer.drawPath(path.get(), paint.get());
            renderContext->flush({.renderTarget = renderTarget.get()});
        }

        // The second draw must have landed in textures[1], and must not have
        // clobbered textures[0].
        INFO("msaaSampleCount " << msaaSampleCount);
        CHECK(readback(textures[1]) == solidColor(Cyan));
        CHECK(readback(textures[0]) == solidColor(Red));
    }

    glDeleteFramebuffers(1, &readbackFBO);
    glDeleteTextures(2, textures);
}
#endif

// namespace rive::gpu

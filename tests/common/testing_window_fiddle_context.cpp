/*
 * Copyright 2022 Rive
 */

#include "testing_window.hpp"

#if defined(RIVE_TOOLS_NO_GLFW)
TestingWindow* TestingWindow::MakeFiddleContext(Backend,
                                                const BackendParams&,
                                                Visibility,
                                                void* platformWindow)
{
    return nullptr;
}

#else

#include "fiddle_context.hpp"
#include "common/offscreen_render_target.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include <queue>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using namespace rive;
using namespace rive::gpu;

static std::queue<TestingWindow::InputEventData> inputEvents;
static float dpi = 1.0f;

static void mouse_position_callback(GLFWwindow* window,
                                    double xPos,
                                    double yPos)
{
    inputEvents.push({TestingWindow::InputEvent::MouseMove,
                      static_cast<float>(xPos) * dpi,
                      static_cast<float>(yPos) * dpi});
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    assert(0 <= button);
    assert(button <= GLFW_MOUSE_BUTTON_LAST);

    double xPos, yPos;
    glfwGetCursorPos(window, &xPos, &yPos);

    switch (action)
    {
        case GLFW_PRESS:
            inputEvents.push({TestingWindow::InputEvent::MouseDown,
                              static_cast<float>(xPos) * dpi,
                              static_cast<float>(yPos) * dpi});
            break;
        case GLFW_RELEASE:
            inputEvents.push({TestingWindow::InputEvent::MouseUp,
                              static_cast<float>(xPos) * dpi,
                              static_cast<float>(yPos) * dpi});
            break;
        default:
            printf("Unsupported GLFW mouse button event received!\n");
            break;
    }
}

static void key_callback(GLFWwindow* window,
                         int key,
                         int scancode,
                         int action,
                         int mods)
{
    if (action == GLFW_PRESS)
    {
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        {
            if (!(mods & GLFW_MOD_SHIFT))
            {
                key += 'a' - 'A';
            }
        }
        else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        {
            if (mods & GLFW_MOD_SHIFT)
            {
                switch (key)
                {
                        // clang-format off
                    case GLFW_KEY_1: key = '!'; break;
                    case GLFW_KEY_2: key = '@'; break;
                    case GLFW_KEY_3: key = '#'; break;
                    case GLFW_KEY_4: key = '$'; break;
                    case GLFW_KEY_5: key = '%'; break;
                    case GLFW_KEY_6: key = '^'; break;
                    case GLFW_KEY_7: key = '&'; break;
                    case GLFW_KEY_8: key = '*'; break;
                    case GLFW_KEY_9: key = '('; break;
                    case GLFW_KEY_0: key = ')'; break;
                        // clang-format on
                }
            }
        }
        if (key < 128)
        {
            inputEvents.push(
                {TestingWindow::InputEvent::KeyPress, static_cast<char>(key)});
            return;
        }
        switch (key)
        {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            case GLFW_KEY_TAB:
                inputEvents.push({TestingWindow::InputEvent::KeyPress, '\t'});
                break;
            case GLFW_KEY_BACKSPACE:
                inputEvents.push({TestingWindow::InputEvent::KeyPress, '\b'});
                break;
        }
    }
}

class TestingWindowFiddleContext : public TestingWindow
{
public:
    TestingWindowFiddleContext(Backend backend,
                               const BackendParams& backendParams,
                               Visibility visibility,
                               void* platformWindow) :
        m_backendParams(backendParams)
    {
        // Vulkan headless rendering doesn't need an OS window.
        // It's convenient to run Swiftshader on CI without GLFW.
        if (backend == Backend::angle)
        {
#if !defined(__EMSCRIPTEN__)
            switch (backendParams.angleRenderer)
            {
                case ANGLERenderer::metal:
                    glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE,
                                 GLFW_ANGLE_PLATFORM_TYPE_METAL);
                    break;
                case ANGLERenderer::d3d11:
                    glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE,
                                 GLFW_ANGLE_PLATFORM_TYPE_D3D11);
                    break;
                case ANGLERenderer::vk:
                    glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE,
                                 GLFW_ANGLE_PLATFORM_TYPE_VULKAN);
                    break;
                case ANGLERenderer::gles:
                    glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE,
                                 GLFW_ANGLE_PLATFORM_TYPE_OPENGLES);
                    break;
                case ANGLERenderer::gl:
                    glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE,
                                 GLFW_ANGLE_PLATFORM_TYPE_OPENGL);
                    break;
            }
#endif
        }
        if (!glfwInit())
        {
            fprintf(stderr, "Failed to initialize GLFW.\n");
            abort();
        }

        if (backend != Backend::gl && backend != Backend::angle)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_SAMPLES, 0);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        }
        else if (backend == Backend::angle)
        {
            glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        }
        else
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        }
        if (backendParams.msaa)
        {
            m_msaaSampleCount = 4;
            glfwWindowHint(GLFW_SAMPLES, m_msaaSampleCount);
            glfwWindowHint(GLFW_STENCIL_BITS, 8);
            glfwWindowHint(GLFW_DEPTH_BITS, 24);
        }

        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        if (visibility == Visibility::headless)
        {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        }

        GLFWmonitor* fullscreenMonitor = nullptr;
        if (visibility == Visibility::fullscreen)
        {
            glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_FALSE);
            glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

            // For GLFW, the platformWindow is a fullscreen monitor index.
            intptr_t fullscreenMonitorIdx =
                reinterpret_cast<intptr_t>(platformWindow);

            int monitorCount;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            if (fullscreenMonitorIdx < 0 ||
                fullscreenMonitorIdx >= monitorCount)
            {
                fprintf(stderr,
                        "Monitor index out of range. Requested %zi but %i are "
                        "available.",
                        fullscreenMonitorIdx,
                        monitorCount);
                abort();
            }
            fullscreenMonitor = monitors[fullscreenMonitorIdx];
        }

        // GLFW doesn't appear to have a way to say "make the window as big as
        // the screen", but if we don't make it large enough here, the
        // fullscreen window will be quarter resultion instead of full. NOTE:
        // glfwGetMonitorPhysicalSize() returns bogus values.
        m_glfwWindow = glfwCreateWindow(3840,
                                        2160,
                                        "Rive Renderer",
                                        fullscreenMonitor,
                                        nullptr);
        if (!m_glfwWindow)
        {
#ifndef __EMSCRIPTEN__
            const char* errorDescription;
            int errorCode = glfwGetError(&errorDescription);
            fprintf(stderr,
                    "Failed to create GLFW window: %s\n",
                    errorDescription);
            if (errorCode == GLFW_API_UNAVAILABLE)
            {
                // This means that the driver does not support the given API and
                // we cannot create a window. MakeFiddleContext will detect this
                // object as non-valid and clean it up and return nullptr.
                return;
            }
#else
            // Emscripten doesn't support glfwGetError so print a generic
            // message.
            fprintf(stderr, "Failed to create GLFW window.\n");
#endif
            glfwTerminate();
            abort();
        }
        glfwMakeContextCurrent(m_glfwWindow);
#ifndef __EMSCRIPTEN__
        glfwSwapInterval(0);
#endif

        glfwSetKeyCallback(m_glfwWindow, key_callback);
        glfwSetCursorPosCallback(m_glfwWindow, mouse_position_callback);
        glfwSetMouseButtonCallback(m_glfwWindow, mouse_button_callback);

        FiddleContextOptions fiddleOptions = {
            .retinaDisplay = visibility == Visibility::fullscreen,
            .shaderCompilationMode = ShaderCompilationMode::alwaysSynchronous,
            .enableReadPixels = true,
            .disableRasterOrdering = backendParams.atomic,
            .coreFeaturesOnly = backendParams.core,
            .srgb = m_backendParams.srgb,
            .allowHeadlessRendering = visibility == Visibility::headless,
            .enableVulkanValidationLayers =
                !backendParams.disableValidationLayers,
            .disableDebugCallbacks = backendParams.disableDebugCallbacks,
            .gpuNameFilter = backendParams.gpuNameFilter.c_str(),
        };

        switch (backend)
        {
            case Backend::wgpu:
            case Backend::rhi:
            case Backend::coregraphics:
            case Backend::skia:
            case Backend::null:
                break;
            case Backend::gl:
            case Backend::angle:
                m_fiddleContext = FiddleContext::MakeGLPLS(fiddleOptions);
                break;
            case Backend::d3d:
                m_fiddleContext = FiddleContext::MakeD3DPLS(fiddleOptions);
                break;
            case Backend::d3d12:
                m_fiddleContext = FiddleContext::MakeD3D12PLS(fiddleOptions);
                break;
            case Backend::metal:
                m_fiddleContext = FiddleContext::MakeMetalPLS(fiddleOptions);
                break;
            case Backend::vk:
            case Backend::moltenvk:
            case Backend::swiftshader:
                m_fiddleContext = FiddleContext::MakeVulkanPLS(fiddleOptions);
                break;
            case Backend::dawn:
                m_fiddleContext = FiddleContext::MakeDawnPLS(fiddleOptions);
                break;
        }

        if (m_fiddleContext == nullptr)
        {
            return;
        }
        // On Mac we need to call glfwSetWindowSize even though we created the
        // window with these same dimensions.
        int w, h;
        glfwGetFramebufferSize(m_glfwWindow, &w, &h);
        m_width = w;
        m_height = h;
        dpi = m_fiddleContext->dpiScale(m_glfwWindow);
        m_fiddleContext->onSizeChanged(m_glfwWindow,
                                       m_width,
                                       m_height,
                                       m_msaaSampleCount);
    }

    ~TestingWindowFiddleContext() override
    {
        m_fiddleContext.reset();
        if (m_glfwWindow != nullptr)
        {
            glfwPollEvents();
            glfwDestroyWindow(m_glfwWindow);
            glfwTerminate();
        }
    }

    bool valid() const { return m_fiddleContext != nullptr; }

    rive::Factory* factory() override { return m_fiddleContext->factory(); }

    void resize(int width, int height) override
    {
        if (width != m_width || height != m_height)
        {
            glfwSetWindowSize(m_glfwWindow, width, height);
            m_fiddleContext->onSizeChanged(m_glfwWindow,
                                           width,
                                           height,
                                           m_msaaSampleCount);
            m_width = width;
            m_height = height;
        }
    }

    void hotloadShaders() override { m_fiddleContext->hotloadShaders(); }

    rive::rcp<rive_tests::OffscreenRenderTarget> makeOffscreenRenderTarget(
        uint32_t width,
        uint32_t height,
        bool riveRenderable) const override
    {
#ifndef RIVE_TOOLS_NO_GL
        if (auto* renderContextGL = m_fiddleContext->renderContextGLImpl())
        {
            return rive_tests::OffscreenRenderTarget::MakeGL(renderContextGL,
                                                             width,
                                                             height);
        }
#endif
#ifdef RIVE_VULKAN
        if (auto* renderContextVulkan =
                m_fiddleContext->renderContextVulkanImpl())
        {
            return rive_tests::OffscreenRenderTarget::MakeVulkan(
                renderContextVulkan->vulkanContext(),
                width,
                height,
                riveRenderable);
        }
#endif
        return nullptr;
    }

    std::unique_ptr<rive::Renderer> beginFrame(
        const FrameOptions& options) override
    {
        rive::gpu::RenderContext::FrameDescriptor frameDescriptor = {
            .renderTargetWidth = static_cast<uint32_t>(m_width),
            .renderTargetHeight = static_cast<uint32_t>(m_height),
            .loadAction = options.doClear
                              ? rive::gpu::LoadAction::clear
                              : rive::gpu::LoadAction::preserveRenderTarget,
            .clearColor = options.clearColor,
            .msaaSampleCount =
                std::max(m_msaaSampleCount, options.forceMSAA ? 4u : 0u),
            .disableRasterOrdering = options.disableRasterOrdering,
            .wireframe = options.wireframe,
            .clockwiseFillOverride =
                m_backendParams.clockwise || options.clockwiseFillOverride,
#ifdef WITH_RIVE_TOOLS
            .synthesizedFailureType = options.synthesizedFailureType,
#endif
        };
        m_fiddleContext->begin(std::move(frameDescriptor));
        return m_fiddleContext->makeRenderer(m_width, m_height);
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        m_fiddleContext->end(m_glfwWindow, pixelData);
        m_fiddleContext->tick();
        glfwPollEvents();
        glfwSwapBuffers(m_glfwWindow);
    }

    rive::gpu::RenderContext* renderContext() const override
    {
        return m_fiddleContext->renderContextOrNull();
    }

    rive::gpu::RenderContextGLImpl* renderContextGLImpl() const override
    {
        return m_fiddleContext->renderContextGLImpl();
    }

    rive::gpu::RenderTarget* renderTarget() const override
    {
        return m_fiddleContext->renderTargetOrNull();
    }

    void flushPLSContext(RenderTarget* offscreenRenderTarget) override
    {
        m_fiddleContext->flushPLSContext(offscreenRenderTarget);
    }

    bool consumeInputEvent(InputEventData& eventData) override
    {
        glfwPollEvents();
        const bool hasNewEvent = !inputEvents.empty();
        if (hasNewEvent)
        {
            eventData = inputEvents.front();
            inputEvents.pop();
        }
        return hasNewEvent;
    }

    InputEventData waitForInputEvent() override
    {
        InputEventData eventData;

        while (!consumeInputEvent(eventData))
        {
            glfwWaitEvents();
            if (glfwWindowShouldClose(m_glfwWindow))
            {
                printf("Window terminated by user.\n");
                exit(0);
            }
        }
        return eventData;
    }

    bool shouldQuit() const override
    {
        return glfwWindowShouldClose(m_glfwWindow);
    }

private:
    GLFWwindow* m_glfwWindow = nullptr;
    uint32_t m_msaaSampleCount = 0;
    BackendParams m_backendParams;
    std::unique_ptr<FiddleContext> m_fiddleContext;
};

TestingWindow* TestingWindow::MakeFiddleContext(
    Backend backend,
    const BackendParams& backendParams,
    Visibility visibility,
    void* platformWindow)
{
    auto window = std::make_unique<TestingWindowFiddleContext>(backend,
                                                               backendParams,
                                                               visibility,
                                                               platformWindow);
    if (!window->valid())
        window = nullptr;
    return window.release();
}

#endif

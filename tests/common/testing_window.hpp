/*
 * Copyright 2022 Rive
 */

#ifndef TESTING_WINDOW_HPP
#define TESTING_WINDOW_HPP

#include "common/offscreen_render_target.hpp"
#include "rive/renderer/gpu.hpp"
#include "rive/refcnt.hpp"
#include <memory>
#include <vector>
#include <string>

namespace rive
{
class Renderer;
class Factory;
namespace gpu
{
class RenderContext;
class RenderContextGLImpl;
class RenderTarget;
class Texture;
class TextureRenderTarget;
} // namespace gpu
}; // namespace rive

// Wraps a factory for rive::Renderer and a singleton target for it to render
// into (GL window, HTML canvas, software buffer, etc.):
//
//   TestingWindow::Init(type);
//   renderer = TestingWindow::Get()->reset(width, height);
//   ...
//

class TestingWindow
{
public:
    enum class Backend
    {
        gl,
        d3d,
        d3d12,
        metal,
        vk,

        // Vulkan on Metal, aka MoltenVK.
        // (defaults to /usr/local/share/vulkan/icd.d/MoltenVK_icd.json if
        // VK_ICD_FILENAMES is not set.)
        moltenvk,

        // Swiftshader, Google's CPU implementation of Vulkan.
        // (defaults to ./vk_swiftshader_icd.json if VK_ICD_FILENAMES is not
        // set.)
        swiftshader,

        angle,
        dawn,
        rhi,
        coregraphics,
        skia,
        null,
    };

    struct BackendParams
    {
        bool atomic = false;
        bool core = false;
        bool msaa = false;
        bool srgb = false;
        bool clockwise = false;
        bool disableValidationLayers = false;
        bool disableDebugCallbacks = false;
        std::string gpuNameFilter;
    };

    enum class Visibility
    {
        headless,
        window,
        fullscreen,
    };

    enum class InputEvent
    {
        KeyPress,
        MouseDown,
        MouseUp,
        MouseMove
    };

    // Aligns with GLFW's mouse button enum
    enum MouseButton : int
    {
        Left,
        Right,
        Middle,
        // can support more buttons
    };

    struct InputEventData
    {
        InputEventData() : InputEventData(InputEvent::KeyPress, '\0') {}

        InputEventData(InputEvent type, float posX, float posY) :
            eventType(type)
        {
            metadata = {.posX = posX, .posY = posY};
        }

        InputEventData(InputEvent type, char c) : eventType(type)
        {
            metadata = {.key = c};
        }

        InputEvent eventType;

        union
        {
            struct
            {
                // Move & down/up mouse press events all have coords
                float posX;
                float posY;
            };

            char key;
        } metadata;
    };

    static const char* BackendName(Backend);

    static Backend ParseBackend(const char* name, BackendParams*);
    static TestingWindow* Init(Backend,
                               const BackendParams&,
                               Visibility,
                               void* platformWindow = nullptr);
    static TestingWindow* Get();
    static void Set(TestingWindow* inWindow);
    static void Destroy();

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    virtual rive::Factory* factory() = 0;
    virtual void resize(int width, int height)
    {
        m_width = width;
        m_height = height;
    }

    struct FrameOptions
    {
        uint32_t clearColor;
        bool doClear = true;
        bool forceMSAA = false;
        bool disableRasterOrdering = false;
        bool wireframe = false;
        bool clockwiseFillOverride = false;
        rive::gpu::SynthesizedFailureType synthesizedFailureType =
            rive::gpu::SynthesizedFailureType::none;
    };
    virtual std::unique_ptr<rive::Renderer> beginFrame(const FrameOptions&) = 0;
    virtual void endFrame(std::vector<uint8_t>* pixelData = nullptr) = 0;

    // For testing directly on RenderContext.
    virtual rive::gpu::RenderContext* renderContext() const { return nullptr; }
    virtual rive::gpu::RenderContextGLImpl* renderContextGLImpl() const
    {
        return nullptr;
    }
    virtual rive::gpu::RenderTarget* renderTarget() const { return nullptr; }

    // Creates a new render target, for testing offscreen rendering.
    // If riveRenderable is false, the texture will lack features required by
    // Rive for rendering directly to it (e.g., input attachment, UAV, etc.),
    // which will exercise indirect rendering fallbacks.
    virtual rive::rcp<rive_tests::OffscreenRenderTarget>
    makeOffscreenRenderTarget(uint32_t width,
                              uint32_t height,
                              bool riveRenderable) const
    {
        return nullptr;
    }

    // For testing render pass breaks. Caller must call
    // renderContext()->beginFrame() again.
    virtual void flushPLSContext(
        rive::gpu::RenderTarget* offscreenRenderTarget = nullptr)
    {}

    virtual bool consumeInputEvent(InputEventData& eventData) { return false; }
    virtual InputEventData waitForInputEvent()
    {
        fprintf(stderr, "TestingWindow::waitForInputEvent not implemented.");
        abort();
    }

    virtual bool shouldQuit() const { return false; }

    virtual void hotloadShaders() {}

    virtual ~TestingWindow() {}

    static TestingWindow* MakeEGL(Backend,
                                  const BackendParams&,
                                  void* platformWindow);
#if defined(__APPLE__) && !defined(RIVE_UNREAL)
    static TestingWindow* MakeMetalTexture();
#endif
    static TestingWindow* MakeCoreGraphics();
    static TestingWindow* MakeFiddleContext(Backend,
                                            const BackendParams&,
                                            Visibility,
                                            void* platformWindow);
    static TestingWindow* MakeVulkanTexture(const BackendParams&);
    static TestingWindow* MakeAndroidVulkan(const BackendParams&,
                                            void* platformWindow);
    static TestingWindow* MakeSkia();
    static TestingWindow* MakeNULL();

protected:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

#endif

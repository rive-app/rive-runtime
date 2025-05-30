/*
 * Copyright 2022 Rive
 */

#ifndef TESTING_WINDOW_HPP
#define TESTING_WINDOW_HPP

#include "rive/rive_types.hpp"
#include "rive/enum_bitset.hpp"
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
        glatomic,
        glcw,
        glmsaa,
        d3d,
        d3datomic,
        d3d12,
        d3d12atomic,
        metal,
        metalcw,
        metalatomic,

        // System default Vulkan driver.
        vk,
        vkcore, // Vulkan with as few features enabled as possible.
        vksrgb,
        vkcw,

        // Vulkan on Metal, aka MoltenVK.
        // (defaults to /usr/local/share/vulkan/icd.d/MoltenVK_icd.json if
        // VK_ICD_FILENAMES is not set.)
        moltenvk,
        moltenvkcore,

        // Swiftshader, Google's CPU implementation of Vulkan.
        // (defaults to ./vk_swiftshader_icd.json if VK_ICD_FILENAMES is not
        // set.)
        swiftshader,
        swiftshadercore,

        angle,
        anglemsaa,
        dawn,

        rhi,

        coregraphics,
        skia,
    };

    constexpr static bool IsGL(Backend backend)
    {
        switch (backend)
        {
            case Backend::gl:
            case Backend::glatomic:
            case Backend::glcw:
            case Backend::glmsaa:
            case Backend::angle:
            case Backend::anglemsaa:
                return true;
            case Backend::d3d:
            case Backend::d3datomic:
            case Backend::d3d12:
            case Backend::d3d12atomic:
            case Backend::metal:
            case Backend::metalcw:
            case Backend::metalatomic:
            case Backend::vk:
            case Backend::vkcore:
            case Backend::vksrgb:
            case Backend::vkcw:
            case Backend::moltenvk:
            case Backend::moltenvkcore:
            case Backend::swiftshader:
            case Backend::swiftshadercore:
            case Backend::dawn:
            case Backend::rhi:
            case Backend::coregraphics:
            case Backend::skia:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    constexpr static bool IsANGLE(Backend backend)
    {
        switch (backend)
        {
            case Backend::angle:
            case Backend::anglemsaa:
                return true;
            case Backend::gl:
            case Backend::glatomic:
            case Backend::glcw:
            case Backend::glmsaa:
            case Backend::d3d:
            case Backend::d3datomic:
            case Backend::d3d12:
            case Backend::d3d12atomic:
            case Backend::metal:
            case Backend::metalcw:
            case Backend::metalatomic:
            case Backend::vk:
            case Backend::vkcore:
            case Backend::vksrgb:
            case Backend::vkcw:
            case Backend::moltenvk:
            case Backend::moltenvkcore:
            case Backend::swiftshader:
            case Backend::swiftshadercore:
            case Backend::dawn:
            case Backend::rhi:
            case Backend::coregraphics:
            case Backend::skia:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    constexpr static bool IsVulkan(Backend backend)
    {
        switch (backend)
        {
            case Backend::vk:
            case Backend::vkcore:
            case Backend::vksrgb:
            case Backend::vkcw:
            case Backend::moltenvk:
            case Backend::moltenvkcore:
            case Backend::swiftshader:
            case Backend::swiftshadercore:
                return true;
            case Backend::gl:
            case Backend::glatomic:
            case Backend::glcw:
            case Backend::glmsaa:
            case Backend::d3d:
            case Backend::d3datomic:
            case Backend::d3d12:
            case Backend::d3d12atomic:
            case Backend::metal:
            case Backend::metalcw:
            case Backend::metalatomic:
            case Backend::dawn:
            case Backend::angle:
            case Backend::anglemsaa:
            case Backend::rhi:
            case Backend::coregraphics:
            case Backend::skia:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    constexpr static bool IsAtomic(Backend backend)
    {
        switch (backend)
        {
            case Backend::glatomic:
            case Backend::glcw:
            case Backend::d3datomic:
            case Backend::d3d12atomic:
            case Backend::metalatomic:
            case Backend::rhi:
            case Backend::vkcore:
            case Backend::vksrgb:
            case Backend::vkcw:
            case Backend::moltenvkcore:
            case Backend::swiftshadercore:
                return true;
            case Backend::gl:
            case Backend::glmsaa:
            case Backend::d3d:
            case Backend::d3d12:
            case Backend::metal:
            case Backend::metalcw:
            case Backend::vk:
            case Backend::moltenvk:
            case Backend::swiftshader:
            case Backend::angle:
            case Backend::anglemsaa:
            case Backend::dawn:
            case Backend::coregraphics:
            case Backend::skia:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    constexpr static bool IsCore(Backend backend)
    {
        switch (backend)
        {
            case Backend::vkcore:
            case Backend::moltenvkcore:
            case Backend::swiftshadercore:
                return true;
            case Backend::glatomic:
            case Backend::glcw:
            case Backend::d3datomic:
            case Backend::metalatomic:
            case Backend::gl:
            case Backend::glmsaa:
            case Backend::d3d:
            case Backend::d3d12:
            case Backend::d3d12atomic:
            case Backend::metal:
            case Backend::metalcw:
            case Backend::vk:
            case Backend::vksrgb:
            case Backend::vkcw:
            case Backend::moltenvk:
            case Backend::swiftshader:
            case Backend::angle:
            case Backend::anglemsaa:
            case Backend::dawn:
            case Backend::rhi:
            case Backend::coregraphics:
            case Backend::skia:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    constexpr static bool IsSRGB(Backend backend)
    {
        switch (backend)
        {
            case Backend::vksrgb:
                return true;
            case Backend::glatomic:
            case Backend::glcw:
            case Backend::d3datomic:
            case Backend::d3d12atomic:
            case Backend::metalatomic:
            case Backend::gl:
            case Backend::glmsaa:
            case Backend::d3d:
            case Backend::d3d12:
            case Backend::metal:
            case Backend::metalcw:
            case Backend::vk:
            case Backend::vkcore:
            case Backend::vkcw:
            case Backend::moltenvk:
            case Backend::moltenvkcore:
            case Backend::swiftshader:
            case Backend::swiftshadercore:
            case Backend::angle:
            case Backend::anglemsaa:
            case Backend::dawn:
            case Backend::rhi:
            case Backend::coregraphics:
            case Backend::skia:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    constexpr static bool IsMSAA(Backend backend)
    {
        switch (backend)
        {
            case Backend::glmsaa:
            case Backend::anglemsaa:
                return true;
            case Backend::glatomic:
            case Backend::glcw:
            case Backend::d3datomic:
            case Backend::metalatomic:
            case Backend::vkcore:
            case Backend::vksrgb:
            case Backend::vkcw:
            case Backend::moltenvkcore:
            case Backend::swiftshadercore:
            case Backend::gl:
            case Backend::d3d:
            case Backend::d3d12:
            case Backend::d3d12atomic:
            case Backend::metal:
            case Backend::metalcw:
            case Backend::vk:
            case Backend::moltenvk:
            case Backend::swiftshader:
            case Backend::angle:
            case Backend::dawn:
            case Backend::rhi:
            case Backend::coregraphics:
            case Backend::skia:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    constexpr static bool IsClockwiseFill(Backend backend)
    {
        switch (backend)
        {
            case Backend::glcw:
            case Backend::metalcw:
            case Backend::vkcw:
                return true;
            case Backend::glatomic:
            case Backend::glmsaa:
            case Backend::d3datomic:
            case Backend::d3d12:
            case Backend::d3d12atomic:
            case Backend::metalatomic:
            case Backend::moltenvkcore:
            case Backend::swiftshadercore:
            case Backend::gl:
            case Backend::d3d:
            case Backend::metal:
            case Backend::vk:
            case Backend::vkcore:
            case Backend::vksrgb:
            case Backend::moltenvk:
            case Backend::swiftshader:
            case Backend::angle:
            case Backend::anglemsaa:
            case Backend::dawn:
            case Backend::rhi:
            case Backend::coregraphics:
            case Backend::skia:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    enum class RendererFlags
    {
        none = 0,
        useMSAA = 1 << 0,
        disableRasterOrdering = 1 << 1,
        clockwiseFillOverride = 1 << 2,
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
    static Backend ParseBackend(const char* name, std::string* gpuNameFilter);
    static TestingWindow* Init(Backend,
                               Visibility,
                               const std::string& gpuNameFilter,
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
        bool disableRasterOrdering = false;
        bool wireframe = false;
        bool clockwiseFillOverride = false;
        bool synthesizeCompilationFailures = false;
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

    // For testing render pass breaks. Caller must call
    // renderContext()->beginFrame() again.
    virtual void flushPLSContext() {}

    virtual bool consumeInputEvent(InputEventData& eventData) { return false; }
    virtual InputEventData waitForInputEvent()
    {
        fprintf(stderr, "TestingWindow::waitForInputEvent not implemented.");
        abort();
    }

    virtual bool shouldQuit() const { return false; }

    virtual void hotloadShaders() {}

    virtual ~TestingWindow() {}

    struct BackendParams
    {
        bool coreFeaturesOnly = false;
        bool srgb = false;
        bool clockwiseFill = false;
        bool disableValidationLayers = false;
        bool disableDebugCallbacks = false;
        std::string gpuNameFilter;
    };

    static TestingWindow* MakeGLFW(Backend, Visibility);
    static TestingWindow* MakeEGL(Backend, void* platformWindow);
#if defined(__APPLE__) && !defined(RIVE_UNREAL)
    static TestingWindow* MakeMetalTexture();
#endif
    static TestingWindow* MakeCoreGraphics();
    static TestingWindow* MakeFiddleContext(Backend,
                                            Visibility,
                                            const BackendParams&,
                                            void* platformWindow);
    static TestingWindow* MakeVulkanTexture(const BackendParams&);
    static TestingWindow* MakeAndroidVulkan(const BackendParams&,
                                            void* platformWindow);
    static TestingWindow* MakeSkia();

protected:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

RIVE_MAKE_ENUM_BITSET(TestingWindow::RendererFlags);

#endif

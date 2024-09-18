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

// Wraps a factory for rive::Renderer and a singleton target for it to render into
// (GL window, HTML canvas, software buffer, etc.):
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
        glmsaa,
        d3d,
        d3datomic,
        metal,
        metalatomic,

        // System default Vulkan driver.
        vk,
        vkcore, // Vulkan with as few features enabled as possible.

        // Vulkan on Metal, aka MoltenVK.
        // (defaults to /usr/local/share/vulkan/icd.d/MoltenVK_icd.json if
        // VK_ICD_FILENAMES is not set.)
        moltenvk,
        moltenvkcore,

        // Swiftshader, Google's CPU implementation of Vulkan.
        // (defaults to ./vk_swiftshader_icd.json if VK_ICD_FILENAMES is not set.)
        swiftshader,
        swiftshadercore,

        angle,
        anglemsaa,
        dawn,
        coregraphics,
    };

    constexpr static bool IsGL(Backend backend)
    {
        switch (backend)
        {
            case Backend::gl:
            case Backend::glatomic:
            case Backend::glmsaa:
            case Backend::angle:
            case Backend::anglemsaa:
                return true;
            case Backend::d3d:
            case Backend::d3datomic:
            case Backend::metal:
            case Backend::metalatomic:
            case Backend::vk:
            case Backend::vkcore:
            case Backend::moltenvk:
            case Backend::moltenvkcore:
            case Backend::swiftshader:
            case Backend::swiftshadercore:
            case Backend::dawn:
            case Backend::coregraphics:
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
            case Backend::glmsaa:
            case Backend::d3d:
            case Backend::d3datomic:
            case Backend::metal:
            case Backend::metalatomic:
            case Backend::vk:
            case Backend::vkcore:
            case Backend::moltenvk:
            case Backend::moltenvkcore:
            case Backend::swiftshader:
            case Backend::swiftshadercore:
            case Backend::dawn:
            case Backend::coregraphics:
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
            case Backend::moltenvk:
            case Backend::moltenvkcore:
            case Backend::swiftshader:
            case Backend::swiftshadercore:
                return true;
            case Backend::gl:
            case Backend::glatomic:
            case Backend::glmsaa:
            case Backend::d3d:
            case Backend::d3datomic:
            case Backend::metal:
            case Backend::metalatomic:
            case Backend::dawn:
            case Backend::coregraphics:
            case Backend::angle:
            case Backend::anglemsaa:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    constexpr static bool IsAtomic(Backend backend)
    {
        switch (backend)
        {
            case Backend::glatomic:
            case Backend::d3datomic:
            case Backend::metalatomic:
            case Backend::vkcore:
            case Backend::moltenvkcore:
            case Backend::swiftshadercore:
                return true;
            case Backend::gl:
            case Backend::glmsaa:
            case Backend::d3d:
            case Backend::metal:
            case Backend::vk:
            case Backend::moltenvk:
            case Backend::swiftshader:
            case Backend::angle:
            case Backend::anglemsaa:
            case Backend::dawn:
            case Backend::coregraphics:
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
            case Backend::d3datomic:
            case Backend::metalatomic:
            case Backend::gl:
            case Backend::glmsaa:
            case Backend::d3d:
            case Backend::metal:
            case Backend::vk:
            case Backend::moltenvk:
            case Backend::swiftshader:
            case Backend::angle:
            case Backend::anglemsaa:
            case Backend::dawn:
            case Backend::coregraphics:
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
            case Backend::d3datomic:
            case Backend::metalatomic:
            case Backend::vkcore:
            case Backend::moltenvkcore:
            case Backend::swiftshadercore:
            case Backend::gl:
            case Backend::d3d:
            case Backend::metal:
            case Backend::vk:
            case Backend::moltenvk:
            case Backend::swiftshader:
            case Backend::angle:
            case Backend::dawn:
            case Backend::coregraphics:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    enum class RendererFlags
    {
        none = 0,
        useMSAA = 1 << 0,
        disableRasterOrdering = 1 << 1,
    };

    enum class Visibility
    {
        headless,
        window,
        fullscreen,
    };

    static const char* BackendName(Backend);
    static Backend ParseBackend(const char* name, std::string* gpuNameFilter);
    static TestingWindow* Init(Backend,
                               Visibility,
                               const std::string& gpuNameFilter,
                               void* platformWindow = nullptr);
    static TestingWindow* Get();
    static void Destroy();

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    virtual rive::Factory* factory() = 0;
    virtual void resize(int width, int height)
    {
        m_width = width;
        m_height = height;
    }
    virtual std::unique_ptr<rive::Renderer> beginFrame(uint32_t clearColor,
                                                       bool doClear = true) = 0;
    virtual void endFrame(std::vector<uint8_t>* pixelData = nullptr) = 0;

    // For testing directly on RenderContext.
    virtual rive::gpu::RenderContext* renderContext() const { return nullptr; }
    virtual rive::gpu::RenderContextGLImpl* renderContextGLImpl() const { return nullptr; }
    virtual rive::gpu::RenderTarget* renderTarget() const { return nullptr; }

    // For testing render pass breaks. Caller must call
    // renderContext()->beginFrame() again.
    virtual void flushPLSContext() {}

    // Blocks until a key is pressed.
    virtual bool peekKey(char& key) { return false; }
    virtual char getKey();
    virtual bool shouldQuit() const { return false; }

    virtual ~TestingWindow() {}

protected:
    uint32_t m_width = 0;
    uint32_t m_height = 0;

private:
    static std::unique_ptr<TestingWindow> MakeGLFW(Backend, Visibility);
    static std::unique_ptr<TestingWindow> MakeEGL(Backend, void* platformWindow);
#ifdef _WIN32
    static std::unique_ptr<TestingWindow> MakeD3D(Visibility);
#endif
#ifdef __APPLE__
    static std::unique_ptr<TestingWindow> MakeMetalTexture();
#endif
#ifdef RIVE_MACOSX
    static std::unique_ptr<TestingWindow> MakeCoreGraphics();
#endif
    static std::unique_ptr<TestingWindow> MakeFiddleContext(Backend,
                                                            Visibility,
                                                            const char* gpuNameFilter,
                                                            void* platformWindow);
    static std::unique_ptr<TestingWindow> MakeVulkanTexture(bool coreFeaturesOnly,
                                                            const char* gpuNameFilter);
    static std::unique_ptr<TestingWindow> MakeAndroidVulkan(void* platformWindow);
};

RIVE_MAKE_ENUM_BITSET(TestingWindow::RendererFlags);

#endif

/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/rive_types.hpp"
#include "rive/enum_bitset.hpp"
#include <memory>
#include <vector>
#include <string>

#include "rive/renderer.hpp"
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace rivegm
{

class GM
{
    std::string m_Name;
    const int m_Width, m_Height;

public:
    GM(int width, int height, const char name[]) :
        m_Name(name, strlen(name)), m_Width(width), m_Height(height)
    {}
    virtual ~GM() {}

    int width() const { return m_Width; }
    int height() const { return m_Height; }
    std::string name() const { return m_Name; }

    void onceBeforeDraw() { this->onOnceBeforeDraw(); }

    // Calls clearColor(), TestingWindow::beginFrame(), draw(), TestingWindow::flush().
    // (Most GMs just need to override onDraw() instead of overriding this method.)
    virtual void run(std::vector<uint8_t>* pixels);

    virtual rive::ColorInt clearColor() const { return 0xffffffff; }

    void draw(rive::Renderer*);

protected:
    virtual void onOnceBeforeDraw() {}
    virtual void onDraw(rive::Renderer*) = 0;
};

template <typename T> class Registry
{
    static Registry* s_Head;
    static Registry* s_Tail;

    T m_Value;
    Registry* m_Next = nullptr;

public:
    Registry(T value, bool isSlow) : m_Value(value)
    {
        if (s_Head == nullptr)
        {
            s_Tail = s_Head = this;
        }
        else if (isSlow)
        {
            m_Next = s_Head;
            s_Head = this;
        }
        else
        {
            s_Tail->m_Next = this;
            s_Tail = this;
        }
    }

    static const Registry* head() { return s_Head; }
    const T& get() const { return m_Value; }
    const Registry* next() const { return m_Next; }
};

template <typename T> Registry<T>* Registry<T>::s_Head = nullptr;
template <typename T> Registry<T>* Registry<T>::s_Tail = nullptr;

using GMFactory = std::unique_ptr<GM> (*)();
using GMRegistry = Registry<GMFactory>;

} // namespace rivegm

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

// Wraps a factory for rive::Renderer and a singleton target for it to render into (GL window, HTML
// canvas, software buffer, etc.):
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
        vulkan,
        vulkanatomic,

        // Vulkan on Metal, aka MoltenVK.
        // (defaults to /usr/local/share/vulkan/icd.d/MoltenVK_icd.json if VK_ICD_FILENAMES is not
        // set.)
        moltenvk,
        moltenvkatomic,

        // Swiftshader, Google's CPU implementation of Vulkan.
        // (defaults to ./vk_swiftshader_icd.json if VK_ICD_FILENAMES is not set.)
        swiftshader,
        swiftshaderatomic,

        angle,
        anglemsaa,
        dawn,
        coregraphics,

        rhi,
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
            case Backend::vulkan:
            case Backend::vulkanatomic:
            case Backend::moltenvk:
            case Backend::moltenvkatomic:
            case Backend::swiftshader:
            case Backend::swiftshaderatomic:
            case Backend::dawn:
            case Backend::coregraphics:
            case Backend::rhi:
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
            case Backend::vulkan:
            case Backend::vulkanatomic:
            case Backend::moltenvk:
            case Backend::moltenvkatomic:
            case Backend::swiftshader:
            case Backend::swiftshaderatomic:
            case Backend::dawn:
            case Backend::coregraphics:
            case Backend::rhi:
                return false;
        }
        RIVE_UNREACHABLE();
    }

    constexpr static bool IsVulkan(Backend backend)
    {
        switch (backend)
        {
            case Backend::vulkan:
            case Backend::vulkanatomic:
            case Backend::moltenvk:
            case Backend::moltenvkatomic:
            case Backend::swiftshader:
            case Backend::swiftshaderatomic:
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
            case Backend::rhi:
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
            case Backend::vulkanatomic:
            case Backend::moltenvkatomic:
            case Backend::swiftshaderatomic:
            case Backend::rhi:
                return true;
            case Backend::gl:
            case Backend::glmsaa:
            case Backend::d3d:
            case Backend::metal:
            case Backend::vulkan:
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
            case Backend::vulkanatomic:
            case Backend::moltenvkatomic:
            case Backend::swiftshaderatomic:
            case Backend::gl:
            case Backend::d3d:
            case Backend::metal:
            case Backend::vulkan:
            case Backend::moltenvk:
            case Backend::swiftshader:
            case Backend::angle:
            case Backend::dawn:
            case Backend::coregraphics:
            case Backend::rhi:
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
    virtual std::unique_ptr<rive::Renderer> beginFrame(uint32_t clearColor,
                                                       bool doClear = true) = 0;
    virtual void endFrame(std::vector<uint8_t>* pixelData = nullptr) = 0;

    // For testing directly on RenderContext.
    virtual rive::gpu::RenderContext* renderContext() const { return nullptr; }
    virtual rive::gpu::RenderContextGLImpl* renderContextGLImpl() const { return nullptr; }
    virtual rive::gpu::RenderTarget* renderTarget() const { return nullptr; }

    // For testing render pass breaks. Caller must call renderContext()->beginFrame() again.
    virtual void flushPLSContext() {}

    // Blocks until a key is pressed.
    virtual bool peekKey(char& key) { return false; }
    virtual char getKey()
    {
        fprintf(stderr, "TestingWindow::getKey not implemented.");
        abort();
    }
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
    static std::unique_ptr<TestingWindow> MakeVulkanTexture(const char* gpuNameFilter);
    static std::unique_ptr<TestingWindow> MakeAndroidVulkan(void* platformWindow);
};

RIVE_MAKE_ENUM_BITSET(TestingWindow::RendererFlags);

typedef const void* REGISTRY_HANDLE;

int gms_main(int argc, const char* argv[]);
void gms_memcpy(void* dst, void* src, size_t size);
bool gms_registry_get_size(REGISTRY_HANDLE position_handle, size_t& width, size_t& height);
bool gms_registry_get_name(REGISTRY_HANDLE position_handle, std::string& name);
REGISTRY_HANDLE gms_get_registry_head();
REGISTRY_HANDLE gms_registry_get_next(REGISTRY_HANDLE position_handle);
bool gms_run_gm(REGISTRY_HANDLE gm_handle);

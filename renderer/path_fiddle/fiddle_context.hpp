#pragma once

#include <vector>

#include "rive/renderer/render_context.hpp"

struct GLFWwindow;

struct FiddleContextOptions
{
    bool retinaDisplay = true;
    bool synchronousShaderCompilations = false;
    bool enableReadPixels = false;
    bool disableRasterOrdering = false;
    // Allow rendering to a texture instead of an OS window. (Speeds up the execution of goldens &
    // gms significantly on Vulkan/Windows.)
    bool allowHeadlessRendering = false;
    bool enableVulkanValidationLayers = false;
    const char* gpuNameFilter = nullptr; // Substring of GPU name to use.
};

class FiddleContext
{
public:
    virtual ~FiddleContext() {}
    virtual float dpiScale(GLFWwindow*) const = 0;
    virtual rive::Factory* factory() = 0;
    virtual rive::gpu::PLSRenderContext* plsContextOrNull() = 0;
    virtual rive::gpu::PLSRenderTarget* plsRenderTargetOrNull() = 0;
    virtual void onSizeChanged(GLFWwindow*, int width, int height, uint32_t sampleCount) {}
    virtual void toggleZoomWindow() = 0;
    virtual std::unique_ptr<rive::Renderer> makeRenderer(int width, int height) = 0;
    virtual void begin(const rive::gpu::PLSRenderContext::FrameDescriptor&) = 0;
    virtual void flushPLSContext() = 0; // Called by end()
    virtual void end(GLFWwindow*, std::vector<uint8_t>* pixelData = nullptr) = 0;
    virtual void tick(){};

    static std::unique_ptr<FiddleContext> MakeGLSkia();
    static std::unique_ptr<FiddleContext> MakeGLPLS();
#ifdef RIVE_MACOSX
    static std::unique_ptr<FiddleContext> MakeMetalPLS(FiddleContextOptions = {});
#else
    static std::unique_ptr<FiddleContext> MakeMetalPLS(FiddleContextOptions = {})
    {
        return nullptr;
    }
#endif
    static std::unique_ptr<FiddleContext> MakeD3DPLS(FiddleContextOptions = {});
    static std::unique_ptr<FiddleContext> MakeVulkanPLS(FiddleContextOptions = {});
    static std::unique_ptr<FiddleContext> MakeDawnPLS(FiddleContextOptions = {});
};

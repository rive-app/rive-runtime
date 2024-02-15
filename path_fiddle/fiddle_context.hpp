#pragma once

#include <vector>

#include "rive/pls/pls_render_context.hpp"

struct GLFWwindow;

struct FiddleContextOptions
{
    bool retinaDisplay = true;
    bool readableFramebuffer = true;
    bool synchronousShaderCompilations = false;
    bool enableReadPixels = false;
    bool disableRasterOrdering = false;
};

class FiddleContext
{
public:
    virtual ~FiddleContext() {}
    virtual float dpiScale(GLFWwindow*) const = 0;
    virtual rive::Factory* factory() = 0;
    virtual rive::pls::PLSRenderContext* plsContextOrNull() = 0;
    virtual void onSizeChanged(GLFWwindow*, int width, int height) {}
    virtual void toggleZoomWindow() = 0;
    virtual std::unique_ptr<rive::Renderer> makeRenderer(int width, int height) = 0;
    virtual void begin(rive::pls::PLSRenderContext::FrameDescriptor&&) = 0;
    virtual void end(GLFWwindow*, std::vector<uint8_t>* pixelData = nullptr) = 0;
    virtual void tick(){};

    static std::unique_ptr<FiddleContext> MakeGLSkia();
    static std::unique_ptr<FiddleContext> MakeGLPLS();
#ifdef __APPLE__
    static std::unique_ptr<FiddleContext> MakeMetalPLS(FiddleContextOptions = {});
#else
    static std::unique_ptr<FiddleContext> MakeMetalPLS(FiddleContextOptions = {})
    {
        return nullptr;
    }
#endif
    static std::unique_ptr<FiddleContext> MakeD3DPLS(FiddleContextOptions = {});
    static std::unique_ptr<FiddleContext> MakeDawnPLS(FiddleContextOptions = {});
};

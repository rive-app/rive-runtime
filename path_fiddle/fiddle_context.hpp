#pragma once

#include <memory>

namespace rive
{
class Factory;
class Renderer;
} // namespace rive

constexpr static int kZoomWindowWidth = 70, kZoomWindowHeight = 42, kZoomWindowScale = 16;

extern struct GLFWwindow* g_window;
extern bool g_wireframe;
extern bool g_disableFill;
extern bool g_disableStroke;

class FiddleContext
{
public:
    virtual ~FiddleContext() {}
    virtual float dpiScale() const = 0;
    virtual std::unique_ptr<rive::Factory> makeFactory() = 0;
    virtual void onSizeChanged(int width, int height) {}
    virtual void toggleZoomWindow() = 0;
    virtual std::unique_ptr<rive::Renderer> makeRenderer(int width, int height) = 0;
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void shrinkGPUResourcesToFit() = 0;

    static std::unique_ptr<FiddleContext> MakeWEBGLSkia();
    static std::unique_ptr<FiddleContext> MakeWEBGLPLS();
#ifdef __APPLE__
    static std::unique_ptr<FiddleContext> MakeMetalPLS();
#endif
};

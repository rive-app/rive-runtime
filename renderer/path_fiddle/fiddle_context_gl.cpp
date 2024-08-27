#include "fiddle_context.hpp"

#ifdef RIVE_TOOLS_NO_GLFW

std::unique_ptr<FiddleContext> FiddleContext::MakeGLPLS() { return nullptr; }

#else

#include "path_fiddle.hpp"
#include "rive/renderer/gl/gles3.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"

#ifdef RIVE_WEBGL
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

using namespace rive;
using namespace rive::gpu;

#ifdef RIVE_DESKTOP_GL
#ifdef DEBUG
static void GLAPIENTRY err_msg_callback(GLenum source,
                                        GLenum type,
                                        GLuint id,
                                        GLenum severity,
                                        GLsizei length,
                                        const GLchar* message,
                                        const void* userParam)
{
    if (type == GL_DEBUG_TYPE_ERROR)
    {
        printf("GL ERROR: %s\n", message);
        fflush(stdout);
        // Don't abort if it's a shader compile error; let our internal handlers print the source
        // (for debugging) and exit on their own.
        if (!strstr(message, "SHADER_ID_COMPILE error has been generated"))
        {
            assert(0);
        }
    }
    else if (type == GL_DEBUG_TYPE_PERFORMANCE)
    {
        if (strcmp(message,
                   "API_ID_REDUNDANT_FBO performance warning has been generated. Redundant state "
                   "change in glBindFramebuffer API call, FBO 0, \"\", already bound.") == 0)
        {
            return;
        }
        if (strstr(message, "is being recompiled based on GL state."))
        {
            return;
        }
        printf("GL PERF: %s\n", message);
        fflush(stdout);
    }
}
#endif
#endif

class FiddleContextGL : public FiddleContext
{
public:
    FiddleContextGL()
    {
#ifdef RIVE_DESKTOP_GL
        // Load the OpenGL API using glad.
        if (!gladLoadCustomLoader((GLADloadproc)glfwGetProcAddress))
        {
            fprintf(stderr, "Failed to initialize glad.\n");
            abort();
        }
#endif

        printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
        printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
        printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
#ifdef RIVE_DESKTOP_GL
        printf("GL_ANGLE_shader_pixel_local_storage_coherent: %i\n",
               GLAD_GL_ANGLE_shader_pixel_local_storage_coherent);
#endif
#if 0
        int n;
        glGetIntegerv(GL_NUM_EXTENSIONS, &n);
        for (size_t i = 0; i < n; ++i)
        {
            printf("  %s\n", glGetStringi(GL_EXTENSIONS, i));
        }
#endif

#ifdef RIVE_DESKTOP_GL
#ifdef DEBUG
        if (GLAD_GL_KHR_debug)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
            glDebugMessageCallbackKHR(&err_msg_callback, nullptr);
        }
#endif
#endif
    }

    ~FiddleContextGL() { glDeleteFramebuffers(1, &m_zoomWindowFBO); }

    float dpiScale(GLFWwindow*) const override
    {
#if defined(__APPLE__) || defined(RIVE_WEBGL)
        return 2;
#else
        return 1;
#endif
    }

    void toggleZoomWindow() final
    {
        if (m_zoomWindowFBO)
        {
            glDeleteFramebuffers(1, &m_zoomWindowFBO);
            m_zoomWindowFBO = 0;
        }
        else
        {
            GLuint tex;
            glGenTextures(1, &tex);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, kZoomWindowWidth, kZoomWindowHeight);

            glGenFramebuffers(1, &m_zoomWindowFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, m_zoomWindowFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glDeleteTextures(1, &tex);
        }
    }

    void end(GLFWwindow* window, std::vector<uint8_t>* pixelData) final
    {
        assert(pixelData == nullptr); // Not implemented yet.
        onEnd();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (m_zoomWindowFBO)
        {
            // Blit the zoom window.
            double xd, yd;
            glfwGetCursorPos(window, &xd, &yd);
            xd *= dpiScale(window);
            yd *= dpiScale(window);
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            int x = xd, y = height - yd;
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_zoomWindowFBO);
            glBlitFramebuffer(x - kZoomWindowWidth / 2,
                              y - kZoomWindowHeight / 2,
                              x + kZoomWindowWidth / 2,
                              y + kZoomWindowHeight / 2,
                              0,
                              0,
                              kZoomWindowWidth,
                              kZoomWindowHeight,
                              GL_COLOR_BUFFER_BIT,
                              GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glEnable(GL_SCISSOR_TEST);
            glScissor(0,
                      0,
                      kZoomWindowWidth * kZoomWindowScale + 2,
                      kZoomWindowHeight * kZoomWindowScale + 2);
            glClearColor(.6, .6, .6, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_zoomWindowFBO);
            glBlitFramebuffer(0,
                              0,
                              kZoomWindowWidth,
                              kZoomWindowHeight,
                              0,
                              0,
                              kZoomWindowWidth * kZoomWindowScale,
                              kZoomWindowHeight * kZoomWindowScale,
                              GL_COLOR_BUFFER_BIT,
                              GL_NEAREST);
            glDisable(GL_SCISSOR_TEST);
        }
    }

protected:
    virtual void onEnd() = 0;

private:
    GLuint m_zoomWindowFBO = 0;
};

class FiddleContextGLPLS : public FiddleContextGL
{
public:
    FiddleContextGLPLS()
    {
        if (!m_plsContext)
        {
            fprintf(stderr, "Failed to create a PLS renderer.\n");
            abort();
        }
    }

    rive::Factory* factory() override { return m_plsContext.get(); }

    rive::gpu::PLSRenderContext* plsContextOrNull() override { return m_plsContext.get(); }

    rive::gpu::PLSRenderTarget* plsRenderTargetOrNull() override { return m_renderTarget.get(); }

    void onSizeChanged(GLFWwindow* window, int width, int height, uint32_t sampleCount) override
    {
        m_renderTarget = make_rcp<FramebufferRenderTargetGL>(width, height, 0, sampleCount);
    }

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<RiveRenderer>(m_plsContext.get());
    }

    void begin(const PLSRenderContext::FrameDescriptor& frameDescriptor) override
    {
        m_plsContext->static_impl_cast<PLSRenderContextGLImpl>()->invalidateGLState();
        m_plsContext->beginFrame(frameDescriptor);
    }

    void onEnd() override
    {
        flushPLSContext();
        m_plsContext->static_impl_cast<PLSRenderContextGLImpl>()->unbindGLInternalResources();
    }

    void flushPLSContext() override { m_plsContext->flush({.renderTarget = m_renderTarget.get()}); }

private:
    std::unique_ptr<PLSRenderContext> m_plsContext =
        PLSRenderContextGLImpl::MakeContext(PLSRenderContextGLImpl::ContextOptions());
    rcp<PLSRenderTargetGL> m_renderTarget;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeGLPLS()
{
    return std::make_unique<FiddleContextGLPLS>();
}

#endif

#ifndef RIVE_SKIA

std::unique_ptr<FiddleContext> FiddleContext::MakeGLSkia() { return nullptr; }

#else

#include "skia_factory.hpp"
#include "skia_renderer.hpp"
#include "skia/include/core/SkCanvas.h"
#include "skia/include/core/SkSurface.h"
#include "skia/include/gpu/GrDirectContext.h"
#include "skia/include/gpu/gl/GrGLAssembleInterface.h"
#include "skia/include/gpu/gl/GrGLInterface.h"
#include "include/effects/SkImageFilters.h"

static GrGLFuncPtr get_skia_gl_proc_address(void* ctx, const char name[])
{
    return glfwGetProcAddress(name);
}

class FiddleContextGLSkia : public FiddleContextGL
{
public:
    FiddleContextGLSkia() :
        m_grContext(
            GrDirectContext::MakeGL(GrGLMakeAssembledInterface(nullptr, get_skia_gl_proc_address)))
    {
        if (!m_grContext)
        {
            fprintf(stderr, "GrDirectContext::MakeGL failed.\n");
            abort();
        }
    }

    rive::Factory* factory() override { return &m_factory; }

    rive::gpu::PLSRenderContext* plsContextOrNull() override { return nullptr; }

    rive::gpu::PLSRenderTarget* plsRenderTargetOrNull() override { return nullptr; }

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        GrBackendRenderTarget backendRT(width,
                                        height,
                                        1 /*samples*/,
                                        0 /*stencilBits*/,
                                        {0 /*fbo 0*/, GL_RGBA8});

        SkSurfaceProps surfProps(0, kUnknown_SkPixelGeometry);

        m_skSurface = SkSurface::MakeFromBackendRenderTarget(m_grContext.get(),
                                                             backendRT,
                                                             kBottomLeft_GrSurfaceOrigin,
                                                             kRGBA_8888_SkColorType,
                                                             nullptr,
                                                             &surfProps);
        if (!m_skSurface)
        {
            fprintf(stderr, "SkSurface::MakeFromBackendRenderTarget failed.\n");
            abort();
        }
        return std::make_unique<SkiaRenderer>(m_skSurface->getCanvas());
    }

    void begin(const PLSRenderContext::FrameDescriptor& frameDescriptor) override
    {
        m_skSurface->getCanvas()->clear(frameDescriptor.clearColor);
        m_grContext->resetContext();
        m_skSurface->getCanvas()->save();
    }

    void onEnd() override
    {
        m_skSurface->getCanvas()->restore();
        m_skSurface->flush();
    }

    void flushPLSContext() override {}

private:
    SkiaFactory m_factory;
    const sk_sp<GrDirectContext> m_grContext;
    sk_sp<SkSurface> m_skSurface;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeGLSkia()
{
    return std::make_unique<FiddleContextGLSkia>();
}

#endif

#include "fiddle_context.hpp"

#include "rive/pls/gl/gles3.hpp"
#include "rive/pls/pls_factory.hpp"
#include "rive/pls/pls_renderer.hpp"
#include "rive/pls/gl/pls_render_context_gl_impl.hpp"
#include "rive/pls/gl/pls_render_target_gl.hpp"

#ifdef RIVE_WASM
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

using namespace rive;
using namespace rive::pls;

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
    }
    else if (type == GL_DEBUG_TYPE_PERFORMANCE)
    {
        if (strcmp(message,
                   "API_ID_REDUNDANT_FBO performance warning has been generated. Redundant state "
                   "change in glBindFramebuffer API call, FBO 0, \"\", already bound.") == 0)
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
            exit(-1);
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

    float dpiScale() const override
    {
#ifdef __APPLE__
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

    void end() final
    {
        onEnd();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (m_zoomWindowFBO)
        {
            // Blit the zoom window.
            double xd, yd;
            glfwGetCursorPos(g_window, &xd, &yd);
            xd *= dpiScale();
            yd *= dpiScale();
            int width = 0, height = 0;
            glfwGetFramebufferSize(g_window, &width, &height);
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

#ifdef RIVE_SKIA
#include "skia_factory.hpp"
#include "skia_renderer.hpp"
#include "skia/include/core/SkCanvas.h"
#include "skia/include/core/SkSurface.h"
#include "skia/include/gpu/GrDirectContext.h"
#include "skia/include/gpu/gl/GrGLAssembleInterface.h"
#include "skia/include/gpu/gl/GrGLInterface.h"

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
            exit(-1);
        }
    }

    std::unique_ptr<rive::Factory> makeFactory() override
    {
        return std::make_unique<SkiaFactory>();
    }

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
            exit(-1);
        }
        return std::make_unique<SkiaRenderer>(m_skSurface->getCanvas());
    }

    void begin() override
    {
        m_skSurface->getCanvas()->clear({.25, .25, .25, 1});
        m_grContext->resetContext();
    }

    void onEnd() override { m_skSurface->flush(); }

    void shrinkGPUResourcesToFit() final {}

private:
    const sk_sp<GrDirectContext> m_grContext;
    sk_sp<SkSurface> m_skSurface;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeGLSkia()
{
    return std::make_unique<FiddleContextGLSkia>();
}
#endif

class FiddleContextGLPLS : public FiddleContextGL
{
public:
    FiddleContextGLPLS()
    {
        if (!m_plsContext)
        {
            fprintf(stderr, "Failed to create a PLS renderer.\n");
            exit(-1);
        }
    }

    std::unique_ptr<rive::Factory> makeFactory() override { return std::make_unique<PLSFactory>(); }

    void onSizeChanged(int width, int height) override
    {
        auto glContext = static_cast<PLSRenderContextGLImpl*>(m_plsContext->impl());
        m_renderTarget = glContext->makeOffscreenRenderTarget(width, height);
    }

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<PLSRenderer>(m_plsContext.get());
    }

    void begin() override
    {
        PLSRenderContext::FrameDescriptor frameDescriptor;
        frameDescriptor.renderTarget = m_renderTarget;
        frameDescriptor.clearColor = 0xff404040;
        frameDescriptor.wireframe = g_wireframe;
        frameDescriptor.fillsDisabled = g_disableFill;
        frameDescriptor.strokesDisabled = g_disableStroke;
        m_plsContext->beginFrame(std::move(frameDescriptor));
    }

    void onEnd() override
    {
        m_plsContext->flush();
        auto [w, h] = std::make_tuple(m_renderTarget->width(), m_renderTarget->height());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_renderTarget->sideFramebufferID());
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    void shrinkGPUResourcesToFit() final { m_plsContext->shrinkGPUResourcesToFit(); }

private:
    std::unique_ptr<PLSRenderContext> m_plsContext =
        std::make_unique<PLSRenderContext>(PLSRenderContextGLImpl::Make());
    rcp<PLSRenderTargetGL> m_renderTarget;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeGLPLS()
{
    return std::make_unique<FiddleContextGLPLS>();
}

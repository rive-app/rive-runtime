#include "fiddle_context.hpp"

#ifdef RIVE_TOOLS_NO_GLFW

std::unique_ptr<FiddleContext> FiddleContext::MakeGLPLS(FiddleContextOptions)
{
    return nullptr;
}

#else

#include "path_fiddle.hpp"
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
        // Don't abort if it's a shader compile error; let our internal handlers
        // print the source (for debugging) and exit on their own.
        if (!strstr(message, "SHADER_ID_COMPILE error has been generated"))
        {
            assert(0);
        }
    }
    else if (type == GL_DEBUG_TYPE_PERFORMANCE)
    {
        if (strcmp(message,
                   "API_ID_REDUNDANT_FBO performance warning has been "
                   "generated. Redundant state "
                   "change in glBindFramebuffer API call, FBO 0, \"\", already "
                   "bound.") == 0)
        {
            return;
        }
        if (strstr(message, "is being recompiled based on GL state"))
        {
            return;
        }
        if (strcmp(message,
                   "Pixel-path performance warning: Pixel transfer is "
                   "synchronized with 3D "
                   "rendering.") == 0)
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
    FiddleContextGL(FiddleContextOptions options)
    {
#ifdef RIVE_DESKTOP_GL
        // Load the OpenGL API using glad.
        if (!gladLoadCustomLoader((GLADloadproc)glfwGetProcAddress))
        {
            fprintf(stderr, "Failed to initialize glad.\n");
            abort();
        }
#endif

        printf("==== GL GPU: %s ====\n", glGetString(GL_RENDERER));
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
            glDebugMessageControlKHR(GL_DONT_CARE,
                                     GL_DONT_CARE,
                                     GL_DONT_CARE,
                                     0,
                                     NULL,
                                     GL_TRUE);
            glDebugMessageCallbackKHR(&err_msg_callback, nullptr);
        }
#endif
#endif

        m_renderContext = RenderContextGLImpl::MakeContext({
            .disableFragmentShaderInterlock = options.disableRasterOrdering,
        });
        if (!m_renderContext)
        {
            fprintf(stderr, "Failed to create a RiveRenderContext for GL.\n");
            abort();
        }
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
            glTexStorage2D(GL_TEXTURE_2D,
                           1,
                           GL_RGB8,
                           kZoomWindowWidth,
                           kZoomWindowHeight);

            glGenFramebuffers(1, &m_zoomWindowFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, m_zoomWindowFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D,
                                   tex,
                                   0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glDeleteTextures(1, &tex);
        }
    }

    rive::Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContextOrNull() override
    {
        return m_renderContext.get();
    }

    rive::gpu::RenderTarget* renderTargetOrNull() override
    {
        return m_renderTarget.get();
    }

    void onSizeChanged(GLFWwindow* window,
                       int width,
                       int height,
                       uint32_t sampleCount) override
    {
        m_renderTarget =
            make_rcp<FramebufferRenderTargetGL>(width, height, 0, sampleCount);
        glViewport(0, 0, width, height);
    }

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void begin(const RenderContext::FrameDescriptor& frameDescriptor) override
    {
        m_renderContext->static_impl_cast<RenderContextGLImpl>()
            ->invalidateGLState();
        m_renderContext->beginFrame(frameDescriptor);
    }

    void flushPLSContext() final
    {
        m_renderContext->flush({.renderTarget = m_renderTarget.get()});
    }

    void end(GLFWwindow* window, std::vector<uint8_t>* pixelData) final
    {
        flushPLSContext();
        m_renderContext->static_impl_cast<RenderContextGLImpl>()
            ->unbindGLInternalResources();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (pixelData)
        {
            pixelData->resize(m_renderTarget->height() *
                              m_renderTarget->width() * 4);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glReadPixels(0,
                         0,
                         m_renderTarget->width(),
                         m_renderTarget->height(),
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         pixelData->data());
        }
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
            glClearColor(.6f, .6f, .6f, 1);
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

private:
    GLuint m_zoomWindowFBO = 0;
    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetGL> m_renderTarget;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeGLPLS(
    FiddleContextOptions options)
{
    return std::make_unique<FiddleContextGL>(options);
}

#endif

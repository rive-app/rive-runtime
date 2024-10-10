/*
 * Copyright 2022 Rive
 */

#include "testing_window.hpp"

#ifdef RIVE_TOOLS_NO_GL

TestingWindow* TestingWindow::MakeEGL(Backend backend, void* platformWindow) { return nullptr; }

#else

#include "rive/renderer.hpp"
#include "testing_gl_renderer.hpp"
#include <cassert>
#include <stdio.h>

#define EGL_EGL_PROTOTYPES 0
#include <EGL/egl.h>
#include <EGL/eglext.h>

// Include after Windows headers due to conflicts with #defines.
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"

#if defined(_WIN32)

#include <libloaderapi.h>

using PlatformLibraryHandle = HMODULE;
using PlatformProcAddress = FARPROC;

static PlatformLibraryHandle platform_dlopen(const char* libname)
{
    PlatformLibraryHandle lib = LoadLibraryA(libname);
    if (!lib)
    {
        WCHAR err[512];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      0,
                      GetLastError(),
                      0,
                      err,
                      512,
                      0);
        fprintf(stderr, "Failed load %s. %ws\n", libname, err);
    }
    return lib;
}

static PlatformProcAddress platform_dlsym(PlatformLibraryHandle lib, const char* symbol)
{

    return GetProcAddress(lib, symbol);
}

#else

#include <dlfcn.h>

using PlatformLibraryHandle = void*;
using PlatformProcAddress = void*;

static PlatformLibraryHandle platform_dlopen(const char* libname)
{
    std::string filename = libname;
#ifdef __APPLE__
    filename.append(".dylib");
#else
    filename.append(".so");
#endif
    PlatformLibraryHandle lib = dlopen(filename.c_str(), RTLD_LAZY);
    if (!lib)
    {
        fprintf(stderr, "Failed load %s. %s\n", filename.c_str(), dlerror());
    }
    return lib;
}

static PlatformProcAddress platform_dlsym(PlatformLibraryHandle lib, const char* symbol)
{
    return dlsym(lib, symbol);
}

#endif

static PlatformLibraryHandle s_LibEGL = nullptr;

#define GET_EGL_PROC(eglProc)                                                                      \
    eglProc = (decltype(eglProc))platform_dlsym(s_LibEGL, #eglProc);                               \
    if (!eglProc)                                                                                  \
    {                                                                                              \
        fprintf(stderr, "Failed locate " #eglProc " in libEGL.\n");                                \
        abort();                                                                                   \
    }

static PFNEGLGETDISPLAYPROC eglGetDisplay;
static PFNEGLQUERYSTRINGPROC eglQueryString;
static PFNEGLINITIALIZEPROC eglInitialize;
static PFNEGLCHOOSECONFIGPROC eglChooseConfig;
static PFNEGLCREATECONTEXTPROC eglCreateContext;
static PFNEGLDESTROYCONTEXTPROC eglDestroyContext;
static PFNEGLCREATEPBUFFERSURFACEPROC eglCreatePbufferSurface;
static PFNEGLCREATEWINDOWSURFACEPROC eglCreateWindowSurface;
static PFNEGLQUERYSURFACEPROC eglQuerySurface;
static PFNEGLDESTROYSURFACEPROC eglDestroySurface;
static PFNEGLMAKECURRENTPROC eglMakeCurrent;
static PFNEGLSWAPBUFFERSPROC eglSwapBuffers;
static PFNEGLGETPROCADDRESSPROC eglGetProcAddress;

static void init_egl()
{
    if (s_LibEGL)
    {
        return;
    }

    s_LibEGL = platform_dlopen("libEGL");
    if (!s_LibEGL)
    {
        fprintf(stderr,
                "Failed load libEGL. Did you build ANGLE and copy over libEGL and libGLESv2?\n");
        abort();
    }

    GET_EGL_PROC(eglGetDisplay);
    GET_EGL_PROC(eglQueryString);
    GET_EGL_PROC(eglInitialize);
    GET_EGL_PROC(eglChooseConfig);
    GET_EGL_PROC(eglCreateContext);
    GET_EGL_PROC(eglDestroyContext);
    GET_EGL_PROC(eglCreatePbufferSurface);
    GET_EGL_PROC(eglCreateWindowSurface);
    GET_EGL_PROC(eglQuerySurface);
    GET_EGL_PROC(eglDestroySurface);
    GET_EGL_PROC(eglMakeCurrent);
    GET_EGL_PROC(eglSwapBuffers);
    GET_EGL_PROC(eglGetProcAddress);
}

#ifdef DEBUG
static void GL_APIENTRY err_msg_callback(GLenum source,
                                         GLenum type,
                                         GLuint id,
                                         GLenum severity,
                                         GLsizei length,
                                         const GLchar* message,
                                         const void* userParam)
{
    if (type == GL_DEBUG_TYPE_ERROR_KHR)
    {
        printf("GL ERROR: %s\n", message);
        fflush(stdout);
        abort();
    }
    else if (type == GL_DEBUG_TYPE_PERFORMANCE_KHR)
    {
        if (strcmp(message, "CPU path taken to copy PBO") == 0 ||
            strcmp(message, "EsxBufferObject::Map - Ignoring EsxBufferMapUnsyncedBit") == 0 ||
            strcmp(message, "Packing allocations for resource %p") == 0)
        {
            return;
        }
        printf("GL PERF: %s\n", message);
    }
}
#endif

// Use an offscreen, platform independent window.
class EGLWindow
{
public:
    virtual ~EGLWindow() { deleteSurface(); }

    operator EGLSurface() const { return m_surface; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    virtual bool isOffscreen() const = 0;

    virtual void resize(int width, int height) = 0;

protected:
    EGLWindow(EGLDisplay display, EGLConfig config) : m_display(display), m_config(config) {}

    void deleteSurface()
    {
        if (m_surface && !eglDestroySurface(m_display, m_surface))
        {
            fprintf(stderr, "eglDestroySurface failed.\n");
            abort();
        }
    }

    const EGLDisplay m_display;
    const EGLConfig m_config;
    void* m_surface = nullptr;
    int m_width = 0;
    int m_height = 0;
};

class PbufferWindow : public EGLWindow
{
public:
    PbufferWindow(EGLDisplay display, EGLConfig config) : EGLWindow(display, config)
    {
        resize(1, 1);
    }

    bool isOffscreen() const final { return true; }

    void resize(int width, int height) final
    {
        deleteSurface();
        const EGLint surfaceAttribs[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
        m_surface = eglCreatePbufferSurface(m_display, m_config, surfaceAttribs);
        if (!m_surface)
        {
            fprintf(stderr, "eglCreatePbufferSurface failed.\n");
            abort();
        }
        m_width = width;
        m_height = height;
    }
};

class NativeWindow : public EGLWindow
{
public:
    NativeWindow(EGLDisplay display, EGLConfig config, void* platformWindow) :
        EGLWindow(display, config)
    {
        m_surface = eglCreateWindowSurface(m_display,
                                           m_config,
                                           reinterpret_cast<EGLNativeWindowType>(platformWindow),
                                           nullptr);
        if (!m_surface)
        {
            fprintf(stderr, "eglCreateWindowSurface failed.\n");
            abort();
        }
        EGLint eglInt;
        eglQuerySurface(m_display, m_surface, EGL_WIDTH, &eglInt);
        m_width = eglInt;
        eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &eglInt);
        m_height = eglInt;
    }

    bool isOffscreen() const final { return true; }

    void resize(int width, int height) final
    {
        fprintf(stderr, "NativeWindow::resize unsupported.\n");
        abort();
    }
};

class TestingWindowEGL : public TestingWindow
{
public:
    TestingWindowEGL(EGLint angleBackend,
                     int samples,
                     std::unique_ptr<TestingGLRenderer> renderer,
                     void* platformWindow) :
        m_renderer(std::move(renderer)), m_isMSAA(samples > 0)
    {
        init_egl();

#ifdef RIVE_DESKTOP_GL
        if (angleBackend != EGL_NONE)
        {
            PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
            GET_EGL_PROC(eglGetPlatformDisplayEXT);

            const EGLint displayAttribs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, angleBackend, EGL_NONE};
            m_Display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
                                                 reinterpret_cast<void*>(EGL_DEFAULT_DISPLAY),
                                                 displayAttribs);
            if (!m_Display)
            {
                fprintf(stderr, "eglGetPlatformDisplayEXT failed.\n");
                abort();
            }
        }
        else
#endif
        {
            assert(angleBackend == EGL_NONE);
            m_Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (!m_Display)
            {
                fprintf(stderr, "eglGetDisplay failed.\n");
                abort();
            }
        }

        EGLint majorVersion;
        EGLint minorVersion;
        if (!eglInitialize(m_Display, &majorVersion, &minorVersion))
        {
            fprintf(stderr, "eglInitialize failed.\n");
            abort();
        }
        // printf("Initialized EGL version %i.%i\n", majorVersion, minorVersion);
        // printf("EGL_VENDOR: %s\n", eglQueryString(m_Display, EGL_VENDOR));
        // printf("EGL_VERSION: %s\n", eglQueryString(m_Display, EGL_VERSION));
        if (angleBackend != EGL_NONE && !strstr(eglQueryString(m_Display, EGL_VERSION), "ANGLE"))
        {
            fprintf(stderr,
                    "libEGL does not appear to be ANGLE. Did you build ANGLE and copy over libEGL "
                    "and libGLESv2?\n");
            abort();
        }

        EGLint numConfigs;
        const EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                        EGL_PBUFFER_BIT,
                                        EGL_RENDERABLE_TYPE,
                                        EGL_OPENGL_ES3_BIT,
                                        EGL_COLOR_BUFFER_TYPE,
                                        EGL_RGB_BUFFER,
                                        EGL_RED_SIZE,
                                        8,
                                        EGL_GREEN_SIZE,
                                        8,
                                        EGL_BLUE_SIZE,
                                        8,
                                        EGL_ALPHA_SIZE,
                                        8,
                                        EGL_DEPTH_SIZE,
                                        samples == 0 ? 0 : 24,
                                        EGL_STENCIL_SIZE,
                                        samples == 0 ? 0 : 8,
                                        EGL_SAMPLE_BUFFERS,
                                        samples == 0 ? 0 : 1,
                                        EGL_SAMPLES,
                                        samples,
                                        EGL_NONE};

        EGLConfig config;
        if (!eglChooseConfig(m_Display, configAttribs, &config, 1, &numConfigs))
        {
            fprintf(stderr, "eglChooseConfig failed.\n");
            abort();
        }

        // Create an ES 3.0 context. This very closely mirrors webgl2.
        const EGLint contextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION,
                                         3,
                                         EGL_CONTEXT_MINOR_VERSION,
                                         0,
                                         EGL_NONE};
        m_Context = eglCreateContext(m_Display, config, nullptr, contextAttribs);
        if (m_Context == EGL_NO_CONTEXT)
        {
            fprintf(stderr, "eglCreateContext failed.\n");
            abort();
        }

        if (platformWindow != nullptr)
        {
            m_window = std::make_unique<NativeWindow>(m_Display, config, platformWindow);
        }
        else
        {
            m_window = std::make_unique<PbufferWindow>(m_Display, config);
        }
        m_width = m_window->width();
        m_height = m_window->height();
        if (!eglMakeCurrent(m_Display, *m_window, *m_window, m_Context))
        {
            fprintf(stderr, "eglMakeCurrent failed.\n");
            abort();
        }

#ifdef RIVE_DESKTOP_GL
        // Load the OpenGL API using glad.
        if (!gladLoadCustomLoader((GLADloadproc)eglGetProcAddress))
        {
            fprintf(stderr, "Failed to initialize glad.\n");
            abort();
        }
#endif

        printf("==== EGL GPU: OpenGL %s %s ====\n",
               glGetString(GL_VENDOR),
               glGetString(GL_RENDERER));

        int extensionCount;
        glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
#ifdef DEBUG
        for (int i = 0; i < extensionCount; ++i)
        {
            auto* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
            if (strcmp(ext, "GL_KHR_debug") == 0)
            {
                glEnable(GL_DEBUG_OUTPUT_KHR);
                glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);

                auto glDebugMessageControlKHR = reinterpret_cast<PFNGLDEBUGMESSAGECONTROLKHRPROC>(
                    eglGetProcAddress("glDebugMessageControlKHR"));
                assert(glDebugMessageControlKHR);
                glDebugMessageControlKHR(GL_DONT_CARE,
                                         GL_DONT_CARE,
                                         GL_DONT_CARE,
                                         0,
                                         NULL,
                                         GL_TRUE);

                auto glDebugMessageCallbackKHR = reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKKHRPROC>(
                    eglGetProcAddress("glDebugMessageCallbackKHR"));
                assert(glDebugMessageCallbackKHR);
                glDebugMessageCallbackKHR(&err_msg_callback, nullptr);

                // printf("%s enabled.\n", ext);
                continue;
            }
        }
#endif

        m_renderer->init(reinterpret_cast<void*>(eglGetProcAddress));
    }

    ~TestingWindowEGL()
    {
        eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_Context)
        {
            eglDestroyContext(m_Display, m_Context);
        }
    }

    rive::Factory* factory() override { return m_renderer->factory(); }

    void resize(int width, int height) override
    {
        if (width == m_width && height == m_height)
        {
            return;
        }

        if (!m_isMSAA && renderContextGLImpl()->capabilities().EXT_shader_pixel_local_storage &&
            m_window->isOffscreen())
        {
            // ARM Mali GPUs seem to hang while rendering goldens to a Pbuffer with
            // EXT_shader_pixel_local_storage. Rendering to a texture instead seems
            // to work.
            m_headlessRenderTexture = glutils::Texture();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_headlessRenderTexture);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
        }
        else
        {
            m_window->resize(width, height);
            if (!eglMakeCurrent(m_Display, *m_window, *m_window, m_Context))
            {
                fprintf(stderr, "eglMakeCurrent failed.\n");
                abort();
            }
        }

        glViewport(0, 0, width, height);
        m_width = width;
        m_height = height;
    }

    std::unique_ptr<rive::Renderer> beginFrame(uint32_t clearColor, bool doClear) override
    {
        auto renderer = m_renderer->reset(m_width, m_height, m_headlessRenderTexture);
        m_renderer->beginFrame(clearColor, doClear);
        return renderer;
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        m_renderer->flush();
        if (pixelData)
        {
            pixelData->resize(m_height * m_width * 4);
            static_cast<rive::gpu::RenderTargetGL*>(m_renderer->renderTarget())
                ->bindDestinationFramebuffer(GL_READ_FRAMEBUFFER);
            glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, pixelData->data());
        }
        eglSwapBuffers(m_Display, *m_window);
    }

    rive::gpu::RenderContext* renderContext() const override { return m_renderer->renderContext(); }

    rive::gpu::RenderContextGLImpl* renderContextGLImpl() const override
    {
        if (auto ctx = renderContext())
        {
            return ctx->static_impl_cast<rive::gpu::RenderContextGLImpl>();
        }
        return nullptr;
    }

    rive::gpu::RenderTarget* renderTarget() const override { return m_renderer->renderTarget(); }

    void flushPLSContext() override { m_renderer->flushPLSContext(); }

private:
    const std::unique_ptr<TestingGLRenderer> m_renderer;
    const bool m_isMSAA = 0;
    void* m_Display;
    void* m_Context;
    std::unique_ptr<EGLWindow> m_window;
    glutils::Texture m_headlessRenderTexture = glutils::Texture::Zero();
};

TestingWindow* TestingWindow::MakeEGL(Backend backend, void* platformWindow)
{
    auto rendererFlags = RendererFlags::none;
    EGLint angleBackend = EGL_NONE;
    int samples = 0;
    switch (backend)
    {
        case Backend::glatomic:
            rendererFlags |= RendererFlags::disableRasterOrdering;
            break;
        case Backend::glmsaa:
            rendererFlags |= RendererFlags::useMSAA;
#ifdef RIVE_ANDROID
            samples = 0; // Test EXT_multisampled_render_to_texture on Android.
#else
            samples = 4;
#endif
            break;
        case Backend::gl:
            break;
        case Backend::anglemsaa:
            rendererFlags |= RendererFlags::useMSAA;
            samples = 0; // Test our offscreen MSAA path on ANGLE.
        case Backend::angle:
#ifdef __APPLE__
            angleBackend = EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE;
#elif defined(_WIN32)
            angleBackend = EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE;
#else
            angleBackend = EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
#endif
            break;
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
        case Backend::rhi:
            printf("Invalid backend for TestingWindow::MakeEGLPbuffer.");
            abort();
            break;
    }
    return new TestingWindowEGL(angleBackend,
                                samples,
                                TestingGLRenderer::Make(rendererFlags),
                                platformWindow);
}

#endif

/*
 * Copyright 2023 Rive
 */

#include "rive/math/simd.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/layout.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"

#include <cmath>
#include <iterator>
#include <vector>
#include <filesystem>

#include "marty.h"
#include "egg_v2.h"
#include "rope.h"

#ifdef RIVE_WEBGPU
#include "../src/webgpu/webgpu_compat.h"
#include <webgpu/webgpu_cpp.h>
#include <emscripten.h>
#include <emscripten/html5_webgpu.h>
#include <emscripten/html5.h>
#else
#include <dawn/webgpu_cpp.h>
#include "dawn/native/DawnNative.h"
#include "dawn/dawn_proc.h"
#define EMSCRIPTEN_KEEPALIVE
#endif

#ifdef RIVE_WAGYU
#include "../src/webgpu/webgpu_wagyu.h"
#endif

using namespace rive;
using namespace rive::gpu;
using PixelLocalStorageType = RenderContextWebGPUImpl::PixelLocalStorageType;

#ifdef RIVE_WEBGPU
static std::unique_ptr<RenderContext> renderContext;
static rcp<RenderTargetWebGPU> renderTarget;
static std::unique_ptr<Renderer> renderer;

static WGPUInstance instance;
static WGPUAdapter adapter;
static wgpu::Device device;
static WGPUSurface surface;
static WGPUTextureFormat format = WGPUTextureFormat_Undefined;
static wgpu::Queue queue;

static std::unique_ptr<File> rivFile;
static std::unique_ptr<ArtboardInstance> artboard;
static std::unique_ptr<Scene> scene;

extern "C" EM_BOOL animationFrame(double time, void* userData);
extern "C" void start(void);

static void requestDeviceCallback(WGPURequestDeviceStatus status,
                                  WGPUDevice deviceArg,
                                  const char* message,
                                  void* userdata);
static void requestAdapterCallback(WGPURequestAdapterStatus status,
                                   WGPUAdapter adapterArg,
                                   const char* message,
                                   void* userdata);

void requestDeviceCallback(WGPURequestDeviceStatus status,
                           WGPUDevice deviceArg,
                           const char* message,
                           void* userdata)
{
    assert(userdata == instance);

    device = wgpu::Device::Acquire(deviceArg);
    assert(device.Get());

    queue = device.GetQueue();
    assert(queue.Get());

    RenderContextWebGPUImpl::ContextOptions contextOptions;
#ifdef RIVE_WAGYU
    WGPUBackendType backend = wgpuWagyuAdapterGetBackend(adapter);
    if (backend == WGPUBackendType_Vulkan)
    {
        WGPUWagyuStringArray deviceExtensions = WGPU_WAGYU_STRING_ARRAY_INIT;
        wgpuWagyuDeviceGetExtensions(device.Get(), &deviceExtensions);
        for (size_t i = 0; i < deviceExtensions.stringCount; i++)
        {
            if (backend == WGPUBackendType_Vulkan &&
                !strcmp(deviceExtensions.strings[i].data,
                        "VK_EXT_rasterization_order_attachment_access"))
            {
                contextOptions.plsType = PixelLocalStorageType::subpassLoad;
                break;
            }
        }
    }
    else if (backend == WGPUBackendType_OpenGLES)
    {
        // TODO: search for "GL_EXT_shader_pixel_local_storage".
        // wgpuWagyuDeviceGetExtensions currently returns nothing in the GL
        // backend.
        contextOptions.plsType =
            PixelLocalStorageType::EXT_shader_pixel_local_storage;
        // TODO: Disable storage buffers if the hardware doesn't support 4 in
        // the vertex shader:
        // contextOptions.disableStorageBuffers =
        //     GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS < 4;
    }
#endif
    renderContext =
        RenderContextWebGPUImpl::MakeContext(device, queue, contextOptions);
    renderTarget =
        renderContext->static_impl_cast<RenderContextWebGPUImpl>()
            ->makeRenderTarget(wgpu::TextureFormat::RGBA8Unorm, 1920, 1080);
    renderer = std::make_unique<RiveRenderer>(renderContext.get());

    rivFile = File::import({marty, marty_len}, renderContext.get());
    // rivFile = File::import({egg_v2, egg_v2_len}, renderContext.get());
    // rivFile = File::import({rope, rope_len}, renderContext.get());
    artboard = rivFile->artboardDefault();
    scene = artboard->defaultScene();
    scene->advanceAndApply(0);

    {
        WGPUSurfaceDescriptorFromCanvasHTMLSelector htmlSelector = {};
        htmlSelector.chain.sType =
            WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
        htmlSelector.selector = "#canvas";

        WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
        surfaceDesc.nextInChain = (WGPUChainedStruct*)&htmlSelector;

        surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);
        assert(surface);
    }

    {
        format = wgpuSurfaceGetPreferredFormat(surface, adapter);
        assert(format);
    }

    {
        WGPUSurfaceConfiguration conf = WGPU_SURFACE_CONFIGURATION_INIT;
        conf.device = device.Get();
        conf.format = format;

        wgpuSurfaceConfigure(surface, &conf);
    }

    emscripten_set_canvas_element_size("#canvas", 1920, 1080);

    emscripten_request_animation_frame_loop(animationFrame, (void*)100);
}

void requestAdapterCallback(WGPURequestAdapterStatus status,
                            WGPUAdapter adapterArg,
                            const char* message,
                            void* userdata)
{
    assert(adapterArg);
    assert(status == WGPURequestAdapterStatus_Success);
    assert(userdata == instance);
    adapter = adapterArg;

    WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;

    wgpuAdapterRequestDevice(adapter,
                             &descriptor,
                             requestDeviceCallback,
                             userdata);
}

static double lastTime = 0;
extern "C" EM_BOOL animationFrame(double time, void* userData)
{
    (void)time;
    (void)userData;

    WGPUSurfaceTexture surfaceTexture = WGPU_SURFACE_TEXTURE_INIT;

    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

    WGPUTexture texture = surfaceTexture.texture;

    WGPUTextureViewDescriptor textureViewDesc =
        WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    textureViewDesc.format = format;
    textureViewDesc.dimension = WGPUTextureViewDimension_2D;

    WGPUTextureView textureView =
        wgpuTextureCreateView(texture, &textureViewDesc);

    scene->advanceAndApply(lastTime == 0 ? 0 : (time - lastTime) * 1e-3f);
    lastTime = time;

    renderContext->beginFrame({
        .renderTargetWidth = renderTarget->width(),
        .renderTargetHeight = renderTarget->height(),
        .loadAction = gpu::LoadAction::clear,
        .clearColor = 0xff8030ff,
    });

    renderer->save();
    renderer->transform(computeAlignment(rive::Fit::contain,
                                         rive::Alignment::center,
                                         rive::AABB(0, 0, 1920, 1080),
                                         artboard->bounds()));
    scene->draw(renderer.get());
    renderer->restore();

    renderTarget->setTargetTextureView(textureView);
    renderContext->flush({.renderTarget = renderTarget.get()});

    wgpuTextureViewRelease(textureView);
    wgpuTextureRelease(texture);

    return EM_TRUE;
}

int main(void)
{
    instance = wgpuCreateInstance(NULL);
    assert(instance);

    wgpuInstanceRequestAdapter(instance,
                               NULL,
                               requestAdapterCallback,
                               instance);

    return 0;
}

extern "C" void start(void) { main(); }

#endif

#ifdef RIVE_DAWN
#include "../path_fiddle/fiddle_context.hpp"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <fstream>

static GLFWwindow* window = nullptr;
static std::unique_ptr<FiddleContext> fiddleContextDawn;

static void glfw_error_callback(int code, const char* message)
{
    printf("GLFW error: %i - %s\n", code, message);
}

int main(int argc, const char** argv)
{
    // Cause stdout and stderr to print immediately without buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize glfw.\n");
        return 1;
    }

    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    window = glfwCreateWindow(1600, 1600, "Rive Renderer", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "Failed to create window.\n");
        return -1;
    }
    glfwSetWindowTitle(window, "Rive Renderer");

    std::unique_ptr<FiddleContext> fiddleContextDawn =
        FiddleContext::MakeDawnPLS({});

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    fiddleContextDawn->onSizeChanged(window, w, h, 1);
    std::unique_ptr<Renderer> renderer = fiddleContextDawn->makeRenderer(w, h);

    const char* filename = argc > 1 ? argv[1] : "webgpu_player/rivs/tape.riv";
    std::ifstream rivStream(filename, std::ios::binary);
    std::vector<uint8_t> rivBytes(std::istreambuf_iterator<char>(rivStream),
                                  {});
    std::unique_ptr<File> rivFile =
        File::import(rivBytes, fiddleContextDawn->factory());
    std::unique_ptr<ArtboardInstance> artboard = rivFile->artboardDefault();
    std::unique_ptr<Scene> scene = artboard->defaultScene();
    scene->advanceAndApply(0);

    double lastTimestamp = 0;

    while (!glfwWindowShouldClose(window))
    {
        double timestamp = glfwGetTime();

        fiddleContextDawn->begin({
            .renderTargetWidth = static_cast<uint32_t>(w),
            .renderTargetHeight = static_cast<uint32_t>(h),
            .clearColor = 0xff8030ff,
        });

        renderer->save();
        renderer->transform(computeAlignment(rive::Fit::contain,
                                             rive::Alignment::center,
                                             rive::AABB(0, 0, w, h),
                                             artboard->bounds()));
        scene->draw(renderer.get());
        renderer->restore();

        fiddleContextDawn->end(window);
        fiddleContextDawn->tick();
        glfwPollEvents();

        if (lastTimestamp != 0)
        {
            scene->advanceAndApply(timestamp - lastTimestamp);
        }
        lastTimestamp = timestamp;
    }
    glfwTerminate();
    return 0;
}
#endif

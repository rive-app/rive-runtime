/*
 * Copyright 2023 Rive
 */

#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/refcnt.hpp"
#include "rive/layout.hpp"
#include "rive/animation/state_machine_instance.hpp"

#include <iterator>
#include <vector>

using namespace rive;

#ifdef RIVE_WEBGPU

#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"

#if RIVE_WEBGPU == 1
#include "../src/webgpu/webgpu_compat.h"
#endif
#include "marty.h"
#include "egg_v2.h"
#include "rope.h"

#include <webgpu/webgpu_cpp.h>
#include <emscripten.h>
#include <emscripten/html5.h>

using namespace rive::gpu;

static std::unique_ptr<RenderContext> renderContext;
static rcp<RenderTargetWebGPU> renderTarget;
static std::unique_ptr<Renderer> renderer;

static WGPUInstance instance;
static wgpu::Adapter adapter;
static wgpu::Device device;
static WGPUSurface surface;
static WGPUTextureFormat format = WGPUTextureFormat_Undefined;
static wgpu::Queue queue;

static rcp<File> rivFile;
static std::unique_ptr<ArtboardInstance> artboard;
static std::unique_ptr<Scene> scene;

extern "C" EM_BOOL animationFrame(double time, void* userData);
EM_JS(uint8_t, get_riv_buffer, (uint8_t** buffer, uint32_t* len), {
    if (!get_wagyu_buffer)
        return 0;
    const arrBuf = get_wagyu_buffer();
    if (!arrBuf)
        return 0;

    const arr = new Uint8Array(arrBuf), ptr = _malloc(arr.length);
    HEAPU8.set(arr, ptr);
    setValue(buffer, ptr, '*');
    setValue(len, arr.length, 'i32');
    return 1;
});

#if RIVE_WEBGPU > 1
void requestDeviceCallback(wgpu::RequestDeviceStatus status,
                           wgpu::Device deviceArg,
                           const char* message,
                           void* userdata)

#else
void requestDeviceCallback(WGPURequestDeviceStatus status,
                           WGPUDevice deviceArg,
                           const char* message,
                           void* userdata)
#endif
{
    assert(userdata == instance);

#if RIVE_WEBGPU > 1
    device = deviceArg;
#else
    device = wgpu::Device::Acquire(deviceArg);
#endif
    assert(device.Get());

    queue = device.GetQueue();
    assert(queue.Get());

    {
#if RIVE_WEBGPU > 1
        WGPUEmscriptenSurfaceSourceCanvasHTMLSelector htmlSelector =
            WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
        htmlSelector.selector.data = "#canvas";
        htmlSelector.selector.length = 7;
#else
        WGPUSurfaceDescriptorFromCanvasHTMLSelector htmlSelector = {};
        htmlSelector.chain.sType =
            WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
        htmlSelector.selector = "#canvas";
#endif

        WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
        surfaceDesc.nextInChain = (WGPUChainedStruct*)&htmlSelector;

        surface = wgpuInstanceCreateSurface(instance, &surfaceDesc);
        assert(surface);
    }

    {
#if RIVE_WEBGPU > 1
        WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
        wgpuSurfaceGetCapabilities(surface, adapter.Get(), &capabilities);
        assert(capabilities.formatCount > 0);
        format = capabilities.formats[0];
        wgpuSurfaceCapabilitiesFreeMembers(capabilities);
#else
        format = wgpuSurfaceGetPreferredFormat(surface, adapter.Get());
#endif
        assert(format);
    }

    {
        WGPUSurfaceConfiguration conf = WGPU_SURFACE_CONFIGURATION_INIT;
        conf.device = device.Get();
        conf.format = format;

        wgpuSurfaceConfigure(surface, &conf);
    }

    RenderContextWebGPUImpl::ContextOptions contextOptions;
    renderContext = RenderContextWebGPUImpl::MakeContext(adapter,
                                                         device,
                                                         queue,
                                                         contextOptions);
    renderTarget =
        renderContext->static_impl_cast<RenderContextWebGPUImpl>()
            ->makeRenderTarget(static_cast<wgpu::TextureFormat>(format),
                               1920,
                               1080);
    renderer = std::make_unique<RiveRenderer>(renderContext.get());

    uint8_t* buffer;
    uint32_t buffer_len;
    if (get_riv_buffer(&buffer, &buffer_len))
    {
        rivFile = File::import({buffer, buffer_len}, renderContext.get());
        free(buffer);
    }
    if (!rivFile)
    {
        rivFile = File::import({marty, marty_len}, renderContext.get());
        // rivFile = File::import({egg_v2, egg_v2_len}, renderContext.get());
        // rivFile = File::import({rope, rope_len}, renderContext.get());
    }
    artboard = rivFile->artboardDefault();
    scene = artboard->defaultScene();
    scene->advanceAndApply(0);

    emscripten_set_canvas_element_size("#canvas", 1920, 1080);

    emscripten_request_animation_frame_loop(animationFrame, (void*)100);
}

#if RIVE_WEBGPU > 1
void requestAdapterCallback(WGPURequestAdapterStatus status,
                            WGPUAdapter adapterArg,
                            WGPUStringView message,
                            void* userdata,
                            void* /*userdata2*/)
#else
void requestAdapterCallback(WGPURequestAdapterStatus status,
                            WGPUAdapter adapterArg,
                            const char* message,
                            void* userdata)
#endif
{
    assert(adapterArg);
    assert(status == WGPURequestAdapterStatus_Success);
    assert(userdata == instance);
    adapter = wgpu::Adapter::Acquire(adapterArg);
#if RIVE_WEBGPU > 1
    adapter.RequestDevice({},
                          wgpu::CallbackMode::AllowSpontaneous,
                          requestDeviceCallback,
                          userdata);
#else
    adapter.RequestDevice({}, requestDeviceCallback, userdata);
#endif
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
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    renderContext->flush({
        .renderTarget = renderTarget.get(),
        .externalCommandBuffer = encoder.Get(),
    });
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    wgpuTextureViewRelease(textureView);
    wgpuTextureRelease(texture);

    return EM_TRUE;
}

int main(void)
{
    instance = wgpuCreateInstance(NULL);
    assert(instance);

#if RIVE_WEBGPU > 1
    WGPURequestAdapterCallbackInfo requestAdapterCallbackInfo =
        WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    requestAdapterCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    requestAdapterCallbackInfo.callback = requestAdapterCallback;
    requestAdapterCallbackInfo.userdata1 = instance;
    wgpuInstanceRequestAdapter(instance, NULL, requestAdapterCallbackInfo);
#else
    wgpuInstanceRequestAdapter(instance,
                               NULL,
                               requestAdapterCallback,
                               instance);
#endif

    return 0;
}

#endif

#ifdef RIVE_DAWN

#include "../path_fiddle/fiddle_context.hpp"

#include <dawn/webgpu_cpp.h>
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

    const char* filename =
        argc > 1 ? argv[1] : "webgpu_player/rivs/poison_loader.riv";
    std::ifstream rivStream(filename, std::ios::binary);
    std::vector<uint8_t> rivBytes(std::istreambuf_iterator<char>(rivStream),
                                  {});
    rcp<File> rivFile = File::import(rivBytes, fiddleContextDawn->factory());
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

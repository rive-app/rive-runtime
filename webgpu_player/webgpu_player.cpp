/*
 * Copyright 2023 Rive
 */

#include "rive/math/simd.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/layout.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/pls/pls_render_context.hpp"
#include "rive/pls/pls_renderer.hpp"
#include "rive/pls/webgpu/pls_render_context_webgpu_impl.hpp"
#include "rive/pls/webgpu/em_js_handle.hpp"

#include <cmath>
#include <iterator>
#include <vector>
#include <filesystem>

#ifdef RIVE_WEBGPU
#include <webgpu/webgpu_cpp.h>
#include <emscripten.h>
#include <emscripten/html5_webgpu.h>
#else
#include <dawn/webgpu_cpp.h>
#include "dawn/native/DawnNative.h"
#include "dawn/dawn_proc.h"
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "../out/obj/generated/marty.riv.c"
// #include "../out/obj/generated/Knight_square_2.riv.c"
// #include "../out/obj/generated/Rope.riv.c"

using namespace rive;
using namespace rive::pls;
using PixelLocalStorageType = PLSRenderContextWebGPUImpl::PixelLocalStorageType;

static std::unique_ptr<File> s_rivFile;
static std::unique_ptr<ArtboardInstance> s_artboard;
static std::unique_ptr<Scene> s_scene;

static std::unique_ptr<PLSRenderContext> s_plsContext;
static rcp<PLSRenderTargetWebGPU> s_renderTarget;
static std::unique_ptr<Renderer> s_renderer;

static int s_width, s_height;
double s_lastTimestamp = 0;

void riveInitPlayer(int w,
                    int h,
                    wgpu::Device gpu,
                    wgpu::Queue queue,
                    const pls::PlatformFeatures& platformFeatures,
                    PixelLocalStorageType plsType)
{
    s_plsContext = PLSRenderContextWebGPUImpl::MakeContext(gpu, queue, platformFeatures, plsType);
    s_renderTarget = s_plsContext->static_impl_cast<PLSRenderContextWebGPUImpl>()->makeRenderTarget(
        wgpu::TextureFormat::BGRA8Unorm,
        w,
        h);
    s_renderer = std::make_unique<PLSRenderer>(s_plsContext.get());

    s_rivFile = File::import({marty_riv, std::size(marty_riv)}, s_plsContext.get());
    // s_rivFile =
    //     File::import({Knight_square_2_riv, std::size(Knight_square_2_riv)}, s_plsContext.get());
    // s_rivFile = File::import({Rope_riv, std::size(Rope_riv)}, s_plsContext.get());
    s_artboard = s_rivFile->artboardDefault();
    s_scene = s_artboard->defaultScene();
    s_scene->advanceAndApply(0);

    s_width = w;
    s_height = h;
}

void riveMainLoop(double timestamp, wgpu::TextureView targetTextureView)
{
    s_renderTarget->setTargetTextureView(targetTextureView);

    rive::pls::PLSRenderContext::FrameDescriptor frameDescriptor = {
        .renderTarget = s_renderTarget,
        .clearColor = 0xff8030ff,
    };
    s_plsContext->beginFrame(std::move(frameDescriptor));

    s_renderer->save();
    s_renderer->transform(computeAlignment(rive::Fit::contain,
                                           rive::Alignment::center,
                                           rive::AABB(0, 0, s_width, s_height),
                                           s_artboard->bounds()));
    s_scene->draw(s_renderer.get());
    s_renderer->restore();

    if (s_lastTimestamp != 0)
    {
        s_scene->advanceAndApply(timestamp - s_lastTimestamp);
    }
    s_lastTimestamp = timestamp;

    s_plsContext->flush();
}

#ifdef RIVE_WEBGPU

EmJsHandle s_gpuHandle;
EmJsHandle s_queueHandle;

extern "C"
{
    void EMSCRIPTEN_KEEPALIVE riveInitPlayer(int w,
                                             int h,
                                             int gpuID,
                                             int queueID,
                                             bool uninvertOnScreenY,
                                             bool EXT_shader_pixel_local_storage)
    {
        s_gpuHandle = gpuID;
        s_queueHandle = queueID;
        pls::PlatformFeatures platformFeatures;
        if (uninvertOnScreenY)
        {
            platformFeatures.uninvertOnScreenY = true;
        }
        riveInitPlayer(w,
                       h,
                       wgpu::Device::Acquire(emscripten_webgpu_import_device(s_gpuHandle.get())),
                       emscripten_webgpu_import_queue(s_queueHandle.get()),
                       platformFeatures,
                       EXT_shader_pixel_local_storage
                           ? PixelLocalStorageType::EXT_shader_pixel_local_storage
                           : PixelLocalStorageType::bestEffort);
    }

    void EMSCRIPTEN_KEEPALIVE riveMainLoop(double timestamp, int textureViewID)
    {
        EmJsHandle textureViewHandle = EmJsHandle(textureViewID);
        riveMainLoop(timestamp,
                     wgpu::TextureView::Acquire(
                         emscripten_webgpu_import_texture_view(textureViewHandle.get())));
    }
}

#endif

#ifdef RIVE_DAWN
#include <GLFW/glfw3.h>
#ifndef __APPLE__
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

static void glfw_error_callback(int code, const char* message)
{
    printf("GLFW error: %i - %s\n", code, message);
}

static void print_device_error(WGPUErrorType errorType, const char* message, void*)
{
    const char* errorTypeName = "";
    switch (errorType)
    {
        case WGPUErrorType_Validation:
            errorTypeName = "Validation";
            break;
        case WGPUErrorType_OutOfMemory:
            errorTypeName = "Out of memory";
            break;
        case WGPUErrorType_Unknown:
            errorTypeName = "Unknown";
            break;
        case WGPUErrorType_DeviceLost:
            errorTypeName = "Device lost";
            break;
        default:
            RIVE_UNREACHABLE();
            return;
    }
    printf("%s error: %s\n", errorTypeName, message);
}

static void device_lost_callback(WGPUDeviceLostReason reason, const char* message, void*)
{
    printf("device lost: %s\n", message);
}

static void device_log_callback(WGPULoggingType type, const char* message, void*)
{
    printf("Device log %s\n", message);
}

static GLFWwindow* s_window = nullptr;
static WGPUDevice s_backendDevice = {};
static wgpu::SwapChain s_swapchain = {};
static std::unique_ptr<dawn::native::Instance> s_instance;

#ifdef __APPLE__
extern float GetDawnWindowBackingScaleFactor(GLFWwindow*, bool retina);
extern std::unique_ptr<wgpu::ChainedStruct> SetupDawnWindowAndGetSurfaceDescriptor(GLFWwindow*,
                                                                                   bool retina);
#else
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

static std::unique_ptr<wgpu::ChainedStruct> SetupDawnWindowAndGetSurfaceDescriptor(
    GLFWwindow* window,
    bool retina)
{
    std::unique_ptr<wgpu::SurfaceDescriptorFromWindowsHWND> desc =
        std::make_unique<wgpu::SurfaceDescriptorFromWindowsHWND>();
    desc->hwnd = glfwGetWin32Window(window);
    desc->hinstance = GetModuleHandle(nullptr);
    return std::move(desc);
}
#endif

#endif

int main(int argc, const char** argv)
{
#ifdef RIVE_DAWN
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
    s_window = glfwCreateWindow(1600, 1600, "Rive Renderer", nullptr, nullptr);
    if (!s_window)
    {
        glfwTerminate();
        fprintf(stderr, "Failed to create window.\n");
        return -1;
    }
    glfwSetWindowTitle(s_window, "Rive Renderer");

    WGPUInstanceDescriptor instanceDescriptor{};
    instanceDescriptor.features.timedWaitAnyEnable = true;
    s_instance = std::make_unique<dawn::native::Instance>(&instanceDescriptor);

    wgpu::RequestAdapterOptions adapterOptions = {};

    // Get an adapter for the backend to use, and create the device.
    auto adapters = s_instance->EnumerateAdapters(&adapterOptions);
    wgpu::DawnAdapterPropertiesPowerPreference power_props{};
    wgpu::AdapterProperties adapterProperties{};
    adapterProperties.nextInChain = &power_props;
    // Find the first adapter which satisfies the adapterType requirement.
    auto isAdapterType = [&adapterProperties](const auto& adapter) -> bool {
        adapter.GetProperties(&adapterProperties);
        return adapterProperties.adapterType == wgpu::AdapterType::DiscreteGPU;
    };
    auto preferredAdapter = std::find_if(adapters.begin(), adapters.end(), isAdapterType);
    if (preferredAdapter == adapters.end())
    {
        fprintf(stderr, "Failed to find an adapter! Please try another adapter type.\n");
        exit(-1);
    }

    std::vector<const char*> enableToggleNames = {
        "allow_unsafe_apis",
        "turn_off_vsync",
        // "skip_validation",
    };
    std::vector<const char*> disabledToggleNames;

    WGPUDawnTogglesDescriptor toggles = {
        .chain =
            {
                .next = nullptr,
                .sType = WGPUSType_DawnTogglesDescriptor,
            },
        .enabledToggleCount = enableToggleNames.size(),
        .enabledToggles = enableToggleNames.data(),
        .disabledToggleCount = disabledToggleNames.size(),
        .disabledToggles = disabledToggleNames.data(),
    };

    std::vector<WGPUFeatureName> requiredFeatures = {};

    WGPUDeviceDescriptor deviceDesc = {
        .nextInChain = reinterpret_cast<WGPUChainedStruct*>(&toggles),
        .requiredFeatureCount = requiredFeatures.size(),
        .requiredFeatures = requiredFeatures.data(),
    };

    s_backendDevice = preferredAdapter->CreateDevice(&deviceDesc);
    DawnProcTable backendProcs = dawn::native::GetProcs();
    dawnProcSetProcs(&backendProcs);
    backendProcs.deviceSetUncapturedErrorCallback(s_backendDevice, print_device_error, nullptr);
    backendProcs.deviceSetDeviceLostCallback(s_backendDevice, device_lost_callback, nullptr);
    backendProcs.deviceSetLoggingCallback(s_backendDevice, device_log_callback, nullptr);
    wgpu::Device device = wgpu::Device::Acquire(s_backendDevice);

    int w, h;
    glfwGetFramebufferSize(s_window, &w, &h);
    // Create the swapchain
    auto surfaceChainedDesc = SetupDawnWindowAndGetSurfaceDescriptor(s_window, true);
    WGPUSurfaceDescriptor surfaceDesc = {
        .nextInChain = reinterpret_cast<WGPUChainedStruct*>(surfaceChainedDesc.get()),
    };
    WGPUSurface surface = backendProcs.instanceCreateSurface(s_instance->Get(), &surfaceDesc);

    WGPUSwapChainDescriptor swapChainDesc = {
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .width = static_cast<uint32_t>(w),
        .height = static_cast<uint32_t>(h),
        .presentMode = WGPUPresentMode_Immediate, // No vsync.
    };

    WGPUSwapChain backendSwapChain =
        backendProcs.deviceCreateSwapChain(s_backendDevice, surface, &swapChainDesc);
    s_swapchain = wgpu::SwapChain::Acquire(backendSwapChain);

    riveInitPlayer(w,
                   h,
                   device.Get(),
                   device.GetQueue(),
                   PlatformFeatures{},
                   PixelLocalStorageType::bestEffort);

    while (!glfwWindowShouldClose(s_window))
    {
        riveMainLoop(glfwGetTime(), s_swapchain.GetCurrentTextureView().Get());
        s_swapchain.Present();
        device.Tick();
        glfwPollEvents();
    }
    glfwTerminate();
#endif
    return 0;
}

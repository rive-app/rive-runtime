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
#include "rive/renderer/webgpu/em_js_handle.hpp"

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

using namespace rive;
using namespace rive::gpu;
using PixelLocalStorageType = PLSRenderContextWebGPUImpl::PixelLocalStorageType;

static std::unique_ptr<PLSRenderContext> s_plsContext;
static rcp<PLSRenderTargetWebGPU> s_renderTarget;
static std::unique_ptr<Renderer> s_renderer;

void riveInitPlayer(int w,
                    int h,
                    wgpu::Device gpu,
                    wgpu::Queue queue,
                    const gpu::PlatformFeatures& platformFeatures,
                    PixelLocalStorageType plsType,
                    int maxVertexStorageBlocks)
{
    PLSRenderContextWebGPUImpl::ContextOptions contextOptions = {
        .plsType = plsType,
        .disableStorageBuffers = maxVertexStorageBlocks < gpu::kMaxStorageBuffers,
    };
    s_plsContext =
        PLSRenderContextWebGPUImpl::MakeContext(gpu, queue, contextOptions, platformFeatures);
    s_renderTarget = s_plsContext->static_impl_cast<PLSRenderContextWebGPUImpl>()->makeRenderTarget(
        wgpu::TextureFormat::BGRA8Unorm,
        w,
        h);
    s_renderer = std::make_unique<RiveRenderer>(s_plsContext.get());
}

#ifdef RIVE_WEBGPU

EmJsHandle s_deviceHandle;
EmJsHandle s_queueHandle;
EmJsHandle s_textureViewHandle;

extern "C"
{
    void EMSCRIPTEN_KEEPALIVE RiveInitialize(int deviceID,
                                             int queueID,
                                             int canvasWidth,
                                             int canvasHeight,
                                             bool invertedY,
                                             int pixelLocalStorageType,
                                             int maxVertexStorgeBlocks)
    {
        s_deviceHandle = EmJsHandle(deviceID);
        s_queueHandle = EmJsHandle(queueID);
        gpu::PlatformFeatures platformFeatures;
        if (invertedY)
        {
            platformFeatures.uninvertOnScreenY = true;
        }
        riveInitPlayer(canvasWidth,
                       canvasHeight,
                       wgpu::Device::Acquire(emscripten_webgpu_import_device(s_deviceHandle.get())),
                       emscripten_webgpu_import_queue(s_queueHandle.get()),
                       platformFeatures,
                       static_cast<PixelLocalStorageType>(pixelLocalStorageType),
                       maxVertexStorgeBlocks);
    }

    intptr_t EMSCRIPTEN_KEEPALIVE RiveBeginRendering(int textureViewID,
                                                     int loadAction,
                                                     ColorInt clearColor)
    {
        s_textureViewHandle = EmJsHandle(textureViewID);
        auto targetTextureView = wgpu::TextureView::Acquire(
            emscripten_webgpu_import_texture_view(s_textureViewHandle.get()));
        s_renderTarget->setTargetTextureView(targetTextureView);

        s_plsContext->beginFrame({
            .renderTargetWidth = s_renderTarget->width(),
            .renderTargetHeight = s_renderTarget->height(),
            .loadAction = static_cast<gpu::LoadAction>(loadAction),
            .clearColor = clearColor,
        });

        s_renderer->save();
        return reinterpret_cast<intptr_t>(s_renderer.get());
    }

    void EMSCRIPTEN_KEEPALIVE RiveFlushRendering()
    {
        s_renderer->restore();
        s_plsContext->flush({.renderTarget = s_renderTarget.get()});
        s_textureViewHandle = EmJsHandle();
    }

    intptr_t EMSCRIPTEN_KEEPALIVE RiveLoadFile(intptr_t wasmBytesPtr, size_t length)
    {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(wasmBytesPtr);
        std::unique_ptr<File> file = File::import({bytes, length}, s_plsContext.get());
        return reinterpret_cast<intptr_t>(file.release());
    }

    intptr_t EMSCRIPTEN_KEEPALIVE File_artboardNamed(intptr_t nativePtr, const char* name)
    {
        auto file = reinterpret_cast<File*>(nativePtr);
        std::unique_ptr<ArtboardInstance> artboard = file->artboardNamed(name);
        return reinterpret_cast<intptr_t>(artboard.release());
    }

    intptr_t EMSCRIPTEN_KEEPALIVE File_artboardDefault(intptr_t nativePtr)
    {
        auto file = reinterpret_cast<File*>(nativePtr);
        std::unique_ptr<ArtboardInstance> artboard = file->artboardDefault();
        return reinterpret_cast<intptr_t>(artboard.release());
    }

    void EMSCRIPTEN_KEEPALIVE File_destroy(intptr_t nativePtr)
    {
        auto file = reinterpret_cast<File*>(nativePtr);
        delete file;
    }

    int EMSCRIPTEN_KEEPALIVE ArtboardInstance_width(intptr_t nativePtr)
    {
        auto artboard = reinterpret_cast<ArtboardInstance*>(nativePtr);
        return artboard->bounds().width();
    }

    int EMSCRIPTEN_KEEPALIVE ArtboardInstance_height(intptr_t nativePtr)
    {
        auto artboard = reinterpret_cast<ArtboardInstance*>(nativePtr);
        return artboard->bounds().height();
    }

    intptr_t EMSCRIPTEN_KEEPALIVE ArtboardInstance_stateMachineNamed(intptr_t nativePtr,
                                                                     const char* name)
    {
        auto artboard = reinterpret_cast<ArtboardInstance*>(nativePtr);
        std::unique_ptr<StateMachineInstance> stateMachine = artboard->stateMachineNamed(name);
        return reinterpret_cast<intptr_t>(stateMachine.release());
    }

    intptr_t EMSCRIPTEN_KEEPALIVE ArtboardInstance_animationNamed(intptr_t nativePtr,
                                                                  const char* name)
    {
        auto artboard = reinterpret_cast<ArtboardInstance*>(nativePtr);
        std::unique_ptr<LinearAnimationInstance> animation = artboard->animationNamed(name);
        return reinterpret_cast<intptr_t>(animation.release());
    }

    intptr_t EMSCRIPTEN_KEEPALIVE ArtboardInstance_defaultStateMachine(intptr_t nativePtr)
    {
        auto artboard = reinterpret_cast<ArtboardInstance*>(nativePtr);
        std::unique_ptr<StateMachineInstance> stateMachine = artboard->defaultStateMachine();
        return reinterpret_cast<intptr_t>(stateMachine.release());
    }

    void EMSCRIPTEN_KEEPALIVE ArtboardInstance_align(intptr_t nativePtr,
                                                     intptr_t rendererNativePtr,
                                                     float frameLeft,
                                                     float frameTop,
                                                     float frameRight,
                                                     float frameBot,
                                                     int fitValue,
                                                     float alignmentX,
                                                     float alignmentY)
    {
        auto artboard = reinterpret_cast<ArtboardInstance*>(nativePtr);
        auto renderer = reinterpret_cast<RiveRenderer*>(rendererNativePtr);
        auto fit = static_cast<Fit>(fitValue);
        Alignment alignment = {alignmentX, alignmentY};
        AABB frame = {frameLeft, frameTop, frameRight, frameBot};
        renderer->align(fit, alignment, frame, artboard->bounds());
    }

    void EMSCRIPTEN_KEEPALIVE ArtboardInstance_destroy(intptr_t nativePtr)
    {
        auto artboard = reinterpret_cast<ArtboardInstance*>(nativePtr);
        delete artboard;
    }

    void EMSCRIPTEN_KEEPALIVE StateMachineInstance_setBool(intptr_t nativePtr,
                                                           const char* inputName,
                                                           int value)
    {
        auto stateMachine = reinterpret_cast<StateMachineInstance*>(nativePtr);
        if (SMIBool* input = stateMachine->getBool(inputName))
        {
            input->value(static_cast<bool>(value));
        }
    }

    void EMSCRIPTEN_KEEPALIVE StateMachineInstance_setNumber(intptr_t nativePtr,
                                                             const char* inputName,
                                                             float value)
    {
        auto stateMachine = reinterpret_cast<StateMachineInstance*>(nativePtr);
        if (SMINumber* input = stateMachine->getNumber(inputName))
        {
            input->value(value);
        }
    }

    void EMSCRIPTEN_KEEPALIVE StateMachineInstance_fireTrigger(intptr_t nativePtr,
                                                               const char* inputName)
    {
        auto stateMachine = reinterpret_cast<StateMachineInstance*>(nativePtr);
        if (SMITrigger* input = stateMachine->getTrigger(inputName))
        {
            input->fire();
        }
    }

    void EMSCRIPTEN_KEEPALIVE StateMachineInstance_pointerDown(intptr_t nativePtr, float x, float y)
    {
        auto stateMachine = reinterpret_cast<StateMachineInstance*>(nativePtr);
        stateMachine->pointerDown({x, y});
    }

    void EMSCRIPTEN_KEEPALIVE StateMachineInstance_pointerMove(intptr_t nativePtr, float x, float y)
    {
        auto stateMachine = reinterpret_cast<StateMachineInstance*>(nativePtr);
        stateMachine->pointerMove({x, y});
    }

    void EMSCRIPTEN_KEEPALIVE StateMachineInstance_pointerUp(intptr_t nativePtr, float x, float y)
    {
        auto stateMachine = reinterpret_cast<StateMachineInstance*>(nativePtr);
        stateMachine->pointerUp({x, y});
    }

    void EMSCRIPTEN_KEEPALIVE StateMachineInstance_advanceAndApply(intptr_t nativePtr,
                                                                   double elapsed)
    {
        auto stateMachine = reinterpret_cast<StateMachineInstance*>(nativePtr);
        stateMachine->advanceAndApply(elapsed);
    }

    void EMSCRIPTEN_KEEPALIVE StateMachineInstance_draw(intptr_t nativePtr,
                                                        intptr_t rendererNativePtr)
    {
        auto stateMachine = reinterpret_cast<StateMachineInstance*>(nativePtr);
        auto renderer = reinterpret_cast<RiveRenderer*>(rendererNativePtr);
        stateMachine->draw(renderer);
    }

    void EMSCRIPTEN_KEEPALIVE StateMachineInstance_destroy(intptr_t nativePtr)
    {
        auto stateMachine = reinterpret_cast<StateMachineInstance*>(nativePtr);
        delete stateMachine;
    }

    void EMSCRIPTEN_KEEPALIVE LinearAnimationInstance_advanceAndApply(intptr_t nativePtr,
                                                                      double elapsed)
    {
        auto animation = reinterpret_cast<LinearAnimationInstance*>(nativePtr);
        animation->advanceAndApply(elapsed);
    }

    void EMSCRIPTEN_KEEPALIVE LinearAnimationInstance_draw(intptr_t nativePtr,
                                                           intptr_t rendererNativePtr)
    {
        auto animation = reinterpret_cast<LinearAnimationInstance*>(nativePtr);
        auto renderer = reinterpret_cast<RiveRenderer*>(rendererNativePtr);
        animation->draw(renderer);
    }

    void EMSCRIPTEN_KEEPALIVE LinearAnimationInstance_destroy(intptr_t nativePtr)
    {
        auto animation = reinterpret_cast<LinearAnimationInstance*>(nativePtr);
        delete animation;
    }

    void EMSCRIPTEN_KEEPALIVE Renderer_save(intptr_t nativePtr)
    {
        auto renderer = reinterpret_cast<RiveRenderer*>(nativePtr);
        renderer->save();
    }

    void EMSCRIPTEN_KEEPALIVE Renderer_restore(intptr_t nativePtr)
    {
        auto renderer = reinterpret_cast<RiveRenderer*>(nativePtr);
        renderer->restore();
    }

    void EMSCRIPTEN_KEEPALIVE Renderer_translate(intptr_t nativePtr, float x, float y)
    {
        auto renderer = reinterpret_cast<RiveRenderer*>(nativePtr);
        renderer->translate(x, y);
    }

    void EMSCRIPTEN_KEEPALIVE Renderer_transform(intptr_t nativePtr,
                                                 float xx,
                                                 float xy,
                                                 float yx,
                                                 float yy,
                                                 float tx,
                                                 float ty)
    {
        auto renderer = reinterpret_cast<RiveRenderer*>(nativePtr);
        Mat2D matrix(xx, xy, yx, yy, tx, ty);
        renderer->transform(matrix);
    }
}

#endif

#ifdef RIVE_DAWN
#include <GLFW/glfw3.h>
#ifndef __APPLE__
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <fstream>

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
        abort();
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
                   PixelLocalStorageType::none,
                   8);

    std::ifstream rivStream("../../../gold/rivs/Santa_Claus.riv", std::ios::binary);
    std::vector<uint8_t> rivBytes(std::istreambuf_iterator<char>(rivStream), {});
    std::unique_ptr<File> rivFile = File::import(rivBytes, s_plsContext.get());
    std::unique_ptr<ArtboardInstance> artboard = rivFile->artboardDefault();
    std::unique_ptr<Scene> scene = artboard->defaultScene();
    scene->advanceAndApply(0);

    double lastTimestamp = 0;

    while (!glfwWindowShouldClose(s_window))
    {
        double timestamp = glfwGetTime();
        s_renderTarget->setTargetTextureView(s_swapchain.GetCurrentTextureView());

        s_plsContext->beginFrame({
            .renderTargetWidth = s_renderTarget->width(),
            .renderTargetHeight = s_renderTarget->height(),
            .clearColor = 0xff8030ff,
        });

        s_renderer->save();
        s_renderer->transform(computeAlignment(rive::Fit::contain,
                                               rive::Alignment::center,
                                               rive::AABB(0, 0, w, h),
                                               artboard->bounds()));
        scene->draw(s_renderer.get());
        s_renderer->restore();

        s_plsContext->flush({.renderTarget = s_renderTarget.get()});
        s_swapchain.Present();
        device.Tick();
        glfwPollEvents();

        if (lastTimestamp != 0)
        {
            scene->advanceAndApply(timestamp - lastTimestamp);
        }
        lastTimestamp = timestamp;
    }
    glfwTerminate();
#endif
    return 0;
}

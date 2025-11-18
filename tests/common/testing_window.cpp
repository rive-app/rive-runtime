/*
 * Copyright 2022 Rive
 */

#include "testing_window.hpp"

#include "rive/rive_types.hpp"
#include <string>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
extern "C"
{
    // https://stackoverflow.com/questions/68469954/how-to-choose-specific-gpu-when-create-opengl-context:
    //
    //   OpenGL, or rather the Win32 GDI integration of it, doesn't offer means
    //   to explicitly select the desired device. However the drivers of Nvidia
    //   and AMD offer a workaround to have programs select, that they prefer to
    //   execute on the discrete GPU rather than the CPU integrated one.
    //
    // These also appear to select the discrete "Arc" GPU on an Intel system,
    // and to influence the GPU selection on D3D11.
    __declspec(dllexport) uint32_t NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// Call TestingWindow::Destroy if you want to delete the window singleton
TestingWindow* s_TestingWindow = nullptr;

const char* TestingWindow::BackendName(Backend backend)
{
    switch (backend)
    {
        case TestingWindow::Backend::gl:
            return "gl";
        case TestingWindow::Backend::d3d:
            return "d3d";
        case TestingWindow::Backend::d3d12:
            return "d3d12";
        case TestingWindow::Backend::metal:
            return "metal";
        case TestingWindow::Backend::vk:
            return "vk";
        case TestingWindow::Backend::moltenvk:
            return "moltenvk";
        case TestingWindow::Backend::swiftshader:
            return "swiftshader";
        case TestingWindow::Backend::angle:
            return "angle";
        case TestingWindow::Backend::dawn:
            return "dawn";
        case TestingWindow::Backend::wgpu:
            return "wgpu";
        case Backend::rhi:
            return "rhi";
        case TestingWindow::Backend::coregraphics:
            return "coregraphics";
        case TestingWindow::Backend::skia:
            return "skia";
        case TestingWindow::Backend::null:
            return "null";
    }
    RIVE_UNREACHABLE();
}

static std::vector<std::string> split(const char* str, char delimiter)
{
    std::stringstream ss(str);
    std::string token;
    std::vector<std::string> tokens;
    while (std::getline(ss, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

TestingWindow::Backend TestingWindow::ParseBackend(const char* name,
                                                   BackendParams* params)
{
    *params = {};
    // Backends can come in the form <backendName>, or
    // <gpuNameFilter>/<backendName>.
    std::vector<std::string> tokens = split(name, '/');
    assert(!tokens.empty());
    params->gpuNameFilter =
        tokens.size() > 1 ? tokens[tokens.size() - 2].c_str() : "";
    const std::string nameStr = tokens.back();
    const auto nameStartsWith = [&](std::string str) {
        return nameStr.rfind(str, 0) == 0;
    };
    const auto nameEndsWith = [&](std::string str) {
        auto result = nameStr.rfind(str);
        return result != std::string::npos &&
               result == nameStr.size() - str.size();
    };
    if (nameStartsWith("angle"))
    {
        if (nameStartsWith("anglemsaa"))
            params->msaa = true;
        if (nameEndsWith("_mtl") || nameEndsWith("_metal"))
            params->angleRenderer = ANGLERenderer::metal;
        else if (nameEndsWith("_d3d") || nameEndsWith("_d3d11"))
            params->angleRenderer = ANGLERenderer::d3d11;
        else if (nameEndsWith("_vk") || nameEndsWith("_vulkan"))
            params->angleRenderer = ANGLERenderer::vk;
        else if (nameEndsWith("_gles"))
            params->angleRenderer = ANGLERenderer::gles;
        else if (nameEndsWith("_gl"))
            params->angleRenderer = ANGLERenderer::gl;
        return Backend::angle;
    }
    if (nameStr == "gl")
    {
        return Backend::gl;
    }
    if (nameStr == "glatomic")
    {
        params->atomic = true;
        return Backend::gl;
    }
    if (nameStr == "glcw")
    {
        params->clockwise = true;
        return Backend::gl;
    }
    if (nameStr == "glmsaa")
    {
        params->msaa = true;
        return Backend::gl;
    }
    if (nameStr == "d3d")
    {
        return Backend::d3d;
    }
    if (nameStr == "d3datomic")
    {
        params->atomic = true;
        return Backend::d3d;
    }
    if (nameStr == "d3d12")
    {
        return Backend::d3d12;
    }
    if (nameStr == "d3d12atomic")
    {
        params->atomic = true;
        return Backend::d3d12;
    }
    if (nameStr == "metal")
    {
        return Backend::metal;
    }
    if (nameStr == "metalcw")
    {
        params->clockwise = true;
        return Backend::metal;
    }
    if (nameStr == "metalatomic")
    {
        params->atomic = true;
        return Backend::metal;
    }
    if (nameStr == "vulkan" || nameStr == "vk")
    {
        return Backend::vk;
    }
    if (nameStr == "vulkanatomic" || nameStr == "vkatomic")
    {
        params->atomic = true;
        return Backend::vk;
    }
    if (nameStr == "vulkanmsaa" || nameStr == "vkmsaa")
    {
        params->msaa = true;
        return Backend::vk;
    }
    if (nameStr == "vulkancore" || nameStr == "vkcore")
    {
        params->core = true;
        return Backend::vk;
    }
    if (nameStr == "vulkanmsaacore" || nameStr == "vkmsaacore")
    {
        params->core = true;
        params->msaa = true;
        return Backend::vk;
    }
    if (nameStr == "vulkansrgb" || nameStr == "vksrgb")
    {
        params->srgb = true;
        return Backend::vk;
    }
    if (nameStr == "vkcw")
    {
        params->clockwise = true;
        return Backend::vk;
    }
    if (nameStr == "vkcwatomic" || nameStr == "vkcwa")
    {
        params->clockwise = true;
        params->atomic = true;
        return Backend::vk;
    }
    if (nameStr == "moltenvk" || nameStr == "mvk")
    {
        return Backend::moltenvk;
    }
    if (nameStr == "moltenvkcore" || nameStr == "mvkcore")
    {
        params->core = true;
        return Backend::moltenvk;
    }
    if (nameStr == "swiftshader" || nameStr == "sw")
    {
        return Backend::swiftshader;
    }
    if (nameStr == "swiftshadercore" || nameStr == "swcore")
    {
        params->core = true;
        return Backend::swiftshader;
    }
    if (nameStr == "dawn")
    {
        return Backend::dawn;
    }
    if (nameStr == "wgpu")
    {
        return Backend::wgpu;
    }
    if (nameStr == "wgpucw")
    {
        params->clockwise = true;
        return Backend::wgpu;
    }
    if (nameStr == "wgpumsaa")
    {
        params->msaa = true;
        return Backend::wgpu;
    }
    if (nameStr == "rhi")
    {
        return Backend::rhi;
    }
    if (nameStr == "coregraphics")
    {
        return Backend::coregraphics;
    }
    if (nameStr == "skia")
    {
        return Backend::skia;
    }
    if (nameStr == "null")
    {
        return Backend::null;
    }
    fprintf(stderr, "'%s': invalid TestingWindow::Backend\n", name);
    abort();
}

static void set_environment_variable(const char* name, const char* value)
{
#ifndef PLATFORM_NO_ENV_API
    if (const char* existingValue = getenv(name))
    {
        printf("warning: %s=%s already set. Overriding with %s=%s\n",
               name,
               existingValue,
               name,
               value);
    }
#ifdef _WIN32
    SetEnvironmentVariableA(name, value);
#else
    setenv(name, value, /*overwrite=*/true);
#endif
#endif
}

TestingWindow* TestingWindow::Init(Backend backend,
                                   const BackendParams& backendParams,
                                   Visibility visibility,
                                   void* platformWindow)
{
    assert((backend == Backend::rhi) == (s_TestingWindow != nullptr));

#ifdef _WIN32
    // Set our backdoor GPU selection variables in case the API doesn't
    // allow us to select explicitly.
    const char* nameFilter = backendParams.gpuNameFilter.c_str();
    if (const char* gpuFromEnv = getenv("RIVE_GPU"); gpuFromEnv != nullptr)
    {
        // Override the program's GPU filter with one from the environment if
        // it's set.
        nameFilter = gpuFromEnv;
    }
    // "i" and "integrated" are special-case gpuNameFilters that mean "use
    // the integrated GPU".
    bool wantIntegratedGPU = static_cast<uint32_t>(
        strcmp(nameFilter, "integrated") == 0 || strcmp(nameFilter, "i") == 0);
    NvOptimusEnablement = !wantIntegratedGPU;
    AmdPowerXpressRequestHighPerformance = !wantIntegratedGPU;
#endif

    switch (backend)
    {
        case Backend::gl:
        case Backend::angle:
#ifndef RIVE_TOOLS_NO_GLFW
            if (backend != Backend::angle || visibility != Visibility::headless)
            {
                s_TestingWindow =
                    TestingWindow::MakeFiddleContext(backend,
                                                     backendParams,
                                                     visibility,
                                                     platformWindow);
            }
            else
#endif
            {
                s_TestingWindow =
                    MakeEGL(backend, backendParams, platformWindow);
            }
            break;
        case Backend::vk:
        case Backend::moltenvk:
        case Backend::swiftshader:
            if (backend == Backend::moltenvk)
            {
                // Use the MoltenVK built by
                // packages/runtime/renderer/make_moltenvk.sh
                constexpr static char kMoltenVKICD[] =
                    "../renderer/dependencies/MoltenVK/Package/Release/"
                    "MoltenVK/dynamic/dylib/macOS/MoltenVK_icd.json";
                set_environment_variable("VK_ICD_FILENAMES", kMoltenVKICD);
            }
            else if (backend == Backend::swiftshader)
            {
                // Use the swiftshader built by
                // packages/runtime/renderer/make_swiftshader.sh
                constexpr static char kSwiftShaderICD[] =
#ifdef __APPLE__
                    "../renderer/dependencies/SwiftShader/build/Darwin/"
                    "vk_swiftshader_icd.json";
#elif defined(_WIN32)
                    "../renderer/dependencies/SwiftShader/build/Windows/"
                    "vk_swiftshader_icd.json";
#else
                    "../renderer/dependencies/SwiftShader/build/Linux/"
                    "vk_swiftshader_icd.json";
#endif
                set_environment_variable("VK_ICD_FILENAMES", kSwiftShaderICD);
            }
#ifdef RIVE_ANDROID
            if (platformWindow != nullptr)
            {
                s_TestingWindow =
                    TestingWindow::MakeAndroidVulkan(backendParams,
                                                     platformWindow);
                break;
            }
#endif
            if (visibility == Visibility::headless)
            {
                s_TestingWindow =
                    TestingWindow::MakeVulkanTexture(backendParams);
            }
            else
            {
                s_TestingWindow =
                    TestingWindow::MakeFiddleContext(backend,
                                                     backendParams,
                                                     visibility,
                                                     platformWindow);
            }
            break;
        case Backend::metal:
#if defined(__APPLE__)
            if (visibility == Visibility::headless)
            {
                s_TestingWindow =
                    TestingWindow::MakeMetalTexture(backendParams);
                break;
            }
#endif
            s_TestingWindow = TestingWindow::MakeFiddleContext(backend,
                                                               backendParams,
                                                               visibility,
                                                               platformWindow);
            break;
        case Backend::d3d:
        case Backend::d3d12:
        case Backend::dawn:
            s_TestingWindow = TestingWindow::MakeFiddleContext(backend,
                                                               backendParams,
                                                               visibility,
                                                               platformWindow);
            break;
        case Backend::wgpu:
            s_TestingWindow = TestingWindow::MakeWGPU(backendParams);
            break;
        case Backend::rhi:
            break;
        case Backend::coregraphics:
            s_TestingWindow = MakeCoreGraphics();
            break;
        case Backend::skia:
            s_TestingWindow = MakeSkia();
            break;
        case Backend::null:
            s_TestingWindow = MakeNULL();
            break;
    }
    if (!s_TestingWindow)
    {
        fprintf(stderr,
                "Failed to create testing window for Backend::%s\n",
                BackendName(backend));
        abort();
    }

    return s_TestingWindow;
}

TestingWindow* TestingWindow::Get()
{
    assert(s_TestingWindow); // Call Init() first!
    return s_TestingWindow;
}

void TestingWindow::Set(TestingWindow* inWindow)
{
    assert(inWindow);
    s_TestingWindow = inWindow;
}

void TestingWindow::Destroy()
{
    assert(s_TestingWindow);
    delete s_TestingWindow;
    s_TestingWindow = nullptr;
}

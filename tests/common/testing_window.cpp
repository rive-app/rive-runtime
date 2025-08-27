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
    if (nameStr == "vulkanmsaa" || nameStr == "vkmsaa")
    {
        params->msaa = true;
        return Backend::vk;
    }
    if (nameStr == "vulkancore" || nameStr == "vkcore")
    {
        return Backend::vk;
    }
    if (nameStr == "vulkansrgb" || nameStr == "vksrgb")
    {
        params->core = true;
        return Backend::vk;
    }
    if (nameStr == "vulkancw" || nameStr == "vkcw")
    {
        params->clockwise = true;
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
    if (nameStr == "angle")
    {
        return Backend::angle;
    }
    if (nameStr == "anglemsaa")
    {
        params->msaa = true;
        return Backend::angle;
    }
    if (nameStr == "dawn")
    {
        return Backend::dawn;
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
    if (backend == Backend::rhi)
        assert(s_TestingWindow);
    else
        assert(!s_TestingWindow);
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
                s_TestingWindow = TestingWindow::MakeMetalTexture();
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

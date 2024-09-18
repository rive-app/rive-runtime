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

// Don't explicitly delete this object. Calling eglDestroyContext during app
// teardown causes a crash on Pixel 4. The OS will clean this up for us
// automatically when we exit.
std::unique_ptr<TestingWindow> s_TestingWindow = nullptr;

const char* TestingWindow::BackendName(Backend backend)
{
    switch (backend)
    {
        case TestingWindow::Backend::gl:
            return "gl";
        case TestingWindow::Backend::glatomic:
            return "glatomic";
        case TestingWindow::Backend::glmsaa:
            return "glmsaa";
        case TestingWindow::Backend::d3d:
            return "d3d";
        case TestingWindow::Backend::d3datomic:
            return "d3datomic";
        case TestingWindow::Backend::metal:
            return "metal";
        case TestingWindow::Backend::metalatomic:
            return "metalatomic";
        case TestingWindow::Backend::vk:
            return "vk";
        case TestingWindow::Backend::vkcore:
            return "vkcore";
        case TestingWindow::Backend::moltenvk:
            return "moltenvk";
        case TestingWindow::Backend::moltenvkcore:
            return "moltenvkcore";
        case TestingWindow::Backend::swiftshader:
            return "swiftshader";
        case TestingWindow::Backend::swiftshadercore:
            return "swiftshadercore";
        case TestingWindow::Backend::angle:
            return "angle";
        case TestingWindow::Backend::anglemsaa:
            return "anglemsaa";
        case TestingWindow::Backend::dawn:
            return "dawn";
        case TestingWindow::Backend::coregraphics:
            return "coregraphics";
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

TestingWindow::Backend TestingWindow::ParseBackend(const char* name, std::string* gpuNameFilter)
{
    // Backends can come in the form <backendName>, or
    // <gpuNameFilter>/<backendName>.
    std::vector<std::string> tokens = split(name, '/');
    assert(!tokens.empty());
    if (gpuNameFilter != nullptr)
    {
        *gpuNameFilter = tokens.size() > 1 ? tokens[tokens.size() - 2].c_str() : "";
    }
    const std::string nameStr = tokens.back();
    if (nameStr == "gl")
        return Backend::gl;
    if (nameStr == "glatomic")
        return Backend::glatomic;
    if (nameStr == "glmsaa")
        return Backend::glmsaa;
    if (nameStr == "d3d")
        return Backend::d3d;
    if (nameStr == "d3datomic")
        return Backend::d3datomic;
    if (nameStr == "metal")
        return Backend::metal;
    if (nameStr == "metalatomic")
        return Backend::metalatomic;
    if (nameStr == "vulkan" || nameStr == "vk")
        return Backend::vk;
    if (nameStr == "vulkancore" || nameStr == "vkcore")
        return Backend::vkcore;
    if (nameStr == "moltenvk" || nameStr == "mvk")
        return Backend::moltenvk;
    if (nameStr == "moltenvkcore" || nameStr == "mvkcore")
        return Backend::moltenvkcore;
    if (nameStr == "swiftshader" || nameStr == "sw")
        return Backend::swiftshader;
    if (nameStr == "swiftshadercore" || nameStr == "swcore")
        return Backend::swiftshadercore;
    if (nameStr == "angle")
        return Backend::angle;
    if (nameStr == "anglemsaa")
        return Backend::anglemsaa;
    if (nameStr == "dawn")
        return Backend::dawn;
    if (nameStr == "coregraphics")
        return Backend::coregraphics;
    fprintf(stderr, "'%s': invalid TestingWindow::Backend\n", name);
    abort();
}

static void set_environment_variable(const char* name, const char* value)
{
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
}

TestingWindow* TestingWindow::Init(Backend backend,
                                   Visibility visibility,
                                   const std::string& gpuNameFilterStr,
                                   void* platformWindow)
{
    const char* gpuNameFilter = !gpuNameFilterStr.empty() ? gpuNameFilterStr.c_str() : nullptr;
    assert(!s_TestingWindow);
    switch (backend)
    {
        case Backend::gl:
        case Backend::glatomic:
        case Backend::glmsaa:
        case Backend::angle:
        case Backend::anglemsaa:
#ifndef RIVE_TOOLS_NO_GLFW
            if (!IsANGLE(backend) || visibility != Visibility::headless)
            {
                s_TestingWindow = TestingWindow::MakeFiddleContext(backend,
                                                                   visibility,
                                                                   gpuNameFilter,
                                                                   platformWindow);
            }
            else
#endif
            {
                s_TestingWindow = MakeEGL(backend, platformWindow);
            }
            break;
        case Backend::vk:
        case Backend::vkcore:
        case Backend::moltenvk:
        case Backend::moltenvkcore:
        case Backend::swiftshader:
        case Backend::swiftshadercore:
            if (backend == Backend::moltenvk || backend == Backend::moltenvkcore)
            {
                // Use the MoltenVK built by
                // packages/runtime/renderer/make_moltenvk.sh
                constexpr static char kMoltenVKICD[] =
                    "../renderer/dependencies/MoltenVK/Package/Release/"
                    "MoltenVK/dynamic/dylib/macOS/MoltenVK_icd.json";
                set_environment_variable("VK_ICD_FILENAMES", kMoltenVKICD);
            }
            else if (backend == Backend::swiftshader || backend == Backend::swiftshadercore)
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
                s_TestingWindow = TestingWindow::MakeAndroidVulkan(platformWindow);
                break;
            }
#endif
            if (visibility == Visibility::headless)
            {
                s_TestingWindow = TestingWindow::MakeVulkanTexture(IsCore(backend), gpuNameFilter);
            }
            else
            {
                s_TestingWindow = TestingWindow::MakeFiddleContext(backend,
                                                                   visibility,
                                                                   gpuNameFilter,
                                                                   platformWindow);
            }
            break;
        case Backend::metal:
        case Backend::metalatomic:
#if defined(__APPLE__) && defined(RIVE_TOOLS_NO_GLFW)
            s_TestingWindow = TestingWindow::MakeMetalTexture();
            break;
#endif
            [[fallthrough]];
        case Backend::d3d:
        case Backend::d3datomic:
        case Backend::dawn:
            s_TestingWindow = TestingWindow::MakeFiddleContext(backend,
                                                               visibility,
                                                               gpuNameFilter,
                                                               platformWindow);
            break;
        case Backend::coregraphics:
#ifdef RIVE_MACOSX
            s_TestingWindow = MakeCoreGraphics();
#endif
            break;
    }
    if (!s_TestingWindow)
    {
        fprintf(stderr, "Failed to create testing window for Backend::%s\n", BackendName(backend));
        abort();
    }
    return s_TestingWindow.get();
}

TestingWindow* TestingWindow::Get()
{
    assert(s_TestingWindow); // Call Init() first!
    return s_TestingWindow.get();
}

void TestingWindow::Destroy()
{
    assert(s_TestingWindow);
    s_TestingWindow = nullptr;
}

char TestingWindow::getKey()
{
    fprintf(stderr, "TestingWindow::getKey not implemented.");
    abort();
}

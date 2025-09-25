#include "fiddle_context.hpp"

#include "rive/math/simd.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/layout.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/static_scene.hpp"
#include "rive/profiler/profiler_macros.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <sstream>
#endif

using namespace rive;

constexpr static char kMoltenVKICD[] =
    "dependencies/MoltenVK/Package/Release/MoltenVK/dynamic/dylib/macOS/"
    "MoltenVK_icd.json";

constexpr static char kSwiftShaderICD[] = "dependencies/SwiftShader/build/"
#ifdef __APPLE__
                                          "Darwin"
#elif defined(_WIN32)
                                          "Windows"
#else
                                          "Linux"
#endif
                                          "/vk_swiftshader_icd.json";
static FiddleContextOptions options;
static GLFWwindow* window = nullptr;
static int msaa = 0;
static bool forceAtomicMode = false;
static bool wireframe = false;
static bool disableFill = false;
static bool disableStroke = false;
static bool clockwiseFill = false;
static bool hotloadShaders = false;

static std::unique_ptr<FiddleContext> fiddleContext;

static float2 pts[] = {{260 + 2 * 100, 60 + 2 * 500},
                       {260 + 2 * 257, 60 + 2 * 233},
                       {260 + 2 * -100, 60 + 2 * 300},
                       {260 + 2 * 100, 60 + 2 * 200},
                       {260 + 2 * 250, 60 + 2 * 0},
                       {260 + 2 * 400, 60 + 2 * 200},
                       {260 + 2 * 213, 60 + 2 * 200},
                       {260 + 2 * 213, 60 + 2 * 300},
                       {260 + 2 * 391, 60 + 2 * 480},
                       {1400, 1400}}; // Feather control.
constexpr static int NUM_INTERACTIVE_PTS = sizeof(pts) / sizeof(*pts);

static float strokeWidth = 70;

static float2 translate;
static float scale = 1;

static StrokeJoin join = StrokeJoin::round;
static StrokeCap cap = StrokeCap::round;

static bool doClose = false;
static bool paused = false;
static bool unlockedLogic = true;

static int dragIdx = -1;
static float2 dragLastPos;

static int animation = -1;
static int stateMachine = -1;
static int horzRepeat = 0;
static int upRepeat = 0;
static int downRepeat = 0;

rcp<File> rivFile;
std::vector<std::unique_ptr<Artboard>> artboards;
std::vector<std::unique_ptr<Scene>> scenes;
std::vector<rive::rcp<rive::ViewModelInstance>> viewModelInstances;

static void clear_scenes()
{
    artboards.clear();
    scenes.clear();
    viewModelInstances.clear();
}

static void make_scenes(size_t count)
{
    clear_scenes();
    for (size_t i = 0; i < count; ++i)
    {
        // Tip: you can change the artboard shown here
        // auto artboard = rivFile->artboardAt(2);
        auto artboard = rivFile->artboardDefault();
        std::unique_ptr<Scene> scene;
        if (stateMachine >= 0)
        {
            scene = artboard->stateMachineAt(stateMachine);
        }
        else if (animation >= 0)
        {
            scene = artboard->animationAt(animation);
        }
        else
        {
            scene = artboard->animationAt(0);
        }
        if (scene == nullptr)
        {
            // This is a riv without any animations or state machines. Just draw
            // the artboard.
            scene = std::make_unique<StaticScene>(artboard.get());
        }

        int viewModelId = artboard.get()->viewModelId();
        viewModelInstances.push_back(
            viewModelId == -1
                ? rivFile->createViewModelInstance(artboard.get())
                : rivFile->createViewModelInstance(viewModelId, 0));
        artboard->bindViewModelInstance(viewModelInstances.back());
        if (viewModelInstances.back() != nullptr)
        {
            scene->bindViewModelInstance(viewModelInstances.back());
        }

        scene->advanceAndApply(scene->durationSeconds() * i / count);
        artboards.push_back(std::move(artboard));
        scenes.push_back(std::move(scene));
    }
}

#ifdef __EMSCRIPTEN__
EM_JS(int, window_inner_width, (), { return window["innerWidth"]; });
EM_JS(int, window_inner_height, (), { return window["innerHeight"]; });
EM_JS(char*, get_location_hash_str, (), {
    var jsString = window.location.hash.substring(1);
    var lengthBytes = lengthBytesUTF8(jsString) + 1;
    var stringOnWasmHeap = _malloc(lengthBytes);
    stringToUTF8(jsString, stringOnWasmHeap, lengthBytes);
    return stringOnWasmHeap;
});
#endif

static void mouse_button_callback(GLFWwindow* window,
                                  int button,
                                  int action,
                                  int mods)
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    float dpiScale = fiddleContext->dpiScale(window);
    x *= dpiScale;
    y *= dpiScale;
    dragLastPos = float2{(float)x, (float)y};
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        dragIdx = -1;
        if (!rivFile)
        {
            for (int i = 0; i < NUM_INTERACTIVE_PTS; ++i)
            {
                if (simd::all(simd::abs(dragLastPos - (pts[i] + translate)) <
                              100))
                {
                    dragIdx = i;
                    break;
                }
            }
        }
    }
}

static void mousemove_callback(GLFWwindow* window, double x, double y)
{
    float dpiScale = fiddleContext->dpiScale(window);
    x *= dpiScale;
    y *= dpiScale;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        float2 pos = float2{(float)x, (float)y};
        if (dragIdx >= 0)
        {
            pts[dragIdx] += (pos - dragLastPos);
        }
        else
        {
            translate += (pos - dragLastPos);
        }
        dragLastPos = pos;
    }
}

int lastWidth = 0, lastHeight = 0;
double fpsLastTime = 0;
double fpsLastTimeLogic = 0;
int fpsFrames = 0;
static bool needsTitleUpdate = false;

enum class API
{
    gl,
    metal,
    d3d,
    d3d12,
    dawn,
    vulkan,
};

API api =
#if defined(__APPLE__)
    API::metal
#elif defined(_WIN32)
    API::d3d
#else
    API::gl
#endif
    ;

bool angle = false;
bool skia = false;

static void key_callback(GLFWwindow* window,
                         int key,
                         int scancode,
                         int action,
                         int mods)
{
    bool shift = mods & GLFW_MOD_SHIFT;
    if (action == GLFW_PRESS)
    {
        switch (key)
        {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, 1);
                break;
            case GLFW_KEY_GRAVE_ACCENT: // ` key, backtick
                hotloadShaders = true;
                break;
            case GLFW_KEY_A:
                forceAtomicMode = !forceAtomicMode;
                fpsLastTime = 0;
                fpsFrames = 0;
                needsTitleUpdate = true;
                break;
            case GLFW_KEY_D:
                printf("static float scale = %f;\n", scale);
                printf("static float2 translate = {%f, %f};\n",
                       translate.x,
                       translate.y);
                printf("static float2 pts[] = {");
                for (int i = 0; i < NUM_INTERACTIVE_PTS; i++)
                {
                    printf("{%g, %g}", pts[i].x, pts[i].y);
                    if (i < NUM_INTERACTIVE_PTS - 1)
                    {
                        printf(", ");
                    }
                    else
                    {
                        printf("};\n");
                    }
                }
                fflush(stdout);
                break;
            case GLFW_KEY_Z:
                fiddleContext->toggleZoomWindow();
                break;
            case GLFW_KEY_MINUS:
                strokeWidth /= 1.5f;
                break;
            case GLFW_KEY_EQUAL:
                strokeWidth *= 1.5f;
                break;
            case GLFW_KEY_W:
                wireframe = !wireframe;
                break;
            case GLFW_KEY_C:
                cap = static_cast<StrokeCap>((static_cast<int>(cap) + 1) % 3);
                break;
            case GLFW_KEY_O:
                doClose = !doClose;
                break;
            case GLFW_KEY_S:
                if (shift)
                {
                    // Toggle Skia.
                    clear_scenes();
                    rivFile = nullptr;
                    skia = !skia;
                    fiddleContext = skia ? FiddleContext::MakeGLSkia()
                                         : FiddleContext::MakeGLPLS();
                    lastWidth = 0;
                    lastHeight = 0;
                    fpsLastTime = 0;
                    fpsFrames = 0;
                    needsTitleUpdate = true;
                }
                else
                {
                    disableStroke = !disableStroke;
                }
                break;
            case GLFW_KEY_F:
                disableFill = !disableFill;
                break;
            case GLFW_KEY_X:
                clockwiseFill = !clockwiseFill;
                break;
            case GLFW_KEY_P:
                paused = !paused;
                break;
            case GLFW_KEY_H:
                if (!shift)
                    ++horzRepeat;
                else if (horzRepeat > 0)
                    --horzRepeat;
                break;
            case GLFW_KEY_K:
                if (!shift)
                    ++upRepeat;
                else if (upRepeat > 0)
                    --upRepeat;
                break;
            case GLFW_KEY_J:
                if (!rivFile)
                    join = static_cast<StrokeJoin>(
                        (static_cast<int>(join) + 1) % 3);
                else if (!shift)
                    ++downRepeat;
                else if (downRepeat > 0)
                    --downRepeat;
                break;
            case GLFW_KEY_UP:
            {
                float oldScale = scale;
                scale *= 1.25;
                double x = 0, y = 0;
                glfwGetCursorPos(window, &x, &y);
                float2 cursorPos = float2{(float)x, (float)y} *
                                   fiddleContext->dpiScale(window);
                translate =
                    cursorPos + (translate - cursorPos) * scale / oldScale;
                break;
            }
            case GLFW_KEY_DOWN:
            {
                float oldScale = scale;
                scale /= 1.25;
                double x = 0, y = 0;
                glfwGetCursorPos(window, &x, &y);
                float2 cursorPos = float2{(float)x, (float)y} *
                                   fiddleContext->dpiScale(window);
                translate =
                    cursorPos + (translate - cursorPos) * scale / oldScale;
                break;
            }
            case GLFW_KEY_U:
                unlockedLogic = !unlockedLogic;
        }
    }
}

static void glfw_error_callback(int code, const char* message)
{
    printf("GLFW error: %i - %s\n", code, message);
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

std::unique_ptr<Renderer> renderer;
std::string rivName;

void riveMainLoop();

int main(int argc, const char** argv)
{
    // Cause stdout and stderr to print immediately without buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

#ifdef DEBUG
    options.enableVulkanValidationLayers = true;
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(riveMainLoop, 0, false);

    // Override argc/argv with the window location hash string.
    char* hash = get_location_hash_str();
    std::stringstream ss(hash);
    std::vector<std::string> hashStrs;
    std::vector<const char*> hashArgs;
    std::string arg;

    hashStrs.push_back("index.html");
    while (std::getline(ss, arg, ':'))
    {
        hashStrs.push_back(std::move(arg));
    }
    for (const std::string& str : hashStrs)
    {
        hashArgs.push_back(str.c_str());
    }

    argc = hashArgs.size();
    argv = hashArgs.data();
    free(hash);
#endif

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--gl"))
        {
            api = API::gl;
        }
        else if (!strcmp(argv[i], "--glatomic"))
        {
            api = API::gl;
            forceAtomicMode = true;
        }
        else if (!strcmp(argv[i], "--glcw"))
        {
            api = API::gl;
            forceAtomicMode = true;
            clockwiseFill = true;
        }
        else if (!strcmp(argv[i], "--metal"))
        {
            api = API::metal;
        }
        else if (!strcmp(argv[i], "--metalcw"))
        {
            api = API::metal;
            clockwiseFill = true;
        }
        else if (!strcmp(argv[i], "--metalatomic"))
        {
            api = API::metal;
            forceAtomicMode = true;
        }
        else if (!strcmp(argv[i], "--mvk") || !strcmp(argv[i], "--moltenvk"))
        {
            set_environment_variable("VK_ICD_FILENAMES", kMoltenVKICD);
            api = API::vulkan;
        }
        else if (!strcmp(argv[i], "--mvkatomic") ||
                 !strcmp(argv[i], "--moltenvkatomic"))
        {
            set_environment_variable("VK_ICD_FILENAMES", kMoltenVKICD);
            api = API::vulkan;
            forceAtomicMode = true;
        }
        else if (!strcmp(argv[i], "--sw") || !strcmp(argv[i], "--swiftshader"))
        {
            // Use the swiftshader built by
            // packages/runtime/renderer/make_swiftshader.sh
            set_environment_variable("VK_ICD_FILENAMES", kSwiftShaderICD);
            api = API::vulkan;
        }
        else if (!strcmp(argv[i], "--swatomic") ||
                 !strcmp(argv[i], "--swiftshaderatomic"))
        {
            // Use the swiftshader built by
            // packages/runtime/renderer/make_swiftshader.sh
            set_environment_variable("VK_ICD_FILENAMES", kSwiftShaderICD);
            api = API::vulkan;
            forceAtomicMode = true;
        }
        else if (!strcmp(argv[i], "--dawn"))
        {
            api = API::dawn;
        }
        else if (!strcmp(argv[i], "--d3d"))
        {
            api = API::d3d;
        }
        else if (!strcmp(argv[i], "--d3d12"))
        {
            api = API::d3d12;
        }
        else if (!strcmp(argv[i], "--d3datomic"))
        {
            api = API::d3d;
            forceAtomicMode = true;
        }
        else if (!strcmp(argv[i], "--d3d12atomic"))
        {
            api = API::d3d12;
            forceAtomicMode = true;
        }
        else if (!strcmp(argv[i], "--vulkan") || !strcmp(argv[i], "--vk"))
        {
            api = API::vulkan;
        }
        else if (!strcmp(argv[i], "--vkcw"))
        {
            api = API::vulkan;
            clockwiseFill = true;
        }
        else if (!strcmp(argv[i], "--vulkanatomic") ||
                 !strcmp(argv[i], "--vkatomic"))
        {
            api = API::vulkan;
            forceAtomicMode = true;
        }
#ifdef RIVE_DESKTOP_GL
        else if (!strcmp(argv[i], "--angle_gl"))
        {
            api = API::gl;
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE,
                         GLFW_ANGLE_PLATFORM_TYPE_OPENGL);
            angle = true;
        }
        else if (!strcmp(argv[i], "--angle_d3d"))
        {
            api = API::gl;
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE,
                         GLFW_ANGLE_PLATFORM_TYPE_D3D11);
            angle = true;
        }
        else if (!strcmp(argv[i], "--angle_vk"))
        {
            api = API::gl;
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE,
                         GLFW_ANGLE_PLATFORM_TYPE_VULKAN);
            angle = true;
        }
        else if (!strcmp(argv[i], "--angle_mtl"))
        {
            api = API::gl;
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE,
                         GLFW_ANGLE_PLATFORM_TYPE_METAL);
            angle = true;
        }
#endif
        else if (!strcmp(argv[i], "--skia"))
        {
            skia = true;
        }
        else if (sscanf(argv[i], "-a%i", &animation) == 1)
        {
            // Already updated animation.
        }
        else if (sscanf(argv[i], "-s%i", &stateMachine) == 1)
        {
            // Already updated stateMachine.
        }
        else if (sscanf(argv[i], "-h%i", &horzRepeat) == 1)
        {
            // Already updated horzRepeat.
        }
        else if (sscanf(argv[i], "-j%i", &downRepeat) == 1)
        {
            // Already updated downRepeat.
        }
        else if (sscanf(argv[i], "-k%i", &upRepeat) == 1)
        {
            // Already updated upRepeat.
        }
        else if (!strcmp(argv[i], "-p"))
        {
            paused = true;
        }
        else if (!strcmp(argv[i], "--d3d12Warp"))
        {
            options.d3d12UseWarpDevice = true;
        }
        else if (!strcmp(argv[i], "--atomic"))
        {
            forceAtomicMode = true;
        }
        else if (sscanf(argv[i], "--msaa%i", &msaa) == 1)
        {
            // Already updated msaa
        }
        else if (!strcmp(argv[i], "--core"))
        {
            options.coreFeaturesOnly = true;
        }
        else if (!strcmp(argv[i], "--validation"))
        {
            options.enableVulkanValidationLayers = true;
        }
        else if (!strcmp(argv[i], "--gpu") || !strcmp(argv[i], "-G"))
        {
            options.gpuNameFilter = argv[++i];
        }
        else if (!strncmp(argv[i], "--", 2))
        {
            fprintf(stderr, "Unknown command-line option %s\n", argv[i]);
            return 1;
        }
        else
        {
            rivName = argv[i];
        }
    }

    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize glfw.\n");
        return 1;
    }

    if (msaa > 0)
    {
        if (msaa > 1)
        {
            glfwWindowHint(GLFW_SAMPLES, msaa);
        }
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_DEPTH_BITS, 16);
    }
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    switch (api)
    {
        case API::metal:
        case API::d3d:
        case API::d3d12:
        case API::dawn:
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
            break;
        case API::vulkan:
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            break;
        case API::gl:
            if (angle)
            {
                glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            }
            else
            {
                glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            }
            break;
    }
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
    // glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    window = glfwCreateWindow(1600, 1600, "Rive Renderer", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "Failed to create window.\n");
        return -1;
    }

    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, mousemove_callback);
    glfwSetKeyCallback(window, key_callback);
    if (api == API::gl)
    {
        glfwMakeContextCurrent(window);
    }
    glfwShowWindow(window);

    switch (api)
    {
        case API::metal:
            fiddleContext = FiddleContext::MakeMetalPLS(options);
            break;
        case API::d3d:
            fiddleContext = FiddleContext::MakeD3DPLS(options);
            break;
        case API::d3d12:
            fiddleContext = FiddleContext::MakeD3D12PLS(options);
            break;
        case API::dawn:
            fiddleContext = FiddleContext::MakeDawnPLS(options);
            break;
        case API::vulkan:
            fiddleContext = FiddleContext::MakeVulkanPLS(options);
            break;
        case API::gl:
            fiddleContext =
                skia ? FiddleContext::MakeGLSkia() : FiddleContext::MakeGLPLS();
            break;
    }
    if (!fiddleContext)
    {
        fprintf(stderr, "Failed to create a fiddle context.\n");
        abort();
    }

#ifndef __EMSCRIPTEN__
    if (api == API::gl)
    {
        glfwSwapInterval(0);
    }
    while (!glfwWindowShouldClose(window))
    {
        riveMainLoop();
        fiddleContext->tick();
        if (api == API::gl)
        {
            glfwSwapBuffers(window);
        }
        if (!rivName.empty())
        {
            glfwPollEvents();
        }
        else
        {
            glfwWaitEvents();
        }
    }

    // We need to clear the riv scene (which can be holding on to render
    // resources) before releasing the fiddle context
    clear_scenes();
    rivFile = nullptr;

    fiddleContext = nullptr;
    glfwTerminate();
#endif

    return 0;
}

static void update_window_title(double fps,
                                int instances,
                                int width,
                                int height)
{
    std::ostringstream title;
    if (fps != 0)
    {
        title << '[' << fps << " FPS]";
    }
    if (instances > 1)
    {
        title << " (x" << instances << " instances)";
    }
    if (skia)
    {
        title << " | SKIA Renderer";
    }
    else
    {
        title << " | RIVE Renderer";
    }
    if (msaa)
    {
        title << " (msaa" << msaa << ')';
    }
    else if (forceAtomicMode)
    {
        title << " (atomic)";
    }
    title << " | " << width << " x " << height;
    glfwSetWindowTitle(window, title.str().c_str());
}

void riveMainLoop()
{
    RIVE_PROF_FRAME()
    RIVE_PROF_SCOPE()

#ifdef __EMSCRIPTEN__
    {
        // Fit the canvas to the browser window size.
        int windowWidth = window_inner_width();
        int windowHeight = window_inner_height();
        double devicePixelRatio = emscripten_get_device_pixel_ratio();
        int canvasExpectedWidth = windowWidth * devicePixelRatio;
        int canvasExpectedHeight = windowHeight * devicePixelRatio;
        int canvasWidth, canvasHeight;
        glfwGetFramebufferSize(window, &canvasWidth, &canvasHeight);
        if (canvasWidth != canvasExpectedWidth ||
            canvasHeight != canvasExpectedHeight)
        {
            glfwSetWindowSize(window,
                              canvasExpectedWidth,
                              canvasExpectedHeight);
            emscripten_set_element_css_size("#canvas",
                                            windowWidth,
                                            windowHeight);
        }
    }
#endif

    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (lastWidth != width || lastHeight != height)
    {
        printf("size changed to %ix%i\n", width, height);
        lastWidth = width;
        lastHeight = height;
        fiddleContext->onSizeChanged(window, width, height, msaa);
        renderer = fiddleContext->makeRenderer(width, height);
        needsTitleUpdate = true;
    }
    if (needsTitleUpdate)
    {
        update_window_title(0, 1, width, height);
        needsTitleUpdate = false;
    }

    if (!rivName.empty() && !rivFile)
    {
        std::ifstream rivStream(rivName, std::ios::binary);
        std::vector<uint8_t> rivBytes(std::istreambuf_iterator<char>(rivStream),
                                      {});
        rivFile = File::import(rivBytes, fiddleContext->factory());
    }

    // Call right before begin()
    if (hotloadShaders)
    {
        hotloadShaders = false;

#ifndef RIVE_BUILD_FOR_IOS
        // We want to build to /tmp/rive (or the correct equivalent)
        std::filesystem::path tempRiveDir =
            std::filesystem::temp_directory_path() / "rive";

        // Get the u8 version of the path (this is especially necessary on
        // windows where the native path character type is wchar_t, then
        // reinterpret_cast the char8_t pointer to char so we can append it to
        // our string.
        // Store the u8string result to extend its lifetime (need to
        // reinterpret_cast through a const char * pointer because u8string
        // returns a std::u8string in C++20 and newer, but we need it as a
        // "char" string)
        std::string tempRiveDirStr =
            reinterpret_cast<const char*>(tempRiveDir.u8string().c_str());

        std::string rebuildCommand = "sh rebuild_shaders.sh " + tempRiveDirStr;
        std::system(rebuildCommand.c_str());
#endif
        fiddleContext->hotloadShaders();
    }
    fiddleContext->begin({
        .renderTargetWidth = static_cast<uint32_t>(width),
        .renderTargetHeight = static_cast<uint32_t>(height),
        .clearColor = 0xff303030,
        .msaaSampleCount = msaa,
        .disableRasterOrdering = forceAtomicMode,
        .wireframe = wireframe,
        .fillsDisabled = disableFill,
        .strokesDisabled = disableStroke,
        .clockwiseFillOverride = clockwiseFill,
    });

    int instances = 1;
    if (rivFile)
    {
        instances = (1 + horzRepeat * 2) * (1 + upRepeat + downRepeat);
        if (artboards.size() != instances || scenes.size() != instances)
        {
            make_scenes(instances);
        }
        else if (!paused)
        {
            float dT = 1 / 120.f;

            if (!unlockedLogic)
            {
                double time = glfwGetTime();
                dT = time - fpsLastTimeLogic;
                fpsLastTimeLogic = time;
            }

            for (const auto& scene : scenes)
            {
                scene->advanceAndApply(dT);
            }
        }
        Mat2D m = computeAlignment(rive::Fit::contain,
                                   rive::Alignment::center,
                                   rive::AABB(0, 0, width, height),
                                   artboards.front()->bounds());
        renderer->save();
        m = Mat2D(scale, 0, 0, scale, translate.x, translate.y) * m;
        renderer->transform(m);
        float spacing = 200 / m.findMaxScale();
        auto scene = scenes.begin();
        for (int j = 0; j < upRepeat + 1 + downRepeat; ++j)
        {
            renderer->save();
            renderer->transform(Mat2D::fromTranslate(-spacing * horzRepeat,
                                                     (j - upRepeat) * spacing));
            for (int i = 0; i < horzRepeat * 2 + 1; ++i)
            {

                (*scene++)->draw(renderer.get());
                renderer->transform(Mat2D::fromTranslate(spacing, 0));
            }
            renderer->restore();
        }
        renderer->restore();
    }
    else
    {
        float2 p[9];
        for (int i = 0; i < 9; ++i)
        {
            p[i] = pts[i] + translate;
        }
        RawPath rawPath;
        rawPath.moveTo(p[0].x, p[0].y);
        rawPath.cubicTo(p[1].x, p[1].y, p[2].x, p[2].y, p[3].x, p[3].y);
        float2 c0 = simd::mix(p[3], p[4], float2(2 / 3.f));
        float2 c1 = simd::mix(p[5], p[4], float2(2 / 3.f));
        rawPath.cubicTo(c0.x, c0.y, c1.x, c1.y, p[5].x, p[5].y);
        rawPath.cubicTo(p[6].x, p[6].y, p[7].x, p[7].y, p[8].x, p[8].y);
        if (doClose)
        {
            rawPath.close();
        }

        Factory* factory = fiddleContext->factory();
        auto path = factory->makeRenderPath(rawPath, FillRule::clockwise);

        auto fillPaint = factory->makeRenderPaint();
        float feather = powf(1.5f, (1400 - pts[std::size(pts) - 1].y) / 75);
        if (feather > 1)
        {
            fillPaint->feather(feather);
        }
        fillPaint->color(0xd0ffffff);

        renderer->drawPath(path.get(), fillPaint.get());

        if (!disableStroke)
        {
            auto strokePaint = factory->makeRenderPaint();
            strokePaint->style(RenderPaintStyle::stroke);
            strokePaint->color(0x8000ffff);
            strokePaint->thickness(strokeWidth);
            if (feather > 1)
            {
                strokePaint->feather(feather);
            }

            strokePaint->join(join);
            strokePaint->cap(cap);
            renderer->drawPath(path.get(), strokePaint.get());

            // Draw the interactive points.
            auto pointPaint = factory->makeRenderPaint();
            pointPaint->style(RenderPaintStyle::stroke);
            pointPaint->color(0xff0000ff);
            pointPaint->thickness(14);
            pointPaint->cap(StrokeCap::round);
            pointPaint->join(StrokeJoin::round);

            auto pointPath = factory->makeEmptyRenderPath();
            for (int i : {1, 2, 4, 6, 7})
            {
                float2 pt = pts[i] + translate;
                pointPath->moveTo(pt.x, pt.y);
                pointPath->close();
            }
            renderer->drawPath(pointPath.get(), pointPaint.get());

            // Draw the feather control point.
            pointPaint->color(0xffff0000);
            pointPath = factory->makeEmptyRenderPath();
            float2 pt = pts[std::size(pts) - 1] + translate;
            pointPath->moveTo(pt.x, pt.y);
            renderer->drawPath(pointPath.get(), pointPaint.get());
        }
    }

    fiddleContext->end(window);

    if (rivFile)
    {
        // Count FPS.
        ++fpsFrames;
        double time = glfwGetTime();
        double fpsElapsed = time - fpsLastTime;
        if (fpsElapsed > 2)
        {
            int instances = (1 + horzRepeat * 2) * (1 + upRepeat + downRepeat);
            double fps = fpsLastTime == 0 ? 0 : fpsFrames / fpsElapsed;
            update_window_title(fps, instances, width, height);
            fpsFrames = 0;
            fpsLastTime = time;
        }
    }
}

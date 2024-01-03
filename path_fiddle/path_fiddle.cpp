#include "fiddle_context.hpp"

#include "rive/math/simd.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/layout.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/static_scene.hpp"

#include <fstream>
#include <iterator>
#include <vector>

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#ifdef RIVE_WEBGL
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <sstream>
#endif

using namespace rive;

static FiddleContextOptions s_options;
static GLFWwindow* s_window = nullptr;
static bool s_atomicMode = false;
static bool s_wireframe = false;
static bool s_disableFill = false;
static bool s_disableStroke = false;

static std::unique_ptr<FiddleContext> s_fiddleContext;

static float2 s_pts[] = {{260 + 2 * 100, 60 + 2 * 500},
                         {260 + 2 * 257, 60 + 2 * 233},
                         {260 + 2 * -100, 60 + 2 * 300},
                         {260 + 2 * 100, 60 + 2 * 200},
                         {260 + 2 * 250, 60 + 2 * 0},
                         {260 + 2 * 400, 60 + 2 * 200},
                         {260 + 2 * 213, 60 + 2 * 200},
                         {260 + 2 * 213, 60 + 2 * 300},
                         {260 + 2 * 391, 60 + 2 * 480}};
constexpr static int kNumInteractivePts = sizeof(s_pts) / sizeof(*s_pts);

static float s_strokeWidth = 70;

static float2 s_translate;
static float s_scale = 1;

static StrokeJoin s_join = StrokeJoin::miter;
static StrokeCap s_cap = StrokeCap::butt;

static bool s_doClose = false;
static bool s_paused = false;

static int s_dragIdx = -1;
static float2 s_dragLastPos;

static int s_animation = -1;
static int s_stateMachine = -1;
static int s_horzRepeat = 0;
static int s_upRepeat = 0;
static int s_downRepeat = 0;

std::unique_ptr<File> s_rivFile;
std::vector<std::unique_ptr<Artboard>> s_artboards;
std::vector<std::unique_ptr<Scene>> s_scenes;

static void make_scenes(size_t count)
{
    s_artboards.clear();
    s_scenes.clear();
    for (size_t i = 0; i < count; ++i)
    {
        auto artboard = s_rivFile->artboardDefault();
        std::unique_ptr<Scene> scene;
        if (s_stateMachine >= 0)
        {
            scene = artboard->stateMachineAt(s_stateMachine);
        }
        else if (s_animation >= 0)
        {
            scene = artboard->animationAt(s_animation);
        }
        else
        {
            scene = artboard->animationAt(0);
        }
        if (scene == nullptr)
        {
            // This is a riv without any animations or state machines. Just draw the artboard.
            scene = std::make_unique<StaticScene>(artboard.get());
        }
        scene->advanceAndApply(scene->durationSeconds() * i / count);
        s_artboards.push_back(std::move(artboard));
        s_scenes.push_back(std::move(scene));
    }
}

#ifdef RIVE_WEBGL
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

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    float dpiScale = s_fiddleContext->dpiScale(s_window);
    x *= dpiScale;
    y *= dpiScale;
    s_dragLastPos = float2{(float)x, (float)y};
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        s_dragIdx = -1;
        if (!s_rivFile)
        {
            for (int i = 0; i < kNumInteractivePts; ++i)
            {
                if (simd::all(simd::abs(s_dragLastPos - (s_pts[i] + s_translate)) < 100))
                {
                    s_dragIdx = i;
                    break;
                }
            }
        }
    }
}

static void mousemove_callback(GLFWwindow* window, double x, double y)
{
    float dpiScale = s_fiddleContext->dpiScale(s_window);
    x *= dpiScale;
    y *= dpiScale;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        float2 pos = float2{(float)x, (float)y};
        if (s_dragIdx >= 0)
        {
            s_pts[s_dragIdx] += (pos - s_dragLastPos);
        }
        else
        {
            s_translate += (pos - s_dragLastPos);
        }
        s_dragLastPos = pos;
    }
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    bool shift = mods & GLFW_MOD_SHIFT;
    if (action == GLFW_PRESS)
    {
        switch (key)
        {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, 1);
                break;
            case GLFW_KEY_A:
                s_atomicMode = !s_atomicMode;
                break;
            case GLFW_KEY_D:
                printf("static float s_scale = %f;\n", s_scale);
                printf("static float2 s_translate = {%f, %f};\n", s_translate.x, s_translate.y);
                printf("static float2 s_pts[] = {");
                for (int i = 0; i < kNumInteractivePts; i++)
                {
                    printf("{%g, %g}", s_pts[i].x, s_pts[i].y);
                    if (i < kNumInteractivePts - 1)
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
                s_fiddleContext->toggleZoomWindow();
                break;
            case GLFW_KEY_MINUS:
                s_strokeWidth /= 1.5f;
                break;
            case GLFW_KEY_EQUAL:
                s_strokeWidth *= 1.5f;
                break;
            case GLFW_KEY_W:
                s_wireframe = !s_wireframe;
                break;
            case GLFW_KEY_C:
                s_cap = static_cast<StrokeCap>((static_cast<int>(s_cap) + 1) % 3);
                break;
            case GLFW_KEY_O:
                s_doClose = !s_doClose;
                break;
            case GLFW_KEY_S:
                s_disableStroke = !s_disableStroke;
                break;
            case GLFW_KEY_F:
                s_disableFill = !s_disableFill;
                break;
            case GLFW_KEY_P:
                s_paused = !s_paused;
                break;
            case GLFW_KEY_H:
                if (!shift)
                    ++s_horzRepeat;
                else if (s_horzRepeat > 0)
                    --s_horzRepeat;
                break;
            case GLFW_KEY_K:
                if (!shift)
                    ++s_upRepeat;
                else if (s_upRepeat > 0)
                    --s_upRepeat;
                break;
            case GLFW_KEY_J:
                if (!s_rivFile)
                    s_join = static_cast<StrokeJoin>((static_cast<int>(s_join) + 1) % 3);
                else if (!shift)
                    ++s_downRepeat;
                else if (s_downRepeat > 0)
                    --s_downRepeat;
                break;
            case GLFW_KEY_UP:
            {
                float oldScale = s_scale;
                s_scale *= 1.25;
                double x = 0, y = 0;
                glfwGetCursorPos(window, &x, &y);
                float2 cursorPos = float2{(float)x, (float)y} * s_fiddleContext->dpiScale(s_window);
                s_translate = cursorPos + (s_translate - cursorPos) * s_scale / oldScale;
                break;
            }
            case GLFW_KEY_DOWN:
            {
                float oldScale = s_scale;
                s_scale /= 1.25;
                double x = 0, y = 0;
                glfwGetCursorPos(window, &x, &y);
                float2 cursorPos = float2{(float)x, (float)y} * s_fiddleContext->dpiScale(s_window);
                s_translate = cursorPos + (s_translate - cursorPos) * s_scale / oldScale;
                break;
            }
        }
    }
}

static void glfw_error_callback(int code, const char* message)
{
    printf("GLFW error: %i - %s\n", code, message);
}

bool skia = false;

enum class API
{
    gl,
    metal,
    d3d,
    dawn,
};

API api = API::gl;
bool angle = false;

int lastWidth = 0, lastHeight = 0;
double fpsLastTime = glfwGetTime();
int fpsFrames = 0;

std::unique_ptr<Renderer> renderer;

void riveMainLoop();

int main(int argc, const char** argv)
{
    // Cause stdout and stderr to print immediately without buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

#ifdef RIVE_WEBGL
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

    const char* rivName = nullptr;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--gl"))
        {
            api = API::gl;
        }
        else if (!strcmp(argv[i], "--metal"))
        {
            api = API::metal;
        }
        else if (!strcmp(argv[i], "--dawn"))
        {
            api = API::dawn;
        }
        else if (!strcmp(argv[i], "--d3d"))
        {
            api = API::d3d;
        }
        else if (!strcmp(argv[i], "--skia"))
        {
            skia = true;
        }
        else if (!strcmp(argv[i], "--angle_gl"))
        {
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_OPENGL);
            angle = true;
        }
        else if (!strcmp(argv[i], "--angle_d3d"))
        {
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_D3D11);
            angle = true;
        }
        else if (!strcmp(argv[i], "--angle_vk"))
        {
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_VULKAN);
            angle = true;
        }
        else if (!strcmp(argv[i], "--angle_mtl"))
        {
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_METAL);
            angle = true;
        }
        else if (!strcmp(argv[i], "--intel") || !strcmp(argv[i], "-i"))
        {
            s_options.preferIntelGPU = true;
        }
        else if (sscanf(argv[i], "-a%i", &s_animation))
        {}
        else if (sscanf(argv[i], "-s%i", &s_stateMachine))
        {}
        else if (sscanf(argv[i], "-h%i", &s_horzRepeat))
        {}
        else if (sscanf(argv[i], "-j%i", &s_downRepeat))
        {}
        else if (sscanf(argv[i], "-k%i", &s_upRepeat))
        {}
        else if (!strcmp(argv[i], "-p"))
        {
            s_paused = true;
        }
        else if (!strcmp(argv[i], "--atomic"))
        {
            s_atomicMode = true;
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

    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    switch (api)
    {
        case API::metal:
        case API::d3d:
        case API::dawn:
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
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
            }
            break;
    }
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
    // glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    s_window = glfwCreateWindow(1600, 1600, "Rive Renderer", nullptr, nullptr);
    if (!s_window)
    {
        glfwTerminate();
        fprintf(stderr, "Failed to create window.\n");
        return -1;
    }

    glfwSetWindowTitle(s_window, "Rive Renderer");
    glfwSetMouseButtonCallback(s_window, mouse_button_callback);
    glfwSetCursorPosCallback(s_window, mousemove_callback);
    glfwSetKeyCallback(s_window, key_callback);
    if (api == API::gl)
    {
        glfwMakeContextCurrent(s_window);
    }
    glfwShowWindow(s_window);

    switch (api)
    {
        case API::metal:
            if (skia)
            {
                fprintf(stderr, "Skia not supported on Metal yet.\n");
                break;
            }
            s_fiddleContext = FiddleContext::MakeMetalPLS(s_options);
            break;
        case API::d3d:
            if (skia)
            {
                fprintf(stderr, "Skia not supported on d3d yet.\n");
                break;
            }
            s_fiddleContext = FiddleContext::MakeD3DPLS(s_options);
            break;
        case API::dawn:
            if (skia)
            {
                fprintf(stderr, "Skia not supported on dawn yet.\n");
                break;
            }
            s_fiddleContext = FiddleContext::MakeDawnPLS(s_options);
            break;
        case API::gl:
            if (skia)
            {
                s_fiddleContext = FiddleContext::MakeGLSkia();
                break;
            }
            s_fiddleContext = FiddleContext::MakeGLPLS();
            break;
    }
    if (!s_fiddleContext)
    {
        fprintf(stderr, "Failed to create a fiddle context.\n");
        exit(-1);
    }
    Factory* factory = s_fiddleContext->factory();

    if (rivName)
    {
        std::ifstream rivStream(rivName, std::ios::binary);
        std::vector<uint8_t> rivBytes(std::istreambuf_iterator<char>(rivStream), {});
        s_rivFile = File::import(rivBytes, factory);
    }

#ifdef RIVE_DESKTOP_GL
    if (api == API::gl)
    {
        glfwSwapInterval(0);
    }
    while (!glfwWindowShouldClose(s_window))
    {
        riveMainLoop();
        s_fiddleContext->tick();
        if (api == API::gl)
        {
            glfwSwapBuffers(s_window);
        }
        if (s_rivFile)
        {
            glfwPollEvents();
        }
        else
        {
            glfwWaitEvents();
        }
    }
    glfwTerminate();
#endif

    return 0;
}

void riveMainLoop()
{
#ifdef RIVE_WEBGL
    {
        // Fit the canvas to the browser window size.
        int windowWidth = window_inner_width();
        int windowHeight = window_inner_height();
        double devicePixelRatio = emscripten_get_device_pixel_ratio();
        int canvasExpectedWidth = windowWidth * devicePixelRatio;
        int canvasExpectedHeight = windowHeight * devicePixelRatio;
        int canvasWidth, canvasHeight;
        glfwGetFramebufferSize(s_window, &canvasWidth, &canvasHeight);
        if (canvasWidth != canvasExpectedWidth || canvasHeight != canvasExpectedHeight)
        {
            glfwSetWindowSize(s_window, canvasExpectedWidth, canvasExpectedHeight);
            emscripten_set_element_css_size("#canvas", windowWidth, windowHeight);
        }
    }
#endif

    int width = 0, height = 0;
    glfwGetFramebufferSize(s_window, &width, &height);
    if (lastWidth != width || lastHeight != height)
    {
        printf("size changed to %ix%i\n", width, height);
        lastWidth = width;
        lastHeight = height;
        s_fiddleContext->onSizeChanged(s_window, width, height);
        renderer = s_fiddleContext->makeRenderer(width, height);
    }

    rive::pls::PLSRenderContext::FrameDescriptor frameDescriptor = {
        .clearColor = 0xff404040,
        .enableExperimentalAtomicMode = s_atomicMode,
        .wireframe = s_wireframe,
        .fillsDisabled = s_disableFill,
        .strokesDisabled = s_disableStroke,
    };
    s_fiddleContext->begin(std::move(frameDescriptor));

    int instances = 1;
    if (s_rivFile)
    {
        instances = (1 + s_horzRepeat * 2) * (1 + s_upRepeat + s_downRepeat);
        if (s_artboards.size() != instances || s_scenes.size() != instances)
        {
            make_scenes(instances);
        }
        else if (!s_paused)
        {
            for (const auto& scene : s_scenes)
            {
                scene->advanceAndApply(1 / 120.f);
            }
        }
        Mat2D m = computeAlignment(rive::Fit::contain,
                                   rive::Alignment::center,
                                   rive::AABB(0, 0, width, height),
                                   s_artboards.front()->bounds());
        renderer->save();
        m = Mat2D(s_scale, 0, 0, s_scale, s_translate.x, s_translate.y) * m;
        renderer->transform(m);
        float spacing = 200 / m.findMaxScale();
        auto scene = s_scenes.begin();
        for (int j = 0; j < s_upRepeat + 1 + s_downRepeat; ++j)
        {
            renderer->save();
            renderer->transform(
                Mat2D::fromTranslate(-spacing * s_horzRepeat, (j - s_upRepeat) * spacing));
            for (int i = 0; i < s_horzRepeat * 2 + 1; ++i)
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
            p[i] = s_pts[i] + s_translate;
        }
        RawPath rawPath;
        rawPath.moveTo(p[0].x, p[0].y);
        rawPath.cubicTo(p[1].x, p[1].y, p[2].x, p[2].y, p[3].x, p[3].y);
        float2 c0 = simd::mix(p[3], p[4], float2(2 / 3.f));
        float2 c1 = simd::mix(p[5], p[4], float2(2 / 3.f));
        rawPath.cubicTo(c0.x, c0.y, c1.x, c1.y, p[5].x, p[5].y);
        rawPath.cubicTo(p[6].x, p[6].y, p[7].x, p[7].y, p[8].x, p[8].y);
        if (s_doClose)
        {
            rawPath.close();
        }

        Factory* factory = s_fiddleContext->factory();
        auto path = factory->makeRenderPath(rawPath, FillRule::nonZero);

        auto fillPaint = factory->makeRenderPaint();
        fillPaint->style(RenderPaintStyle::fill);
        fillPaint->color(-1);

        auto strokePaint = factory->makeRenderPaint();
        strokePaint->style(RenderPaintStyle::stroke);
        strokePaint->color(0x8000ffff);
        strokePaint->thickness(s_strokeWidth);
        strokePaint->join(s_join);
        strokePaint->cap(s_cap);

        renderer->drawPath(path.get(), fillPaint.get());
        renderer->drawPath(path.get(), strokePaint.get());

        // Draw the interactive points.
        auto pointPaint = factory->makeRenderPaint();
        pointPaint->style(RenderPaintStyle::stroke);
        pointPaint->color(0xff0000ff);
        pointPaint->thickness(14);
        pointPaint->cap(StrokeCap::round);

        auto pointPath = factory->makeEmptyRenderPath();
        for (int i : {1, 2, 4, 6, 7})
        {
            float2 pt = s_pts[i] + s_translate;
            pointPath->moveTo(pt.x, pt.y);
        }

        renderer->drawPath(pointPath.get(), pointPaint.get());
    }

    s_fiddleContext->end(s_window);

    if (s_rivFile)
    {
        // Count FPS.
        ++fpsFrames;
        double time = glfwGetTime();
        double fpsElapsed = time - fpsLastTime;
        if (fpsElapsed > 2)
        {
            int instances = (1 + s_horzRepeat * 2) * (1 + s_upRepeat + s_downRepeat);
            double fps = fpsFrames / fpsElapsed;
            // printf("%.1f FPS\n", fps);
            // fflush(stdout);
            char title[100];
            if (instances > 1)
            {
                snprintf(title,
                         sizeof(title),
                         "[%.1f FPS] (x%i instances) | [%.1f ms] | %s Renderer | %i x %i",
                         fps,
                         instances,
                         1000 / fps,
                         skia ? "Skia" : "Rive",
                         width,
                         height);
            }
            else
            {
                snprintf(title,
                         sizeof(title),
                         "[%.1f FPS] | [%.1f ms] | %s Renderer | %i x %i",
                         fps,
                         1000 / fps,
                         skia ? "Skia" : "Rive",
                         width,
                         height);
            }
            glfwSetWindowTitle(s_window, title);
            fpsFrames = 0;
            fpsLastTime = time;
            s_fiddleContext->shrinkGPUResourcesToFit();
        }
    }
}

/*
 * Copyright 2022 Rive
 */

#include "rive/pls/gl/gles3.hpp"

#ifdef RIVE_WASM
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>
#include <chrono>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

constexpr static char vs[] = R"(#version 300 es
precision highp float;
uniform vec2 window;
uniform float T;
layout(location=0) in vec3 bubble;
layout(location=1) in vec2 speed;
layout(location=2) in vec4 incolor;
out vec2 coord;
out vec4 color;
void main() {
    vec2 offset = vec2((gl_VertexID & 1) == 0 ? -1.0 : 1.0, (gl_VertexID & 2) == 0 ? -1.0 : 1.0);
    coord = offset;
    color = incolor;
    float r = bubble.z;
    vec2 center = bubble.xy + speed * T;
    vec2 span = window - 2.0 * r;
    center = span - abs(span - mod(center - r, span * 2.0)) + r;
    gl_Position.xy = (center + offset * r) * 2.0 / window - 1.0;
    gl_Position.zw = vec2(0, 1);
})";

constexpr static char fs[] = R"(#version 300 es
#extension GL_ANGLE_shader_pixel_local_storage : require

precision mediump float;

in vec2 coord;
in vec4 color;

layout(binding=0, rgba8) uniform mediump pixelLocalANGLE framebuffer;

float color_burn_component(vec2 s, vec2 d) {
    if (d.y == d.x) {
        return s.y*d.y + s.x*(1.0 - d.y) + d.x*(1.0 - s.y);
    } else if (s.x == 0.0) {
        return d.x*(1.0 - s.y);
    } else {
        float delta = max(0.0, d.y - (d.y - d.x)*s.y / s.x);
        return delta*s.y + s.x*(1.0 - d.y) + d.x*(1.0 - s.y);
    }
}

vec4 blend(vec4 src, vec4 dst) {
    return (dst.a == 0.0) ? src : vec4(color_burn_component(src.ra, dst.ra),
                                       color_burn_component(src.ga, dst.ga),
                                       color_burn_component(src.ba, dst.ba),
                                       src.a + (1.0 - src.a)*dst.a);
}

void main() {
    float f = coord.x * coord.x + coord.y * coord.y - 1.0;
    float coverage = clamp(.5 - f/fwidth(f), 0.0, 1.0);
    vec4 s = vec4(color.rgb, 1) * (color.a * mix(.25, 1.0, dot(coord, coord)) * coverage);
    vec4 d = pixelLocalLoadANGLE(framebuffer);
    pixelLocalStoreANGLE(framebuffer, blend(s, d));
})";

static bool compile_and_attach_shader(GLuint program, GLuint type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> infoLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
        fprintf(stderr, "Failed to compile shader\n");
        int l = 1;
        std::stringstream stream(source);
        std::string lineStr;
        while (std::getline(stream, lineStr, '\n'))
        {
            fprintf(stderr, "%4i| %s\n", l++, lineStr.c_str());
        }
        fprintf(stderr, "%s\n", &infoLog[0]);
        fflush(stderr);
        glDeleteShader(shader);
        return false;
    }
    glAttachShader(program, shader);
    glDeleteShader(shader);
    return true;
}

static bool link_program(GLuint program)
{
    glLinkProgram(program);
    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
        fprintf(stderr, "Failed to link program %s\n", &infoLog[0]);
        fflush(stderr);
        return false;
    }
    return true;
}

#ifdef RIVE_DESKTOP_GL
static void GLAPIENTRY err_msg_callback(GLenum source,
                                        GLenum type,
                                        GLuint id,
                                        GLenum severity,
                                        GLsizei length,
                                        const GLchar* message,
                                        const void* userParam)
{
    if (type == GL_DEBUG_TYPE_ERROR)
    {
        printf("GL ERROR: %s\n", message);
    }
    else if (type == GL_DEBUG_TYPE_PERFORMANCE)
    {
        printf("GL PERF: %s\n", message);
    }
}
#endif

static int W = 2048;
static int H = 2048;

struct Bubble
{
    float x, y, r;
    float dx, dy;
    std::array<float, 4> color;
};

static float lerp(float a, float b, float t) { return a + (b - a) * t; }
static float frand() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }
static float frand(float lo, float hi) { return lerp(lo, hi, frand()); }

double now()
{
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::time_point_cast<std::chrono::nanoseconds>(now).time_since_epoch().count() *
           1e-9;
}

#ifdef RIVE_WASM
EM_JS(int, window_inner_width, (), { return window["innerWidth"]; });
EM_JS(int, window_inner_height, (), { return window["innerHeight"]; });
#endif

constexpr int n = 800;

GLFWwindow* window;

GLint uniformWindow;
GLint uniformT;

GLuint tex = 0;

GLuint blitFBO;
GLuint renderFBO;

GLenum clearOp = GL_LOAD_OP_CLEAR_WEBGL;
GLenum keepOp = GL_STORE_OP_STORE_WEBGL;

int totalFrames = 0;
int frames = 0;
double start = now();
int lastWidth = 0, lastHeight = 0;

static void mainLoop();

int main(int argc, const char* argv[])
{
#if RIVE_WASM
    emscripten_set_main_loop(mainLoop, 0, false);
#endif

#ifdef RIVE_DESKTOP_GL
    // Select the ANGLE backend.
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--gl"))
        {
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_OPENGL);
        }
        else if (!strcmp(argv[i], "--gles"))
        {
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_OPENGLES);
        }
        else if (!strcmp(argv[i], "--d3d"))
        {
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_D3D11);
        }
        else if (!strcmp(argv[i], "--vk"))
        {
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_VULKAN);
        }
        else if (!strcmp(argv[i], "--mtl"))
        {
            glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_METAL);
        }
    }
#endif

    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize glfw.\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    window = glfwCreateWindow(W, H, "Rive Bubbles", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "Failed to create window.\n");
        return -1;
    }

    glfwSetWindowTitle(window, "Rive Bubbles");
    glfwMakeContextCurrent(window);

#ifdef RIVE_DESKTOP_GL
    // Load the OpenGL API using glad.
    if (!gladLoadCustomLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Failed to initialize glad.\n");
        return -1;
    }
    glfwSwapInterval(0);
#endif

    printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
    printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
    fflush(stdout);

#ifdef RIVE_DESKTOP_GL
    if (!GLAD_GL_ANGLE_shader_pixel_local_storage_coherent)
    {
        printf("ANGLE_shader_pixel_local_storage_coherent not supported\n");
        return -1;
    }

    if (GLAD_GL_KHR_debug)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
        glDebugMessageCallbackKHR(&err_msg_callback, nullptr);
    }
#endif

#ifdef RIVE_WASM
    if (!emscripten_webgl_enable_WEBGL_shader_pixel_local_storage(
            emscripten_webgl_get_current_context()))
    {
        printf("WEBGL_shader_pixel_local_storage is not supported\n");
        return -1;
    }

    if (!emscripten_webgl_shader_pixel_local_storage_is_coherent())
    {
        printf("WEBGL_shader_pixel_local_storage is not coherent\n");
        return -1;
    }
#endif

    GLuint program = glCreateProgram();
    if (!compile_and_attach_shader(program, GL_VERTEX_SHADER, vs) ||
        !compile_and_attach_shader(program, GL_FRAGMENT_SHADER, fs) || !link_program(program))
    {
        return -1;
    }
    glUseProgram(program);
    uniformWindow = glGetUniformLocation(program, "window");
    uniformT = glGetUniformLocation(program, "T");

    // Generate n bubbles.
    Bubble bubbles[n];
    for (Bubble& bubble : bubbles)
    {
        float r = lerp(.1f, .3f, powf(frand(), 4));
        bubble.x = (frand(-1 + r, 1 - r) + 1) * 1024.f;
        bubble.y = (frand(-1 + r, 1 - r) + 1) * 1024.f;
        bubble.r = r * 1024.f;
        bubble.dx = (frand() - .5) * .02 * 1024.f;
        bubble.dy = (frand() - .5) * .02 * 1024.f;
        // bubble.da = 0; //(frand() - .5) * .03;
        bubble.color = {frand(.5, 1), frand(.5, 1), frand(.5, 1), frand(.75, 1)};
    }

    GLuint bubbleBuff;
    glGenBuffers(1, &bubbleBuff);
    glBindBuffer(GL_ARRAY_BUFFER, bubbleBuff);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bubbles), bubbles, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Bubble), 0);
    glVertexAttribDivisor(0, 1);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(Bubble),
                          reinterpret_cast<const void*>(offsetof(Bubble, dx)));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,
                          4,
                          GL_FLOAT,
                          GL_TRUE,
                          sizeof(Bubble),
                          reinterpret_cast<const void*>(offsetof(Bubble, color)));
    glVertexAttribDivisor(2, 1);

    glGenFramebuffers(1, &blitFBO);

    glGenFramebuffers(1, &renderFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderFBO);
    glDrawBuffers(0, nullptr);
    glDisable(GL_DITHER);

    float clearColor[] = {.1f, .1f, .1f, .1f};
    glFramebufferPixelLocalClearValuefvWEBGL(0, clearColor);

#if RIVE_DESKTOP_GL
    while (!glfwWindowShouldClose(window))
    {
        mainLoop();
        glfwSwapBuffers(window);
    }

    glfwTerminate();
#endif

    return 0;
}

static void mainLoop()
{
#ifdef RIVE_WASM
    {
        // Fit the canvas to the browser window size.
        int windowWidth = window_inner_width();
        int windowHeight = window_inner_height();
        double devicePixelRatio = emscripten_get_device_pixel_ratio();
        int canvasExpectedWidth = windowWidth * devicePixelRatio;
        int canvasExpectedHeight = windowHeight * devicePixelRatio;
        int canvasWidth, canvasHeight;
        glfwGetFramebufferSize(window, &canvasWidth, &canvasHeight);
        if (canvasWidth != canvasExpectedWidth || canvasHeight != canvasExpectedHeight)
        {
            glfwSetWindowSize(window, canvasExpectedWidth, canvasExpectedHeight);
            emscripten_set_element_css_size("#canvas", windowWidth, windowHeight);
        }
    }
#endif

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (lastWidth != width || lastHeight != height)
    {
        printf("rendering %i bubbles at %i x %i\n", n, width, height);
        glViewport(0, 0, width, height);
        glUniform2f(uniformWindow, width, height);

        glDeleteTextures(1, &tex);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

        glBindFramebuffer(GL_FRAMEBUFFER, blitFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, renderFBO);
        glFramebufferTexturePixelLocalStorageWEBGL(0, tex, 0, 0);
        glDrawBuffers(0, nullptr);

        lastWidth = width;
        lastHeight = height;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, renderFBO);
    glBeginPixelLocalStorageWEBGL(1, &clearOp);
    glUniform1f(uniformT, totalFrames++);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n);
    glEndPixelLocalStorageWEBGL(1, &keepOp);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, blitFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    ++frames;
    double end = now();
    double seconds = end - start;
    if (seconds >= 2)
    {
        printf("%f fps\n", frames / seconds);
        fflush(stdout);
        frames = 0;
        start = end;
    }

    glfwPollEvents();
}

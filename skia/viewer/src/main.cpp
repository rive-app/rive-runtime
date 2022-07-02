/*
 * Copyright 2022 Rive
 */

// Makes ure gl3w is included before glfw3
#include "GL/gl3w.h"

#define SK_GL
#include "GLFW/glfw3.h"

#include "GrBackendSurface.h"
#include "GrDirectContext.h"
#include "SkCanvas.h"
#include "SkColorSpace.h"
#include "SkSurface.h"

#include "gl/GrGLInterface.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "../src/render_counter.hpp"

#include <cmath>
#include <stdio.h>

#include "viewer_content.hpp"

int lastScreenWidth = 0, lastScreenHeight = 0;

std::unique_ptr<ViewerContent> gContent;

std::vector<uint8_t> ViewerContent::LoadFile(const char filename[]) {
    std::vector<uint8_t> bytes;

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Can't find file: %s\n", filename);
        return bytes;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    bytes.resize(size);
    size_t bytesRead = fread(bytes.data(), 1, size, fp);
    fclose(fp);

    if (bytesRead != size) {
        fprintf(stderr, "Failed to read all of %s\n", filename);
        bytes.resize(0);
    }
    return bytes;
}

static void glfwCursorPosCallback(GLFWwindow* window, double x, double y) {
    if (gContent) {
        float xscale, yscale;
        glfwGetWindowContentScale(window, &xscale, &yscale);
        gContent->handlePointerMove(x * xscale, y * yscale);
    }
}

void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (gContent) {
        switch (action) {
            case GLFW_PRESS:
                gContent->handlePointerDown();
                break;
            case GLFW_RELEASE:
                gContent->handlePointerUp();
                break;
        }
    }
}

void glfwErrorCallback(int error, const char* description) { puts(description); }

void glfwDropCallback(GLFWwindow* window, int count, const char** paths) {
    // Just get the last dropped file for now...
    const char* filename = paths[count - 1];

    auto newContent = ViewerContent::FindHandler(filename);
    if (newContent) {
        gContent = std::move(newContent);
        gContent->handleResize(lastScreenWidth, lastScreenHeight);
    } else {
        fprintf(stderr, "No handler found for %s\n", filename);
    }
}

int main() {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize glfw.\n");
        return 1;
    }
    glfwSetErrorCallback(glfwErrorCallback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Rive Viewer", NULL, NULL);
    if (window == nullptr) {
        fprintf(stderr, "Failed to make window or GL.\n");
        glfwTerminate();
        return 1;
    }

    glfwSetDropCallback(window, glfwDropCallback);
    glfwSetCursorPosCallback(window, glfwCursorPosCallback);
    glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);
    glfwMakeContextCurrent(window);
    if (gl3wInit() != 0) {
        fprintf(stderr, "Failed to make initialize gl3w.\n");
        glfwTerminate();
        return 1;
    }
    // Enable VSYNC.
    glfwSwapInterval(1);

    // Setup ImGui
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
    io.Fonts->AddFontDefault();

    // Setup Skia
    GrContextOptions options;
    sk_sp<GrDirectContext> context = GrDirectContext::MakeGL(nullptr, options);
    GrGLFramebufferInfo framebufferInfo;
    framebufferInfo.fFBOID = 0;
    framebufferInfo.fFormat = GL_RGBA8;

    sk_sp<SkSurface> surface;
    SkCanvas* canvas = nullptr;

    // Render loop.
    int width = 0, height = 0;
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwGetFramebufferSize(window, &width, &height);

        // Update surface.
        if (!surface || width != lastScreenWidth || height != lastScreenHeight) {
            lastScreenWidth = width;
            lastScreenHeight = height;

            if (gContent) {
                gContent->handleResize(width, height);
            }

            SkColorType colorType =
                kRGBA_8888_SkColorType; // GrColorTypeToSkColorType(GrPixelConfigToColorType(kRGBA_8888_GrPixelConfig));
            //
            // if (kRGBA_8888_GrPixelConfig == kSkia8888_GrPixelConfig)
            // {
            // 	colorType = kRGBA_8888_SkColorType;
            // }
            // else
            // {
            // 	colorType = kBGRA_8888_SkColorType;
            // }

            GrBackendRenderTarget backendRenderTarget(width,
                                                      height,
                                                      0, // sample count
                                                      0, // stencil bits
                                                      framebufferInfo);

            surface = SkSurface::MakeFromBackendRenderTarget(context.get(),
                                                             backendRenderTarget,
                                                             kBottomLeft_GrSurfaceOrigin,
                                                             colorType,
                                                             nullptr,
                                                             nullptr);
            if (!surface) {
                fprintf(stderr, "Failed to create Skia surface\n");
                return 1;
            }
            canvas = surface->getCanvas();
        }

        double time = glfwGetTime();
        float elapsed = (float)(time - lastTime);
        lastTime = time;

        // Clear screen.
        SkPaint paint;
        paint.setColor(SK_ColorDKGRAY);
        canvas->drawPaint(paint);

        if (gContent) {
            SkAutoCanvasRestore acr(canvas, true);
            gContent->handleDraw(canvas, elapsed);
        }

        context->flush();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (gContent) {
           gContent->handleImgui();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    gContent = nullptr;  // force delete now, so we can clean up

    // Cleanup Skia.
    surface = nullptr;
    context = nullptr;

    ImGui_ImplGlfw_Shutdown();

    // Cleanup GLFW.
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

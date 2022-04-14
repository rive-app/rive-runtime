#ifdef TESTING

// Don't compile this file as part of the "tests" project.

#else

#include "goldens_grid.hpp"

#include "GLFW/glfw3.h"
#include "skia_renderer.hpp"
#include "skia/include/core/SkSurface.h"
#include "skia/include/gpu/GrDirectContext.h"
#include "skia/include/gpu/gl/GrGLInterface.h"
#include "skia/include/gpu/gl/GrGLAssembleInterface.h"
#include "skia/third_party/externals/libpng/png.h"
#include "tools/write_png_file.hpp"
#include <iostream>
#include <string>

static GrGLFuncPtr get_proc_address(void* ctx, const char name[]) {
    return glfwGetProcAddress(name);
}

static bool read_line(std::string& line) {
    if (!std::getline(std::cin, line)) {
        return false;
    }
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    return true;
}

int main(int argc, const char* argv[]) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize glfw.\n");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SW, SH, "Rive Goldens", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        fprintf(stderr, "Failed to create window.\n");
        return -1;
    }

    glfwMakeContextCurrent(window);

    auto grCtx = GrDirectContext::MakeGL(GrGLMakeAssembledInterface(nullptr, get_proc_address));

    GrBackendRenderTarget backendRT(
        SW, SH, 1 /*samples*/, 0 /*stencilBits*/, {0 /*fbo 0*/, GL_RGBA8});

    SkSurfaceProps surfProps(0, kUnknown_SkPixelGeometry);

    auto surf = SkSurface::MakeFromBackendRenderTarget(grCtx.get(),
                                                       backendRT,
                                                       kBottomLeft_GrSurfaceOrigin,
                                                       kRGBA_8888_SkColorType,
                                                       nullptr,
                                                       &surfProps);
    auto canvas = surf->getCanvas();
    rive::SkiaRenderer renderer(canvas);

    std::string source, artboard, animation, destination;
    while (read_line(source) && read_line(artboard) && read_line(animation) &&
           read_line(destination)) {
        try {
            canvas->clear(SK_ColorWHITE);

            renderer.save();

            RenderGoldensGrid(&renderer, source.c_str(), artboard.c_str(), animation.c_str());

            renderer.restore();
            canvas->flush();

            std::vector<uint8_t> pixels(SH * SW * 4);
            glReadPixels(0, 0, SW, SH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            WritePNGFile(pixels.data(), SW, SH, true, destination.c_str());

            glfwSwapBuffers(window);
        } catch (const char* msg) {
            fprintf(stderr, "%s: error: %s\n", source.c_str(), msg);
            fflush(stderr);
        } catch (...) {
            fprintf(stderr, "error rendering %s\n", source.c_str());
            fflush(stderr);
        }
        fprintf(stdout, "finished %s\n", source.c_str());
        fflush(stdout);
    }

    return 0;
}

#endif

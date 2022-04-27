
// Makes ure gl3w is included before glfw3
#include "GL/gl3w.h"

#define SK_GL
#include "GLFW/glfw3.h"

#include "GrBackendSurface.h"
#include "GrDirectContext.h"
#include "SkCanvas.h"
#include "SkColorSpace.h"
#include "SkSurface.h"
#include "SkTypes.h"
#include "gl/GrGLInterface.h"

#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_number.hpp"
#include "rive/animation/state_machine_bool.hpp"
#include "rive/animation/state_machine_trigger.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/layout.hpp"
#include "rive/math/aabb.hpp"
#include "skia_factory.hpp"
#include "skia_renderer.hpp"

#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include <cmath>
#include <stdio.h>

rive::SkiaFactory skiaFactory;

std::string filename;
std::unique_ptr<rive::File> currentFile;
std::unique_ptr<rive::ArtboardInstance> artboardInstance;
std::unique_ptr<rive::StateMachineInstance> stateMachineInstance;
std::unique_ptr<rive::LinearAnimationInstance> animationInstance;

// ImGui wants raw pointers to names, but our public API returns
// names as strings (by value), so we cache these names each time we
// load a file
std::vector<std::string> animationNames;
std::vector<std::string> stateMachineNames;

// We hold onto the file's bytes for the lifetime of the file, in case we want
// to change animations or state-machines, we just rebuild the rive::File from
// it.
std::vector<uint8_t> fileBytes;

int animationIndex = 0;
int stateMachineIndex = -1;

static void loadNames(const rive::Artboard* ab) {
    animationNames.clear();
    stateMachineNames.clear();
    if (ab) {
        for (size_t i = 0; i < ab->animationCount(); ++i) {
            animationNames.push_back(ab->animationNameAt(i));
        }
        for (size_t i = 0; i < ab->stateMachineCount(); ++i) {
            stateMachineNames.push_back(ab->stateMachineNameAt(i));
        }
    }
}

void initStateMachine(int index) {
    stateMachineIndex = index;
    animationIndex = -1;
    assert(fileBytes.size() != 0);
    auto file = rive::File::import(rive::toSpan(fileBytes), &skiaFactory);
    if (!file) {
        fileBytes.clear();
        fprintf(stderr, "failed to import file\n");
        return;
    }
    animationInstance = nullptr;
    stateMachineInstance = nullptr;
    artboardInstance = nullptr;

    currentFile = std::move(file);
    artboardInstance = currentFile->artboardDefault();
    artboardInstance->advance(0.0f);
    loadNames(artboardInstance.get());

    if (index >= 0 && index < artboardInstance->stateMachineCount()) {
        stateMachineInstance = artboardInstance->stateMachineAt(index);
    }
}

void initAnimation(int index) {
    animationIndex = index;
    stateMachineIndex = -1;
    assert(fileBytes.size() != 0);
    auto file = rive::File::import(rive::toSpan(fileBytes), &skiaFactory);
    if (!file) {
        fileBytes.clear();
        fprintf(stderr, "failed to import file\n");
        return;
    }
    animationInstance = nullptr;
    stateMachineInstance = nullptr;
    artboardInstance = nullptr;

    currentFile = std::move(file);
    artboardInstance = currentFile->artboardDefault();
    artboardInstance->advance(0.0f);
    loadNames(artboardInstance.get());

    if (index >= 0 && index < artboardInstance->animationCount()) {
        animationInstance = artboardInstance->animationAt(index);
    }
}

rive::Mat2D gInverseViewTransform;
rive::Vec2D lastWorldMouse;
static void glfwCursorPosCallback(GLFWwindow* window, double x, double y) {
    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    lastWorldMouse = gInverseViewTransform * rive::Vec2D(x * xscale, y * yscale);
    if (stateMachineInstance != nullptr) {
        stateMachineInstance->pointerMove(lastWorldMouse);
    }
}
void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (stateMachineInstance != nullptr) {
        switch (action) {
            case GLFW_PRESS:
                stateMachineInstance->pointerDown(lastWorldMouse);
                break;
            case GLFW_RELEASE:
                stateMachineInstance->pointerUp(lastWorldMouse);
                break;
        }
    }
}

void glfwErrorCallback(int error, const char* description) { puts(description); }

void glfwDropCallback(GLFWwindow* window, int count, const char** paths) {
    // Just get the last dropped file for now...
    filename = paths[count - 1];

    FILE* fp = fopen(filename.c_str(), "rb");
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fileBytes.resize(size);
    if (fread(fileBytes.data(), 1, size, fp) != size) {
        fileBytes.clear();
        fprintf(stderr, "failed to read all of %s\n", filename.c_str());
        return;
    }
    initAnimation(0);
}

static void test_messages(rive::Artboard* artboard) {
    rive::Message msg;
    int i = 0;
#ifdef DEBUG
    bool hasAny = artboard->hasMessages();
#endif

    while (artboard->nextMessage(&msg)) {
        printf("-- message[%d]: '%s'\n", i, msg.m_Str.c_str());
        i += 1;
    }
    assert((hasAny && i > 0) || (!hasAny && i == 0));
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
    int lastScreenWidth = 0, lastScreenHeight = 0;
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwGetFramebufferSize(window, &width, &height);

        // Update surface.
        if (!surface || width != lastScreenWidth || height != lastScreenHeight) {
            lastScreenWidth = width;
            lastScreenHeight = height;

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

        if (artboardInstance != nullptr) {
            if (animationInstance != nullptr) {
                animationInstance->advance(elapsed);
                animationInstance->apply();
            } else if (stateMachineInstance != nullptr) {
                stateMachineInstance->advance(elapsed);
            }
            artboardInstance->advance(elapsed);

            rive::SkiaRenderer renderer(canvas);
            renderer.save();

            auto viewTransform = rive::computeAlignment(rive::Fit::contain,
                                                        rive::Alignment::center,
                                                        rive::AABB(0, 0, width, height),
                                                        artboardInstance->bounds());
            renderer.transform(viewTransform);
            // Store the inverse view so we can later go from screen to world.
            gInverseViewTransform = viewTransform.invertOrIdentity();
            // post_mouse_event(artboard.get(), canvas->getTotalMatrix());

            artboardInstance->draw(&renderer);
            renderer.restore();
        }

        context->flush();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (artboardInstance != nullptr) {
            ImGui::Begin(filename.c_str(), nullptr);
            if (ImGui::ListBox(
                    "Animations",
                    &animationIndex,
                    [](void* data, int index, const char** name) {
                        *name = animationNames[index].c_str();
                        return true;
                    },
                    artboardInstance.get(),
                    animationNames.size(),
                    4))
            {
                stateMachineIndex = -1;
                initAnimation(animationIndex);
            }
            if (ImGui::ListBox(
                    "State Machines",
                    &stateMachineIndex,
                    [](void* data, int index, const char** name) {
                        *name = stateMachineNames[index].c_str();
                        return true;
                    },
                    artboardInstance.get(),
                    stateMachineNames.size(),
                    4))
            {
                animationIndex = -1;
                initStateMachine(stateMachineIndex);
            }
            if (stateMachineInstance != nullptr) {

                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.6666);

                for (int i = 0; i < stateMachineInstance->inputCount(); i++) {
                    auto inputInstance = stateMachineInstance->input(i);

                    if (inputInstance->input()->is<rive::StateMachineNumber>()) {
                        // ImGui requires names as id's, use ## to hide the
                        // label but still give it an id.
                        char label[256];
                        snprintf(label, 256, "##%u", i);

                        auto number = static_cast<rive::SMINumber*>(inputInstance);
                        float v = number->value();
                        ImGui::InputFloat(label, &v, 1.0f, 2.0f, "%.3f");
                        number->value(v);
                        ImGui::NextColumn();
                    } else if (inputInstance->input()->is<rive::StateMachineTrigger>()) {
                        // ImGui requires names as id's, use ## to hide the
                        // label but still give it an id.
                        char label[256];
                        snprintf(label, 256, "Fire##%u", i);
                        if (ImGui::Button(label)) {
                            auto trigger = static_cast<rive::SMITrigger*>(inputInstance);
                            trigger->fire();
                        }
                        ImGui::NextColumn();
                    } else if (inputInstance->input()->is<rive::StateMachineBool>()) {
                        // ImGui requires names as id's, use ## to hide the
                        // label but still give it an id.
                        char label[256];
                        snprintf(label, 256, "##%u", i);
                        auto boolInput = static_cast<rive::SMIBool*>(inputInstance);
                        bool value = boolInput->value();

                        ImGui::Checkbox(label, &value);
                        boolInput->value(value);
                        ImGui::NextColumn();
                    }
                    ImGui::Text("%s", inputInstance->input()->name().c_str());
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);
            }
            ImGui::End();
            test_messages(artboardInstance.get());

        } else {
            ImGui::Text("Drop a .riv file to preview.");
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup Skia.
    surface = nullptr;
    context = nullptr;

    ImGui_ImplGlfw_Shutdown();

    // Cleanup GLFW.
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

// Viewer & Rive
#include "viewer/viewer.hpp"
#include "viewer/viewer_content.hpp"
#include "rive/shapes/paint/color.hpp"

// Graphics and UI abstraction
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "imgui.h"
#include "util/sokol_imgui.h"

// Std lib
#include <stdio.h>
#include <memory>

#ifdef RIVE_RENDERER_SKIA
#include "viewer/skia/viewer_skia_renderer.hpp"
sk_sp<GrDirectContext> g_SkiaContext;
sk_sp<SkSurface> g_SkiaSurface;
#endif
#ifdef RIVE_RENDERER_TESS
#include "rive/tess/sokol/sokol_tess_renderer.hpp"
std::unique_ptr<rive::SokolTessRenderer> g_TessRenderer;
#endif
std::unique_ptr<ViewerContent> g_Content = ViewerContent::TrimPath("");
static struct { sg_pass_action pass_action; } state;

void displayStats();

static const int backgroundColor = rive::colorARGB(255, 22, 22, 22);

static void init(void) {
    sg_desc descriptor = {.context = sapp_sgcontext()};
    sg_setup(&descriptor);
    simgui_desc_t imguiDescriptor = {
        .write_alpha_channel = true,
    };
    simgui_setup(&imguiDescriptor);

#if defined(SK_METAL)
    // Skia is layered behind the Sokol view, so we need to make sure Sokol
    // clears transparent. Skia will draw the background.
    state.pass_action =
        (sg_pass_action){.colors[0] = {.action = SG_ACTION_CLEAR, .value = {0.0f, 0.0, 0.0f, 0.0}}};
#elif defined(SK_GL)
    // Skia commands are issued to the same GL context before Sokol, so we need
    // to make sure Sokol does not clear the buffer.
    state.pass_action = (sg_pass_action){.colors[0] = {.action = SG_ACTION_DONTCARE}};
#else
    // In every other case, Sokol is in full control, so let's clear to our bg
    // color.
    state.pass_action =
        (sg_pass_action){.colors[0] = {.action = SG_ACTION_CLEAR,
                                       .value = {rive::colorRed(backgroundColor) / 255.0f,
                                                 rive::colorGreen(backgroundColor) / 255.0f,
                                                 rive::colorBlue(backgroundColor) / 255.0f,
                                                 rive::colorOpacity(backgroundColor)}}};
#endif

#ifdef RIVE_RENDERER_SKIA
    g_SkiaContext = makeSkiaContext();
    if (!g_SkiaContext) {
        fprintf(stderr, "failed to create skia context\n");
        sapp_quit();
    }
#endif
#ifdef RIVE_RENDERER_TESS
    g_TessRenderer = std::make_unique<rive::SokolTessRenderer>();
    g_TessRenderer->orthographicProjection(0.0f, sapp_width(), sapp_height(), 0.0f, 0.0f, 1.0f);
#endif
}

static void frame(void) {
#ifdef RIVE_RENDERER_SKIA
    g_SkiaContext->resetContext();
    g_SkiaSurface = makeSkiaSurface(g_SkiaContext.get(), sapp_width(), sapp_height());
    SkCanvas* canvas = g_SkiaSurface->getCanvas();
    SkPaint paint;
    paint.setColor(backgroundColor);
    canvas->drawPaint(paint);

    ViewerSkiaRenderer skiaRenderer(canvas);
    if (g_Content) {
        g_Content->handleDraw(&skiaRenderer, sapp_frame_duration());
    }

    canvas->flush();
    skiaPresentSurface(g_SkiaSurface);
    sg_reset_state_cache();
#endif

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());

#ifdef RIVE_RENDERER_TESS
    if (g_Content) {
        g_Content->handleDraw(g_TessRenderer.get(), sapp_frame_duration());
    }
#endif

    simgui_frame_desc_t imguiDesc = {
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    };
    simgui_new_frame(&imguiDesc);

    displayStats();

    if (g_Content) {
        g_Content->handleImgui();
    }
    simgui_render();

    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    g_Content = nullptr;
#ifdef RIVE_RENDERER_SKIA
    g_SkiaSurface = nullptr;
    g_SkiaContext = nullptr;
#endif
    simgui_shutdown();
    sg_shutdown();
}

static void event(const sapp_event* ev) {
    simgui_handle_event(ev);

    switch (ev->type) {
        case SAPP_EVENTTYPE_RESIZED:
            if (g_Content) {
                g_Content->handleResize(ev->framebuffer_width, ev->framebuffer_height);
            }
#ifdef RIVE_RENDERER_TESS
            if (g_TessRenderer) {
                g_TessRenderer->orthographicProjection(
                    0.0f, ev->framebuffer_width, ev->framebuffer_height, 0.0f, 0.0f, 1.0f);
            }
#endif
            break;
        case SAPP_EVENTTYPE_FILES_DROPPED: {
            // Do this to make sure the graphics is bound.
            bindGraphicsContext();

            // get the number of files and their paths like this:
            const int numDroppedFiles = sapp_get_num_dropped_files();
            if (numDroppedFiles != 0) {
                const char* filename = sapp_get_dropped_file_path(numDroppedFiles - 1);
                auto newContent = ViewerContent::findHandler(filename);
                if (newContent) {
                    g_Content = std::move(newContent);
                    g_Content->handleResize(ev->framebuffer_width, ev->framebuffer_height);
                } else {
                    fprintf(stderr, "No handler found for %s\n", filename);
                }
            }
            break;
        }
        case SAPP_EVENTTYPE_MOUSE_DOWN:
        case SAPP_EVENTTYPE_TOUCHES_BEGAN:
            if (g_Content) {
                g_Content->handlePointerDown(ev->mouse_x, ev->mouse_y);
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
        case SAPP_EVENTTYPE_TOUCHES_ENDED:
            if (g_Content) {
                g_Content->handlePointerUp(ev->mouse_x, ev->mouse_y);
                break;
            }
        case SAPP_EVENTTYPE_MOUSE_MOVE:
        case SAPP_EVENTTYPE_TOUCHES_MOVED:
            if (g_Content) {
                g_Content->handlePointerMove(ev->mouse_x, ev->mouse_y);
            }
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            switch (ev->key_code) {
                case SAPP_KEYCODE_ESCAPE:
                    sapp_quit();
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    return (sapp_desc) {
        .init_cb = init, .frame_cb = frame, .cleanup_cb = cleanup, .event_cb = event,
        .enable_dragndrop = true, .high_dpi = true,
        .window_title = "Rive Viewer "
#if defined(SOKOL_GLCORE33)
                        "(OpenGL 3.3)",
#elif defined(SOKOL_GLES2)
                        "(OpenGL ES 2)",
#elif defined(SOKOL_GLES3)
                        "(OpenGL ES 3)",
#elif defined(SOKOL_D3D11)
                        "(D3D11)",
#elif defined(SOKOL_METAL)
                        "(Metal)",
#elif defined(SOKOL_WGPU)
                        "(WebGPU)",
#endif
        .width = 800, .height = 600, .icon.sokol_default = true, .gl_force_gles2 = true,
    };
}
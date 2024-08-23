// Viewer & Rive
#include "viewer/viewer.hpp"
#include "viewer/viewer_content.hpp"
#include "viewer/viewer_host.hpp"
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
#include <chrono>

std::unique_ptr<ViewerHost> g_Host = ViewerHost::Make();
std::unique_ptr<ViewerContent> g_Content = ViewerContent::TrimPath("");
static struct
{
    sg_pass_action pass_action;
} state;

void displayStats();

static const int backgroundColor = rive::colorARGB(255, 22, 22, 22);
static std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;
static const float billion = 1000000000.0;

static void init(void)
{
    sg_desc descriptor = {
        .context = sapp_sgcontext(),
        .buffer_pool_size = 1024,
        .pipeline_pool_size = 1024,
    };
    sg_setup(&descriptor);
    simgui_desc_t imguiDescriptor = {
        .write_alpha_channel = true,
    };
    simgui_setup(&imguiDescriptor);

    // If the host doesn't overwrite this (in init()), Sokol is in full control,
    // so let's clear to our bg color.
    state.pass_action = (sg_pass_action){
        .colors[0] =
            {
                .action = SG_ACTION_CLEAR,
                .value =
                    {
                        rive::colorRed(backgroundColor) / 255.0f,
                        rive::colorGreen(backgroundColor) / 255.0f,
                        rive::colorBlue(backgroundColor) / 255.0f,
                        rive::colorOpacity(backgroundColor),
                    },
            },
        .stencil =
            {
                .action = SG_ACTION_CLEAR,
            },
    };

    lastTime = std::chrono::high_resolution_clock::now();

    if (!g_Host->init(&state.pass_action, sapp_width(), sapp_height()))
    {
        fprintf(stderr, "failed to initialize host\n");
        sapp_quit();
    }
}

static void frame(void)
{

    auto newTime = std::chrono::high_resolution_clock::now();
    auto dur =
        std::chrono::duration_cast<std::chrono::nanoseconds>(newTime - lastTime).count() / billion;
    lastTime = newTime;

    g_Host->beforeDefaultPass(g_Content.get(), dur);

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());

    g_Host->afterDefaultPass(g_Content.get(), dur);

    simgui_frame_desc_t imguiDesc = {
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    };
    simgui_new_frame(&imguiDesc);

    displayStats();

    if (g_Content)
    {
        g_Content->handleImgui();
    }
    simgui_render();

    sg_end_pass();
    sg_commit();
}

static void cleanup(void)
{
    g_Content = nullptr;
    g_Host = nullptr;

    simgui_shutdown();
    sg_shutdown();
}

static void event(const sapp_event* ev)
{
    simgui_handle_event(ev);

    switch (ev->type)
    {
        case SAPP_EVENTTYPE_RESIZED:
            if (g_Content)
            {
                g_Content->handleResize(ev->framebuffer_width, ev->framebuffer_height);
            }
            g_Host->handleResize(ev->framebuffer_width, ev->framebuffer_height);
            break;
        case SAPP_EVENTTYPE_FILES_DROPPED:
        {
            // Do this to make sure the graphics is bound.
            bindGraphicsContext();

            // get the number of files and their paths like this:
            const int numDroppedFiles = sapp_get_num_dropped_files();
            if (numDroppedFiles != 0)
            {
                const char* filename = sapp_get_dropped_file_path(numDroppedFiles - 1);
                auto newContent = ViewerContent::findHandler(filename);
                if (newContent)
                {
                    g_Content = std::move(newContent);
                    g_Content->handleResize(ev->framebuffer_width, ev->framebuffer_height);
                }
                else
                {
                    fprintf(stderr, "No handler found for %s\n", filename);
                }
            }
            break;
        }
        case SAPP_EVENTTYPE_MOUSE_DOWN:
        case SAPP_EVENTTYPE_TOUCHES_BEGAN:
            if (g_Content)
            {
                g_Content->handlePointerDown(ev->mouse_x, ev->mouse_y);
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
        case SAPP_EVENTTYPE_TOUCHES_ENDED:
            if (g_Content)
            {
                g_Content->handlePointerUp(ev->mouse_x, ev->mouse_y);
                break;
            }
        case SAPP_EVENTTYPE_MOUSE_MOVE:
        case SAPP_EVENTTYPE_TOUCHES_MOVED:
            if (g_Content)
            {
                g_Content->handlePointerMove(ev->mouse_x, ev->mouse_y);
            }
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            switch (ev->key_code)
            {
                case SAPP_KEYCODE_ESCAPE:
                    sapp_quit();
                    break;
                case SAPP_KEYCODE_T:
                    g_Content = ViewerContent::Text(".svg");
                    break;
                case SAPP_KEYCODE_P:
                    g_Content = ViewerContent::TextPath("");
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

sapp_desc sokol_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    return (sapp_desc)
    {
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
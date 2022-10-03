#include "sokol_app.h"
#include "imgui.h"

void displayStats()
{
    bool isOpen = true;
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowBorderSize = 0.0f;
    ImGui::Begin("stats",
                 &isOpen,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    if (ImGui::BeginTable("table2", 2))
    {

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("fps");
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", ImGui::GetIO().Framerate);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("ms/frame");
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", 1000.0f / ImGui::GetIO().Framerate);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("window size");
        ImGui::TableNextColumn();
        ImGui::Text("%dx%d (%.1f)", sapp_width(), sapp_height(), sapp_dpi_scale());

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("graphics api");
        ImGui::TableNextColumn();
        ImGui::Text(
#if defined(SOKOL_GLCORE33)
            "OpenGL 3.3"
#elif defined(SOKOL_GLES2)
            "OpenGL ES 2"
#elif defined(SOKOL_GLES3)
            "OpenGL ES 3"
#elif defined(SOKOL_D3D11)
            "D3D11"
#elif defined(SOKOL_METAL)
            "Metal"
#elif defined(SOKOL_WGPU)
            "WebGPU"
#endif
        );

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("renderer");
        ImGui::TableNextColumn();
        ImGui::Text(
#if defined(RIVE_RENDERER_TESS)
            "Rive Tess"
#elif defined(RIVE_RENDERER_SKIA)
            "Rive Skia"
#endif
        );

        ImGui::EndTable();
    }

    ImGui::SetWindowSize(ImVec2(230.0f, 102.0f));
    ImGui::SetWindowPos(ImVec2(sapp_width() / sapp_dpi_scale() - ImGui::GetWindowWidth(),
                               sapp_height() / sapp_dpi_scale() - ImGui::GetWindowHeight()),
                        true);
    ImGui::End();
}
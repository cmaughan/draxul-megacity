#include "megacity_host_panels.h"

#include "ui_city_map_panel.h"

#include <draxul/codeviz_scene_pass.h>
#include <draxul/plugin_imgui_context.h>
#include <imgui.h>

namespace draxul
{

MegacityHostPanelFrame::MegacityHostPanelFrame(
    plugin_support::PluginImGuiContext& imgui,
    const PluginRuntimeViewport& viewport,
    int pixel_w,
    int pixel_h,
    float dt,
    bool show_panels)
{
    if (!imgui.begin_frame(
            viewport.pixel_pos.x, viewport.pixel_pos.y, pixel_w, pixel_h, dt))
        return;

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoBackground;
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(viewport.pixel_pos.x), static_cast<float>(viewport.pixel_pos.y)));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(pixel_w), static_cast<float>(pixel_h)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##dockspace_root", nullptr, flags);
    ImGui::PopStyleVar(3);
    const ImGuiID dock_id = ImGui::GetID("MegaCityDock");
    ImGui::DockSpace(dock_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();

    active_ = true;
    panels_visible_ = show_panels;
}

MegacityHostPanelFrame::~MegacityHostPanelFrame()
{
    finish();
}

bool MegacityHostPanelFrame::active() const
{
    return active_;
}

bool MegacityHostPanelFrame::panels_visible() const
{
    return active_ && panels_visible_;
}

void MegacityHostPanelFrame::render_fixed_panels(
    CodeVizScenePass* scene_pass,
    bool biology_view,
    const std::shared_ptr<const CityGrid>& grid,
    bool grid_build_in_progress)
{
    if (!panels_visible())
        return;
    if (scene_pass)
        scene_pass->render_gbuffer_debug_ui();
    if (!biology_view)
        render_city_map_panel(grid, grid_build_in_progress);
}

void MegacityHostPanelFrame::finish()
{
    if (!active_ || finished_)
        return;
    ImGui::Render();
    finished_ = true;
}

} // namespace draxul

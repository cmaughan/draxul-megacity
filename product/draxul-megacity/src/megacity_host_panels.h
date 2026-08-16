#pragma once

#include <draxul/events.h>
#include <draxul/plugin_runtime.h>
#include <memory>

namespace draxul
{

namespace plugin_support
{
class PluginImGuiContext;
}

class CodeVizScenePass;
struct CityGrid;

// Input routing goes through the shared plugin_support::ImGuiInputBridge; this
// file owns only the frame/dockspace setup and the fixed debug/map panels.

// Owns ImGui frame/dockspace setup and the module's fixed debug/map panels.
// The host retains only application-specific control result handling.
class MegacityHostPanelFrame
{
public:
    MegacityHostPanelFrame(
        plugin_support::PluginImGuiContext& imgui,
        const PluginRuntimeViewport& viewport,
        int pixel_w,
        int pixel_h,
        float dt,
        bool show_panels);
    ~MegacityHostPanelFrame();

    bool active() const;
    bool panels_visible() const;
    void render_fixed_panels(
        CodeVizScenePass* scene_pass,
        bool biology_view,
        const std::shared_ptr<const CityGrid>& grid,
        bool grid_build_in_progress);
    void finish();

private:
    bool active_ = false;
    bool panels_visible_ = false;
    bool finished_ = false;
};

} // namespace draxul

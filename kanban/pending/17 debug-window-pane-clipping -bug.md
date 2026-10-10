# Keep MegaCity's G-buffer debug window inside its pane

**Summary:** Draw MegaCity's floating G-buffer Debug window within MegaCity's own pane instead of over a neighbouring pane.

**Priority:** P2 — diagnostic overlay covers another pane's content in split layouts.
**Source:** Windows card-85 texture-ownership gate, 2026-10-08 (root `kanban/done/85 plugin-texture-release-ownership -bug.md`).

**Evidence:** With SatView in the left half and MegaCity (`show_ui = true`) in the right half of one tab, the "GBuffer Debug" ImGui window renders at the window's left edge, across the SatView pane. The same placement is visible in the earlier 2026-10-08 gate capture `textures-rendered.bmp`, so it predates the descriptor-lifetime fixes. MegaCity's scene and its other UI render in the correct pane.

- [x] Investigate whether the window position/clip comes from MegaCity's debug-window placement or from the shared plugin ImGui display origin/clip rectangle (`plugins/support/imgui`); check SatView's HDR Buffers window in the mirrored layout.
- [x] Fix the owning layer so plugin ImGui windows are positioned and clipped within their pane on Vulkan and Metal.
- [ ] Verify with SatView/MegaCity split both ways, a split resize, and a native window resize; run the MegaCity aggregate and same-cache smoke.

## Investigation (2026-10-10)

The cause is the shared plugin layer, not MegaCity's window code. `PluginImGuiContext::begin_frame` sets `io.DisplaySize` to the pane's bottom-right corner in window-global pixels, and without `ImGuiConfigFlags_ViewportsEnable` Dear ImGui 1.90.8 pins the main viewport at (0,0) (`UpdateViewportsNewFrame`). Every product context therefore spans from the window's top-left corner: new windows appear at ImGui's default (60,60) in window coordinates, which lies in the left/top neighbour when the product pane is on the right or bottom, and windows can be dragged over other panes. Nothing clips product draw lists to their pane. SatView and ScoreView share the same layer, so their windows behave the same in mirrored layouts.

Reproduced with scripted render-test input (root `kanban/pending/121 pointer-capture-and-scripted-render-input -bug.md`): GBuffer Debug and City Map default to window position (60,60), and drags past the pane edge previously left ImGui without a release (fixed by pointer capture in that card).

Fix direction for the next slice: run product ImGui in pane-local coordinates — `io.DisplaySize` = pane size, pointer events offset by the pane origin, and draw data shifted back by setting `DisplayPos = -pane_origin` (with the full framebuffer extent) in the shared Metal/Vulkan GPU host — then migrate products that place windows with `viewport.pixel_pos` (MegaCity dockspace root and Codebase Analysis, SatView dockspace, ScoreView inspector).

## Fix (2026-10-10)

`PluginImGuiContext::begin_frame` now makes the pane the ImGui display (`DisplaySize` = pane size) and records the pane origin; `ImGuiInputBridge::route_mouse_move`/`route_mouse_button` take the `PluginImGuiContext` and convert window-pixel events to pane-local positions (a button also queues its position); the shared Metal and Vulkan GPU hosts draw with `DisplayPos = -pane origin` over the full framebuffer, which also offsets every clip rectangle. MegaCity's dockspace root and Codebase Analysis first-use position, SatView's dockspace root, scene rectangle and test hook, and ScoreView's inspector position moved to pane-local coordinates. Saved `*_imgui.ini` positions are reinterpreted as pane-local and ImGui clamps them into the pane.

Evidence (macOS/Metal, Release): with the pane at window (10,51) MegaCity's GBuffer Debug opens at pane (60,60), a scripted drag docks it into the pane dockspace, and a drag past the window's top-left leaves it clipped at the pane edge instead of over the tab bar. SatView's docked control/Scene layout fills its pane with the scene inside the Scene window. The SatView pause-button click test now runs with the pane at (320,48). The Vulkan path shares the same placement helper but was not compiled on this host. Split layouts both ways and resizes remain for interactive confirmation below.

# Keep MegaCity's G-buffer debug window inside its pane

**Summary:** Draw MegaCity's floating G-buffer Debug window within MegaCity's own pane instead of over a neighbouring pane.

**Priority:** P2 — diagnostic overlay covers another pane's content in split layouts.
**Source:** Windows card-85 texture-ownership gate, 2026-10-08 (root `kanban/done/85 plugin-texture-release-ownership -bug.md`).

**Evidence:** With SatView in the left half and MegaCity (`show_ui = true`) in the right half of one tab, the "GBuffer Debug" ImGui window renders at the window's left edge, across the SatView pane. The same placement is visible in the earlier 2026-10-08 gate capture `textures-rendered.bmp`, so it predates the descriptor-lifetime fixes. MegaCity's scene and its other UI render in the correct pane.

- [x] Investigate whether the window position/clip comes from MegaCity's debug-window placement or from the shared plugin ImGui display origin/clip rectangle (`plugins/support/imgui`); check SatView's HDR Buffers window in the mirrored layout.
- [ ] Fix the owning layer so plugin ImGui windows are positioned and clipped within their pane on Vulkan and Metal.
- [ ] Verify with SatView/MegaCity split both ways, a split resize, and a native window resize; run the MegaCity aggregate and same-cache smoke.

## Investigation (2026-10-10)

The cause is the shared plugin layer, not MegaCity's window code. `PluginImGuiContext::begin_frame` sets `io.DisplaySize` to the pane's bottom-right corner in window-global pixels, and without `ImGuiConfigFlags_ViewportsEnable` Dear ImGui 1.90.8 pins the main viewport at (0,0) (`UpdateViewportsNewFrame`). Every product context therefore spans from the window's top-left corner: new windows appear at ImGui's default (60,60) in window coordinates, which lies in the left/top neighbour when the product pane is on the right or bottom, and windows can be dragged over other panes. Nothing clips product draw lists to their pane. SatView and ScoreView share the same layer, so their windows behave the same in mirrored layouts.

Reproduced with scripted render-test input (root `kanban/pending/121 pointer-capture-and-scripted-render-input -bug.md`): GBuffer Debug and City Map default to window position (60,60), and drags past the pane edge previously left ImGui without a release (fixed by pointer capture in that card).

Fix direction for the next slice: run product ImGui in pane-local coordinates — `io.DisplaySize` = pane size, pointer events offset by the pane origin, and draw data shifted back by setting `DisplayPos = -pane_origin` (with the full framebuffer extent) in the shared Metal/Vulkan GPU host — then migrate products that place windows with `viewport.pixel_pos` (MegaCity dockspace root and Codebase Analysis, SatView dockspace, ScoreView inspector).

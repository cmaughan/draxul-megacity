# Keep MegaCity's G-buffer debug window inside its pane

**Summary:** Draw MegaCity's floating G-buffer Debug window within MegaCity's own pane instead of over a neighbouring pane.

**Priority:** P2 — diagnostic overlay covers another pane's content in split layouts.
**Source:** Windows card-85 texture-ownership gate, 2026-10-08 (root `kanban/done/85 plugin-texture-release-ownership -bug.md`).

**Evidence:** With SatView in the left half and MegaCity (`show_ui = true`) in the right half of one tab, the "GBuffer Debug" ImGui window renders at the window's left edge, across the SatView pane. The same placement is visible in the earlier 2026-10-08 gate capture `textures-rendered.bmp`, so it predates the descriptor-lifetime fixes. MegaCity's scene and its other UI render in the correct pane.

- [ ] Investigate whether the window position/clip comes from MegaCity's debug-window placement or from the shared plugin ImGui display origin/clip rectangle (`plugins/support/imgui`); check SatView's HDR Buffers window in the mirrored layout.
- [ ] Fix the owning layer so plugin ImGui windows are positioned and clipped within their pane on Vulkan and Metal.
- [ ] Verify with SatView/MegaCity split both ways, a split resize, and a native window resize; run the MegaCity aggregate and same-cache smoke.

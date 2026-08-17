#pragma once

// MegaCity's binding of the SHARED camera input layer
// (Draxul::PluginSupport::CameraInput). The key latch table, the Ctrl+R guard,
// the drag inertia and the click/double-click detection all live in
// draxul/camera_input.h now; what stays here is MegaCity's camera policy: which
// axes the key groups drive and how a screen-space drag becomes camera motion.
//
// WHY THESE BINDINGS: MegaCity's isometric city view can pan, so the arrows and
// WASD drive the pan axes, Q/E orbit and T/G are a separate pitch axis.
// SatView's globe cannot pan, so it folds the same groups into its orbit axes.
// Neither is "the" intended mapping — they are different cameras, and the shared
// table exposes the axes separately so each product picks.

#include <draxul/camera_input.h>
#include <draxul/events.h>
#include <glm/vec2.hpp>
#include <optional>

namespace draxul
{

class IsometricCamera;

struct CameraMovement
{
    glm::vec2 pan_input{ 0.0f };
    float orbit = 0.0f;
    float zoom = 0.0f;
    float pitch = 0.0f;
};

// Arrows/WASD pan, Q/E orbit, T/G pitch, R/F zoom with Ctrl+R reserved for the
// host. The Ctrl+R guard is new here: it came from SatView's copy of the table
// and is the canonical behaviour now.
inline constexpr camera_input::OrbitKeyBindings kMegacityCameraBindings{
    .horizontal_arrows_orbit = false,
    .vertical_arrows_orbit = false,
    .pitch_folds_into_orbit = false,
    .zoom_in_guard_modifiers = kModCtrl,
};

// Self-contained input state machine for code-visualization views.
// Translates SDL key/mouse events into camera movement intent.
class CodeVizInputState
{
public:
    // Clear all pressed-key state (called when the host loses focus).
    void reset_keys();

    // Returns true if the key was consumed and state changed.
    bool on_key(const KeyEvent& event);
    void on_mouse_button(const MouseButtonEvent& event);

    // Accumulates drag pan/orbit deltas. Returns true if drag state changed.
    bool on_mouse_move(const MouseMoveEvent& event, IsometricCamera& camera);

    bool movement_active() const;
    bool drag_smoothing_active() const;
    CameraMovement movement() const;

    // Returns a click position if the last mouse-up was a click (not a drag).
    // Consumes the click — subsequent calls return nullopt until the next click.
    std::optional<glm::ivec2> consume_click();

    // Returns a double-click position if two clicks occurred within ~400ms and ~4px.
    // Consumes the double-click — subsequent calls return nullopt until the next one.
    std::optional<glm::ivec2> consume_double_click();

    // Apply pending drag smoothing for this frame tick.
    // Returns true if the camera was modified.
    bool apply_drag_smoothing(float dt, IsometricCamera& camera);

private:
    camera_input::OrbitKeyState keys_{ kMegacityCameraBindings };
    camera_input::DragSmoother drag_;
};

} // namespace draxul

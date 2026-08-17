#include <draxul/codeviz_input_state.h>
#include <draxul/isometric_camera.h>
#include <SDL3/SDL.h>
#include <draxul/perf_timing.h>
#include <glm/geometric.hpp>

namespace draxul
{

namespace
{

constexpr float kOrbitSpeedRadiansPerSecond = 1.8f;
constexpr float kOrbitDragReferencePixelsPerSecond = 240.0f;
constexpr float kOrbitDragRadiansPerPixel
    = kOrbitSpeedRadiansPerSecond / kOrbitDragReferencePixelsPerSecond;

} // namespace

void CodeVizInputState::reset_keys()
{
    keys_.reset();
}

bool CodeVizInputState::on_key(const KeyEvent& event)
{
    PERF_MEASURE();
    return keys_.on_key(event);
}

void CodeVizInputState::on_mouse_button(const MouseButtonEvent& event)
{
    PERF_MEASURE();
    drag_.on_mouse_button(event, SDL_BUTTON_LEFT);
}

bool CodeVizInputState::on_mouse_move(const MouseMoveEvent& event, IsometricCamera& camera)
{
    PERF_MEASURE();
    if (!drag_.dragging())
        return false;

    // The platform can drop a button release (e.g. it happened outside the
    // window); trust the live button state over the last event we saw.
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0
        && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) == 0)
    {
        drag_.cancel_drag();
        return false;
    }

    const auto pixel_delta = drag_.on_mouse_move(event);
    if (!pixel_delta)
        return false;

    // Camera policy, not input plumbing: Alt turns a horizontal drag into an
    // orbit; otherwise the drag pans in the camera's ground plane.
    if ((event.mod & kModAlt) != 0)
    {
        if (pixel_delta->x != 0.0f)
        {
            drag_.add_orbit(-pixel_delta->x * kOrbitDragRadiansPerPixel);
            return true;
        }
        return false;
    }

    const glm::vec2 pan = camera.pan_delta_for_screen_drag(*pixel_delta);
    if (glm::dot(pan, pan) <= 0.0f)
        return false;

    drag_.add_pan(pan);
    return true;
}

bool CodeVizInputState::movement_active() const
{
    return keys_.movement_active();
}

bool CodeVizInputState::drag_smoothing_active() const
{
    return drag_.smoothing_active();
}

CameraMovement CodeVizInputState::movement() const
{
    PERF_MEASURE();
    const camera_input::OrbitMovement shared = keys_.movement();
    CameraMovement movement;
    movement.pan_input = shared.pan;
    movement.orbit = shared.orbit.x;
    movement.zoom = shared.zoom;
    movement.pitch = shared.pitch;
    return movement;
}

std::optional<glm::ivec2> CodeVizInputState::consume_click()
{
    return drag_.consume_click();
}

std::optional<glm::ivec2> CodeVizInputState::consume_double_click()
{
    return drag_.consume_double_click();
}

bool CodeVizInputState::apply_drag_smoothing(float dt, IsometricCamera& camera)
{
    PERF_MEASURE();
    const camera_input::DragStep step = drag_.consume_step(dt);
    if (step.empty())
        return false;

    bool changed = false;
    if (step.pan.x != 0.0f || step.pan.y != 0.0f)
    {
        camera.translate_target(step.pan.x, step.pan.y);
        changed = true;
    }
    if (step.orbit != 0.0f)
    {
        camera.orbit_target(step.orbit);
        changed = true;
    }
    return changed;
}

} // namespace draxul

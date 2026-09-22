#pragma once

#include <algorithm>
#include <draxul/semantic_city_layout.h>

#include <utility>

namespace draxul::megacity_model_detail
{

inline constexpr float kRoadSurfaceTextureLift = 0.002f;
inline constexpr float kDependencyRouteLift = 0.01f;

inline float building_base_elevation(const SemanticCityLayoutOptions& config)
{
    return config.sidewalk_surface_lift + config.sidewalk_surface_height;
}

inline float route_base_elevation(const SemanticCityLayoutOptions& config)
{
    return std::max(
               kRoadSurfaceTextureLift + config.road_surface_height,
               config.sidewalk_surface_lift + config.sidewalk_surface_height)
        + kDependencyRouteLift;
}

// Hollow-ring brick layout: grids larger than 2x2 reserve their center.
inline int brick_slots_per_floor(int grid_size)
{
    if (grid_size <= 2)
        return grid_size * grid_size;
    return 4 * (grid_size - 1);
}

inline std::pair<int, int> brick_slot_position(int local_index, int grid_size)
{
    if (grid_size <= 2)
        return { local_index % grid_size, local_index / grid_size };

    const int perimeter = 4 * (grid_size - 1);
    const int index = local_index % perimeter;
    const int edge = grid_size - 1;
    if (index < grid_size)
        return { index, 0 };
    if (index < grid_size + edge - 1)
        return { edge, index - edge };
    if (index < 2 * grid_size + edge - 2)
        return { edge - (index - (grid_size + edge - 1)), edge };
    return { 0, edge - (index - (2 * edge + grid_size - 1)) };
}

} // namespace draxul::megacity_model_detail

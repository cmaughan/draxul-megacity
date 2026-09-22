#include <draxul/semantic_city_layout.h>

#include <algorithm>
#include <cmath>
#include <draxul/perf_timing.h>

namespace draxul
{
CityGrid build_city_grid(const SemanticMegacityLayout& layout, const SemanticCityLayoutOptions& config)
{
    SemanticMegacityModel empty_model;
    return build_city_grid(layout, empty_model, config);
}

CityGrid build_city_grid(
    const SemanticMegacityLayout& layout, const SemanticMegacityModel& model, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    (void)model;
    CityGrid grid;
    if (layout.empty())
        return grid;

    const float step = std::max(config.placement_step, 0.01f);
    grid.cell_size = step;

    // Pad bounds by one cell so buildings at the edge are fully inside.
    grid.origin_x = layout.min_x - step;
    grid.origin_z = layout.min_z - step;
    const float extent_x = (layout.max_x + step) - grid.origin_x;
    const float extent_z = (layout.max_z + step) - grid.origin_z;
    grid.cols = static_cast<int>(std::ceil(extent_x / step));
    grid.rows = static_cast<int>(std::ceil(extent_z / step));

    if (grid.cols <= 0 || grid.rows <= 0)
        return grid;

    grid.cells.assign(static_cast<size_t>(grid.cols) * grid.rows, kCityGridEmpty);

    // Helper: rasterize an axis-aligned rect into the grid with a given cell value.
    // All geometry is snapped to `step`, so edges land exactly on cell boundaries.
    // Use a small inset on max edges to avoid filling cells the geometry only touches
    // at the boundary (off-by-one), and a small inset on min edges for symmetry.
    const float eps = step * 0.01f;
    auto fill_rect = [&](float world_min_x, float world_max_x, float world_min_z, float world_max_z, uint8_t value) {
        const int c0 = std::max(0, static_cast<int>(std::floor((world_min_x - grid.origin_x + eps) / step)));
        const int c1 = std::min(grid.cols - 1, static_cast<int>(std::floor((world_max_x - grid.origin_x - eps) / step)));
        const int r0 = std::max(0, static_cast<int>(std::floor((world_min_z - grid.origin_z + eps) / step)));
        const int r1 = std::min(grid.rows - 1, static_cast<int>(std::floor((world_max_z - grid.origin_z - eps) / step)));
        for (int r = r0; r <= r1; ++r)
            for (int c = c0; c <= c1; ++c)
                grid.cells[static_cast<size_t>(r) * grid.cols + c] = value;
    };

    // Three separate passes so higher-priority layers always overwrite lower ones:
    // 1) roads, 2) sidewalks, 3) buildings + parks.
    auto for_each_building = [&](auto&& fn) {
        for (const auto& module_layout : layout.modules)
            for (const auto& building : module_layout.buildings)
                fn(building);
    };

    // Pass 1: shared road surface (outermost layer)
    const CitySurfaceBounds road_surface_bounds = compute_city_road_surface_bounds(layout);
    if (road_surface_bounds.valid())
    {
        fill_rect(
            road_surface_bounds.min_x,
            road_surface_bounds.max_x,
            road_surface_bounds.min_z,
            road_surface_bounds.max_z,
            kCityGridRoad);
    }

    // Pass 2: sidewalks (overwrites roads within sidewalk areas)
    for_each_building([&fill_rect](const SemanticCityBuilding& building) {
        const auto sidewalks = build_sidewalk_segments(building);
        for (const auto& sw : sidewalks)
            fill_rect(
                sw.center.x - sw.extent.x * 0.5f,
                sw.center.x + sw.extent.x * 0.5f,
                sw.center.y - sw.extent.y * 0.5f,
                sw.center.y + sw.extent.y * 0.5f,
                kCityGridSidewalk);
    });
    for (const auto& module_layout : layout.modules)
    {
        if (module_layout.park_footprint > 0.0f)
        {
            SemanticCityBuilding park_building;
            park_building.center = module_layout.park_center;
            park_building.metrics.footprint = module_layout.park_footprint;
            park_building.metrics.sidewalk_width = module_layout.park_sidewalk_width;
            park_building.metrics.road_width = module_layout.park_road_width;
            const auto sidewalks = build_sidewalk_segments(park_building);
            for (const auto& sw : sidewalks)
                fill_rect(
                    sw.center.x - sw.extent.x * 0.5f,
                    sw.center.x + sw.extent.x * 0.5f,
                    sw.center.y - sw.extent.y * 0.5f,
                    sw.center.y + sw.extent.y * 0.5f,
                    kCityGridSidewalk);
        }
    }

    // Pass 3: building footprints + parks (parks never overlap buildings)
    for_each_building([&fill_rect](const SemanticCityBuilding& building) {
        const float half_fp = building.metrics.footprint * 0.5f;
        const float cx = building.center.x;
        const float cz = building.center.y; // center.y is world Z
        fill_rect(cx - half_fp, cx + half_fp, cz - half_fp, cz + half_fp, kCityGridBuilding);
    });
    for (const auto& module_layout : layout.modules)
    {
        if (module_layout.park_footprint > 0.0f)
        {
            const float half = module_layout.park_footprint * 0.5f;
            fill_rect(
                module_layout.park_center.x - half,
                module_layout.park_center.x + half,
                module_layout.park_center.y - half,
                module_layout.park_center.y + half,
                kCityGridPark);
        }
    }

    return grid;
}

} // namespace draxul

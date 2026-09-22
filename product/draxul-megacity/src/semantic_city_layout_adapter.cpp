#include "semantic_city_layout.h"

namespace draxul
{

SemanticCityLayoutOptions semantic_city_layout_options_from_config(const MegaCityCodeConfig& config)
{
    return {
        config.clamp_semantic_metrics,
        config.hide_test_entities,
        config.hide_struct_entities,
        config.enable_struct_stacking,
        config.struct_stack_max,
        config.struct_stack_gap,
        config.struct_brick_grid_size,
        config.struct_brick_gap,
        config.hide_function_entities,
        config.functions_per_building_max,
        config.height_multiplier,
        config.placement_step,
        config.max_spiral_rings,
        config.footprint_base,
        config.footprint_range,
        config.footprint_unclamped_scale,
        config.height_base,
        config.height_mass_weight,
        config.height_count_weight,
        config.height_range,
        config.height_unclamped_count_weight,
        config.road_width_base,
        config.road_width_scale,
        config.road_width_range,
        config.sidewalk_width,
        config.dependency_route_layer_step,
        config.park_footprint,
        config.park_sidewalk_width,
        config.park_road_width,
        config.central_park_scale,
        config.roof_sign_thickness,
        config.sidewalk_surface_height,
        config.sidewalk_surface_lift,
        config.road_surface_height,
    };
}

BuildingMetrics derive_building_metrics(const CityClassRecord& row, const MegaCityCodeConfig& config)
{
    return derive_building_metrics(row, semantic_city_layout_options_from_config(config));
}

SemanticCityModuleModel build_semantic_city_model(
    std::string_view module_path, const std::vector<CityClassRecord>& rows, const MegaCityCodeConfig& config)
{
    return build_semantic_city_model(module_path, rows, semantic_city_layout_options_from_config(config));
}

SemanticMegacityModel build_semantic_megacity_model(
    const std::vector<SemanticCityModuleInput>& modules, const MegaCityCodeConfig& config)
{
    return build_semantic_megacity_model(modules, semantic_city_layout_options_from_config(config));
}

float compute_module_border_width(const SemanticCityModuleLayout& module_layout, const MegaCityCodeConfig& config)
{
    return compute_module_border_width(module_layout, semantic_city_layout_options_from_config(config));
}

std::array<ModuleBoundarySignPlacement, 2> build_module_boundary_sign_placements(
    const SemanticCityModuleLayout& module_layout, const MegaCityCodeConfig& config)
{
    return build_module_boundary_sign_placements(module_layout, semantic_city_layout_options_from_config(config));
}

SemanticCityLayout build_semantic_city_layout(
    const SemanticCityModuleModel& module_model, const MegaCityCodeConfig& config)
{
    return build_semantic_city_layout(module_model, semantic_city_layout_options_from_config(config));
}

SemanticCityLayout build_semantic_city_layout(
    const std::vector<CityClassRecord>& rows, const MegaCityCodeConfig& config)
{
    return build_semantic_city_layout(rows, semantic_city_layout_options_from_config(config));
}

SemanticMegacityLayout build_semantic_megacity_layout(
    const SemanticMegacityModel& model, const MegaCityCodeConfig& config)
{
    return build_semantic_megacity_layout(model, semantic_city_layout_options_from_config(config));
}

SemanticMegacityLayout build_semantic_megacity_layout(
    const std::vector<SemanticCityModuleInput>& modules, const MegaCityCodeConfig& config)
{
    return build_semantic_megacity_layout(modules, semantic_city_layout_options_from_config(config));
}

CityGrid build_city_grid(const SemanticMegacityLayout& layout, const MegaCityCodeConfig& config)
{
    return build_city_grid(layout, semantic_city_layout_options_from_config(config));
}

CityGrid build_city_grid(
    const SemanticMegacityLayout& layout, const SemanticMegacityModel& model, const MegaCityCodeConfig& config)
{
    return build_city_grid(layout, model, semantic_city_layout_options_from_config(config));
}

std::vector<CityGrid::RoutePolyline> build_city_routes(
    const SemanticMegacityLayout& layout, const SemanticMegacityModel& model, const MegaCityCodeConfig& config)
{
    return build_city_routes(layout, model, semantic_city_layout_options_from_config(config));
}

std::vector<CityGrid::RoutePolyline> build_city_routes_for_selection(
    const SemanticMegacityLayout& layout, const SemanticMegacityModel& model, const CityGrid& grid,
    const MegaCityCodeConfig& config,
    std::string_view focus_source_file_path,
    std::string_view focus_module_path,
    std::string_view focus_qualified_name,
    std::string_view focus_function_name)
{
    return build_city_routes_for_selection(
        layout,
        model,
        grid,
        semantic_city_layout_options_from_config(config),
        focus_source_file_path,
        focus_module_path,
        focus_qualified_name,
        focus_function_name);
}

} // namespace draxul

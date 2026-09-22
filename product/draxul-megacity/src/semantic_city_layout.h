#pragma once

#include <draxul/semantic_city_layout.h>
#include <draxul/megacity_code_config.h>

namespace draxul
{

[[nodiscard]] SemanticCityLayoutOptions semantic_city_layout_options_from_config(
    const MegaCityCodeConfig& config);

[[nodiscard]] BuildingMetrics derive_building_metrics(
    const CityClassRecord& row, const MegaCityCodeConfig& config);
[[nodiscard]] SemanticCityModuleModel build_semantic_city_model(
    std::string_view module_path, const std::vector<CityClassRecord>& rows, const MegaCityCodeConfig& config);
[[nodiscard]] SemanticMegacityModel build_semantic_megacity_model(
    const std::vector<SemanticCityModuleInput>& modules, const MegaCityCodeConfig& config);
[[nodiscard]] float compute_module_border_width(
    const SemanticCityModuleLayout& module_layout, const MegaCityCodeConfig& config);
[[nodiscard]] std::array<ModuleBoundarySignPlacement, 2> build_module_boundary_sign_placements(
    const SemanticCityModuleLayout& module_layout, const MegaCityCodeConfig& config);
[[nodiscard]] SemanticCityLayout build_semantic_city_layout(
    const SemanticCityModuleModel& module_model, const MegaCityCodeConfig& config);
[[nodiscard]] SemanticCityLayout build_semantic_city_layout(
    const std::vector<CityClassRecord>& rows, const MegaCityCodeConfig& config);
[[nodiscard]] SemanticMegacityLayout build_semantic_megacity_layout(
    const SemanticMegacityModel& model, const MegaCityCodeConfig& config);
[[nodiscard]] SemanticMegacityLayout build_semantic_megacity_layout(
    const std::vector<SemanticCityModuleInput>& modules, const MegaCityCodeConfig& config);
[[nodiscard]] CityGrid build_city_grid(
    const SemanticMegacityLayout& layout, const MegaCityCodeConfig& config);
[[nodiscard]] CityGrid build_city_grid(
    const SemanticMegacityLayout& layout, const SemanticMegacityModel& model, const MegaCityCodeConfig& config);
[[nodiscard]] std::vector<CityGrid::RoutePolyline> build_city_routes(
    const SemanticMegacityLayout& layout, const SemanticMegacityModel& model, const MegaCityCodeConfig& config);
[[nodiscard]] std::vector<CityGrid::RoutePolyline> build_city_routes_for_selection(
    const SemanticMegacityLayout& layout, const SemanticMegacityModel& model, const CityGrid& grid,
    const MegaCityCodeConfig& config,
    std::string_view focus_source_file_path,
    std::string_view focus_module_path,
    std::string_view focus_qualified_name,
    std::string_view focus_function_name = {});

} // namespace draxul

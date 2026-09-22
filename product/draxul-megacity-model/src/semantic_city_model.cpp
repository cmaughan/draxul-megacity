#include <draxul/semantic_city_layout.h>

#include "semantic_city_model_internal.h"

#include <algorithm>
#include <cmath>
#include <draxul/perf_timing.h>
#include <limits>
#include <numbers>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace draxul
{
namespace
{

using megacity_model_detail::brick_slots_per_floor;

float snap_to_grid(float value, float grid_step)
{
    return std::round(value / grid_step) * grid_step;
}

bool path_has_prefix(std::string_view path, std::string_view prefix)
{
    return path.rfind(prefix, 0) == 0;
}

int function_mass(const CityClassRecord& row)
{
    int mass = 0;
    for (const int size : row.function_sizes)
        mass += size;
    return mass;
}

std::vector<SemanticBuildingLayer> build_function_layers(const CityClassRecord& row, const BuildingMetrics& metrics)
{
    PERF_MEASURE();
    struct FunctionEntry
    {
        std::string name;
        int size = 0;
    };
    std::vector<FunctionEntry> entries;
    entries.reserve(row.function_sizes.size());
    for (size_t i = 0; i < row.function_sizes.size(); ++i)
    {
        if (row.function_sizes[i] > 0)
        {
            std::string name = (i < row.function_names.size()) ? row.function_names[i] : std::string();
            entries.push_back({ std::move(name), row.function_sizes[i] });
        }
    }

    if (entries.empty())
        return { { std::string(), std::string(), 0, metrics.height } };

    int total_size = 0;
    for (const auto& entry : entries)
        total_size += entry.size;
    if (total_size <= 0)
        return { { std::string(), std::string(), 0, metrics.height } };

    std::vector<SemanticBuildingLayer> layers;
    layers.reserve(entries.size());

    float remaining_height = metrics.height;
    for (size_t index = 0; index < entries.size(); ++index)
    {
        const int function_size = entries[index].size;
        float layer_height = metrics.height;
        if (index + 1 < entries.size())
        {
            layer_height = metrics.height * static_cast<float>(function_size) / static_cast<float>(total_size);
            layer_height = std::min(layer_height, remaining_height);
        }
        else
        {
            layer_height = remaining_height;
        }

        layers.push_back({ entries[index].name, std::string(), function_size, std::max(layer_height, 0.0f) });
        remaining_height = std::max(0.0f, remaining_height - layer_height);
    }

    if (!layers.empty())
    {
        float total_height = 0.0f;
        for (const SemanticBuildingLayer& layer : layers)
            total_height += layer.height;
        const float error = metrics.height - total_height;
        layers.back().height += error;
    }

    return layers;
}

} // namespace

BuildingMetrics derive_building_metrics(const CityClassRecord& row, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    const float base = static_cast<float>(std::max(row.base_size, 0));
    const float mass = static_cast<float>(std::max(function_mass(row), 0));
    const float funcs = static_cast<float>(std::max(row.building_functions, 0));
    const float road = static_cast<float>(std::max(row.road_size, 0));

    const bool clamp_metrics = config.clamp_semantic_metrics;
    const float step = std::max(config.placement_step, 0.01f);
    const float raw_footprint = clamp_metrics
        ? std::clamp(config.footprint_base + std::sqrt(base), config.footprint_range.x, config.footprint_range.y)
        : config.footprint_base + base * config.footprint_unclamped_scale;
    const float footprint = std::max(step, snap_to_grid(raw_footprint, step));
    const float height = clamp_metrics
        ? std::clamp(
              config.height_base + config.height_mass_weight * std::log1p(mass)
                  + config.height_count_weight * std::sqrt(funcs),
              config.height_range.x, config.height_range.y)
        : config.height_base + config.height_multiplier * std::log1p(mass)
            + config.height_multiplier * config.height_unclamped_count_weight * std::log1p(funcs);
    const float raw_road_width = clamp_metrics
        ? std::clamp(config.road_width_base + config.road_width_scale * std::log1p(road), config.road_width_range.x, config.road_width_range.y)
        : config.road_width_base + config.road_width_scale * std::log1p(road);
    const float road_width = std::max(step, snap_to_grid(raw_road_width, step));
    const float sidewalk_width = std::max(step, snap_to_grid(config.sidewalk_width, step));
    return { footprint, height, sidewalk_width, road_width };
}

bool is_test_semantic_source(std::string_view source_file_path)
{
    return path_has_prefix(source_file_path, "tests/");
}

SemanticCityModuleModel build_semantic_city_model(
    std::string_view module_path, const std::vector<CityClassRecord>& rows, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    SemanticCityModuleModel module_model;
    module_model.module_path = std::string(module_path);
    module_model.buildings.reserve(rows.size());

    std::vector<const CityClassRecord*> free_functions;
    for (const auto& row : rows)
    {
        if ((row.entity_kind != "building" && row.entity_kind != "block" && row.entity_kind != "tree") || row.is_abstract)
            continue;
        if (config.hide_test_entities && is_test_semantic_source(row.source_file_path))
            continue;
        if (config.hide_struct_entities && row.is_struct)
            continue;
        if (row.entity_kind == "tree")
        {
            if (!config.hide_function_entities)
                free_functions.push_back(&row);
            continue;
        }

        const BuildingMetrics metrics = derive_building_metrics(row, config);
        module_model.connectivity += std::max(row.road_size, 0);
        module_model.buildings.push_back({
            row.module_path.empty() ? std::string(module_path) : row.module_path,
            row.name,
            row.qualified_name,
            row.source_file_path,
            row.is_struct,
            false,
            false,
            std::max(row.base_size, 0),
            std::max(row.building_functions, 0),
            function_mass(row),
            std::max(row.road_size, 0),
            metrics,
            { 0.0f, 0.0f },
            build_function_layers(row, metrics),
        });
    }

    // Bundle free functions into grouped buildings (triangular, up to N per bundle).
    if (!free_functions.empty())
    {
        const int max_per_bundle = std::max(config.functions_per_building_max, 1);
        const int bundle_count = (static_cast<int>(free_functions.size()) + max_per_bundle - 1) / max_per_bundle;
        for (int bi = 0; bi < bundle_count; ++bi)
        {
            const size_t start = static_cast<size_t>(bi) * max_per_bundle;
            const size_t end = std::min(start + static_cast<size_t>(max_per_bundle), free_functions.size());

            CityClassRecord bundle_row;
            bundle_row.module_path = std::string(module_path);
            bundle_row.name = bundle_count > 1
                ? "Functions " + std::to_string(bi + 1)
                : "Functions";
            bundle_row.qualified_name = bundle_row.name;
            bundle_row.entity_kind = "tree";
            bundle_row.base_size = 0;
            bundle_row.building_functions = static_cast<int>(end - start);
            bundle_row.road_size = 0;

            struct BundledFunction
            {
                std::string name;
                std::string source_file_path;
                int size = 0;
            };
            std::vector<BundledFunction> bundled;
            bundled.reserve(end - start);
            for (size_t j = start; j < end; ++j)
            {
                const auto& fn = *free_functions[j];
                const int sz = (!fn.function_sizes.empty() && fn.function_sizes[0] > 0) ? fn.function_sizes[0] : 0;
                bundle_row.function_sizes.push_back(sz);
                bundle_row.function_names.push_back(fn.name);
                if (sz > 0)
                    bundled.push_back({ fn.name, fn.source_file_path, sz });
            }

            const BuildingMetrics metrics = derive_building_metrics(bundle_row, config);

            // Build layers with per-function source file paths.
            std::vector<SemanticBuildingLayer> layers;
            if (bundled.empty())
            {
                layers.push_back({ std::string(), std::string(), 0, metrics.height });
            }
            else
            {
                int total_size = 0;
                for (const auto& bf : bundled)
                    total_size += bf.size;
                layers.reserve(bundled.size());
                float remaining_height = metrics.height;
                for (size_t fi = 0; fi < bundled.size(); ++fi)
                {
                    float layer_height = (fi + 1 < bundled.size())
                        ? std::min(metrics.height * static_cast<float>(bundled[fi].size) / static_cast<float>(total_size), remaining_height)
                        : remaining_height;
                    layers.push_back({ bundled[fi].name, bundled[fi].source_file_path, bundled[fi].size, std::max(layer_height, 0.0f) });
                    remaining_height = std::max(0.0f, remaining_height - layer_height);
                }
            }

            module_model.buildings.push_back({
                std::string(module_path),
                bundle_row.name,
                bundle_row.qualified_name,
                "",
                false,
                false,
                true,
                0,
                bundle_row.building_functions,
                function_mass(bundle_row),
                0,
                metrics,
                { 0.0f, 0.0f },
                std::move(layers),
            });

            // Map individual function names to the bundle so dependency routing finds them.
            for (size_t j = start; j < end; ++j)
                module_model.function_bundle_remap[free_functions[j]->qualified_name] = bundle_row.qualified_name;
        }
    }

    // Stack same-footprint structs into grouped buildings (square, up to N per stack).
    if (config.enable_struct_stacking && !config.hide_struct_entities)
    {
        // Collect struct buildings and partition them out of the main list.
        std::vector<SemanticCityBuilding> structs;
        std::vector<SemanticCityBuilding> non_structs;
        for (auto& bldg : module_model.buildings)
        {
            if (bldg.is_struct && !bldg.is_struct_stack)
                structs.push_back(std::move(bldg));
            else
                non_structs.push_back(std::move(bldg));
        }
        module_model.buildings = std::move(non_structs);

        // Group structs by base_size so each stack has uniform footprint.
        std::unordered_map<int, std::vector<size_t>> by_footprint;
        for (size_t i = 0; i < structs.size(); ++i)
            by_footprint[structs[i].base_size].push_back(i);

        const float floor_gap = std::max(config.struct_stack_gap, 0.0f);
        const float brick_gap = std::max(config.struct_brick_gap, 0.0f);
        const int grid_size = std::max(config.struct_brick_grid_size, 1);
        const int bricks_per_floor = brick_slots_per_floor(grid_size);
        const int max_per_stack = std::max(config.struct_stack_max, 1) * bricks_per_floor;

        // Count total stacks across all base_size groups so names are unique.
        int total_stacks = 0;
        for (const auto& [bs, idxs] : by_footprint)
        {
            if (idxs.size() <= 1)
                continue;
            total_stacks += (static_cast<int>(idxs.size()) + max_per_stack - 1) / max_per_stack;
        }

        int stack_index = 0;
        for (auto& [base_size, indices] : by_footprint)
        {
            if (indices.size() <= 1)
            {
                for (size_t idx : indices)
                    module_model.buildings.push_back(std::move(structs[idx]));
                continue;
            }
            const int group_stack_count = (static_cast<int>(indices.size()) + max_per_stack - 1) / max_per_stack;
            for (int si = 0; si < group_stack_count; ++si)
            {
                ++stack_index;
                const size_t start = static_cast<size_t>(si) * max_per_stack;
                const size_t end = std::min(start + static_cast<size_t>(max_per_stack), indices.size());
                const size_t plate_count = end - start;

                const SemanticCityBuilding& first = structs[indices[start]];
                BuildingMetrics stack_metrics = first.metrics;

                // One brick per struct — brick size comes from the struct's
                // metrics which already scale with field count.
                std::vector<SemanticBuildingLayer> layers;
                layers.reserve(plate_count);
                for (size_t pi = start; pi < end; ++pi)
                {
                    const auto& s = structs[indices[pi]];
                    layers.push_back({
                        s.qualified_name,
                        s.source_file_path,
                        s.base_size,
                        s.metrics.height,
                    });
                }

                // Brick layout: N bricks per floor, height = sum of floor maxes + floor gaps.
                const int num_floors = (static_cast<int>(plate_count) + bricks_per_floor - 1) / bricks_per_floor;
                float stack_height = 0.0f;
                for (int fi = 0; fi < num_floors; ++fi)
                {
                    const size_t fs = static_cast<size_t>(fi) * bricks_per_floor;
                    const size_t fe = std::min(fs + static_cast<size_t>(bricks_per_floor), layers.size());
                    float floor_height = 0.0f;
                    for (size_t bi = fs; bi < fe; ++bi)
                        floor_height = std::max(floor_height, layers[bi].height);
                    stack_height += floor_height;
                }
                if (num_floors > 1)
                    stack_height += floor_gap * static_cast<float>(num_floors - 1);
                stack_metrics.height = stack_height;

                const std::string stack_name = total_stacks > 1
                    ? "Structs " + std::to_string(stack_index)
                    : "Structs";

                module_model.buildings.push_back({
                    std::string(module_path),
                    stack_name,
                    stack_name,
                    "",
                    true,
                    true,
                    false,
                    base_size,
                    static_cast<int>(plate_count),
                    0,
                    0,
                    stack_metrics,
                    { 0.0f, 0.0f },
                    std::move(layers),
                });

                // Map individual struct names to the stack for dependency routing.
                for (size_t pi = start; pi < end; ++pi)
                    module_model.struct_stack_remap[structs[indices[pi]].qualified_name] = stack_name;
            }
        }
    }

    std::sort(module_model.buildings.begin(), module_model.buildings.end(), [](const SemanticCityBuilding& a, const SemanticCityBuilding& b) {
        if (a.metrics.height != b.metrics.height)
            return a.metrics.height > b.metrics.height;
        if (a.metrics.footprint != b.metrics.footprint)
            return a.metrics.footprint > b.metrics.footprint;
        return a.qualified_name < b.qualified_name;
    });

    return module_model;
}

SemanticMegacityModel build_semantic_megacity_model(
    const std::vector<SemanticCityModuleInput>& modules, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    SemanticMegacityModel model;
    model.modules.reserve(modules.size());

    for (const auto& module_input : modules)
    {
        SemanticCityModuleModel module_model = build_semantic_city_model(module_input.module_path, module_input.rows, config);
        if (!module_model.empty())
        {
            module_model.quality = module_input.quality;
            module_model.health = module_input.health;
            model.function_bundle_remap.insert(
                module_model.function_bundle_remap.begin(), module_model.function_bundle_remap.end());
            model.struct_stack_remap.insert(
                module_model.struct_stack_remap.begin(), module_model.struct_stack_remap.end());
            model.modules.push_back(std::move(module_model));
        }

        for (const auto& dep : module_input.dependencies)
        {
            model.dependencies.push_back({
                dep.source_module_path,
                dep.source_qualified_name,
                dep.field_name,
                dep.field_type_name,
                dep.target_module_path,
                dep.target_qualified_name,
                dep.source_file_path,
                dep.target_file_path,
                dep.is_abstract_ref,
            });
        }
    }

    return model;
}

} // namespace draxul

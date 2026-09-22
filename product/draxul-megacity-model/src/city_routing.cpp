#include <draxul/semantic_city_layout.h>

#include "semantic_city_model_internal.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <draxul/perf_timing.h>
#include <functional>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <limits>
#include <optional>
#include <queue>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace draxul
{

namespace
{
constexpr glm::vec4 kOutgoingRouteColor{ 0.20f, 0.88f, 0.30f, 1.0f };
constexpr glm::vec4 kAbstractOutgoingRouteColor{ 0.25f, 0.50f, 0.95f, 1.0f };
constexpr glm::vec4 kIncomingRouteColor{ 0.92f, 0.22f, 0.18f, 1.0f };

using megacity_model_detail::brick_slot_position;
using megacity_model_detail::brick_slots_per_floor;
using megacity_model_detail::building_base_elevation;
using megacity_model_detail::route_base_elevation;

std::string building_key(
    std::string_view source_file_path,
    std::string_view module_path,
    std::string_view qualified_name)
{
    return std::string(source_file_path) + "|" + std::string(module_path) + "|" + std::string(qualified_name);
}

int grid_index(const CityGrid& grid, int col, int row)
{
    return row * grid.cols + col;
}

glm::vec2 grid_cell_center_world(const CityGrid& grid, int col, int row)
{
    return {
        grid.origin_x + (static_cast<float>(col) + 0.5f) * grid.cell_size,
        grid.origin_z + (static_cast<float>(row) + 0.5f) * grid.cell_size,
    };
}

int world_to_grid_col(const CityGrid& grid, float world_x)
{
    return static_cast<int>(std::floor((world_x - grid.origin_x) / grid.cell_size));
}

int world_to_grid_row(const CityGrid& grid, float world_z)
{
    return static_cast<int>(std::floor((world_z - grid.origin_z) / grid.cell_size));
}

struct BuildingRoutePort
{
    glm::vec2 edge_world{ 0.0f };
    glm::vec2 road_entry_world{ 0.0f };
};

enum class PortSide : uint8_t
{
    North,
    South,
    West,
    East,
};

struct RoutePair
{
    const SemanticCityBuilding* source = nullptr;
    const SemanticCityBuilding* target = nullptr;
    std::string source_qualified_name;
    std::string target_qualified_name;
    std::string field_name;
    std::string field_type_name;
    bool is_abstract_ref = false;
};

struct RouteEndpointRequest
{
    size_t route_index = 0;
    bool source_endpoint = false;
    const SemanticCityBuilding* building = nullptr;
    const SemanticCityBuilding* other = nullptr;
    PortSide side = PortSide::North;
    float sort_key = 0.0f;
    std::string field_name;
    std::string individual_qualified_name; // for struct stacks: the specific struct name
    bool is_abstract_ref = false;
};

PortSide side_towards(const SemanticCityBuilding& from, const SemanticCityBuilding& to)
{
    const glm::vec2 delta = to.center - from.center;
    if (std::abs(delta.x) >= std::abs(delta.y))
        return delta.x >= 0.0f ? PortSide::East : PortSide::West;
    return delta.y >= 0.0f ? PortSide::North : PortSide::South;
}

float side_sort_key(PortSide side, const SemanticCityBuilding& other)
{
    switch (side)
    {
    case PortSide::North:
    case PortSide::South:
        return other.center.x;
    case PortSide::West:
    case PortSide::East:
        return other.center.y;
    }
    return 0.0f;
}

std::string route_port_group_key(std::string_view building_key_value, PortSide side)
{
    return std::string(building_key_value) + "#" + std::to_string(static_cast<int>(side));
}

std::optional<BuildingRoutePort> make_building_route_port(
    const SemanticCityBuilding& building, PortSide side, float tangent_offset, const CityGrid& grid)
{
    PERF_MEASURE();
    (void)grid;
    const float half_footprint = building.metrics.footprint * 0.5f;
    const float road_center_offset = half_footprint + building.metrics.sidewalk_width + building.metrics.road_width * 0.5f;
    // Start the edge anchor 1/4 inside the footprint so the rendered line
    // visually intersects non-rectangular (pentagonal) building geometry.
    const float edge_inset = half_footprint * 0.5f;

    glm::vec2 edge_world(0.0f);
    glm::vec2 road_world(0.0f);
    switch (side)
    {
    case PortSide::North:
        edge_world = { building.center.x + tangent_offset, building.center.y + edge_inset };
        road_world = { building.center.x + tangent_offset, building.center.y + road_center_offset };
        break;
    case PortSide::South:
        edge_world = { building.center.x + tangent_offset, building.center.y - edge_inset };
        road_world = { building.center.x + tangent_offset, building.center.y - road_center_offset };
        break;
    case PortSide::West:
        edge_world = { building.center.x - edge_inset, building.center.y + tangent_offset };
        road_world = { building.center.x - road_center_offset, building.center.y + tangent_offset };
        break;
    case PortSide::East:
        edge_world = { building.center.x + edge_inset, building.center.y + tangent_offset };
        road_world = { building.center.x + road_center_offset, building.center.y + tangent_offset };
        break;
    }

    return BuildingRoutePort{
        edge_world,
        road_world,
    };
}

std::vector<RoutePair> collect_route_pairs(
    const SemanticMegacityLayout& layout,
    const SemanticMegacityModel& model,
    std::string_view focus_source_file_path,
    std::string_view focus_module_path,
    std::string_view focus_qualified_name,
    std::string_view focus_function_name)
{
    PERF_MEASURE();
    std::unordered_map<std::string, const SemanticCityBuilding*> buildings_by_key;
    for (const auto& module_layout : layout.modules)
    {
        for (const auto& building : module_layout.buildings)
            buildings_by_key.emplace(
                building_key(building.source_file_path, building.module_path, building.qualified_name),
                &building);
    }

    const std::string focus_key = focus_qualified_name.empty()
        ? std::string()
        : building_key(focus_source_file_path, focus_module_path, focus_qualified_name);

    // For function bundles and struct stacks, look up the building via the remap when the direct key misses.
    auto find_building = [&](const std::string& key, const std::string& qualified_name,
                             const std::string& module_path) -> const SemanticCityBuilding* {
        auto it = buildings_by_key.find(key);
        if (it != buildings_by_key.end())
            return it->second;
        auto remap_it = model.function_bundle_remap.find(qualified_name);
        if (remap_it != model.function_bundle_remap.end())
        {
            auto bundle_it = buildings_by_key.find(building_key("", module_path, remap_it->second));
            if (bundle_it != buildings_by_key.end())
                return bundle_it->second;
        }
        auto struct_remap_it = model.struct_stack_remap.find(qualified_name);
        if (struct_remap_it != model.struct_stack_remap.end())
        {
            auto stack_it = buildings_by_key.find(building_key("", module_path, struct_remap_it->second));
            if (stack_it != buildings_by_key.end())
                return stack_it->second;
        }
        return nullptr;
    };

    // Deduplicate by (source, target) — keep the first non-std::function match per pair.
    // Abstract fan-outs (same source, same field, multiple targets) are NOT deduped.
    std::unordered_map<std::string, size_t> seen_pairs; // edge_key → index in route_pairs
    std::vector<RoutePair> route_pairs;
    route_pairs.reserve(model.dependencies.size());
    for (const auto& dep : model.dependencies)
    {
        // When a specific function/struct in a bundle/stack is focused, only show that entity's deps.
        if (!focus_function_name.empty()
            && dep.source_qualified_name != focus_function_name
            && dep.target_qualified_name != focus_function_name)
            continue;

        const std::string source_key = building_key(
            dep.source_file_path, dep.source_module_path, dep.source_qualified_name);
        const std::string target_key = building_key(
            dep.target_file_path, dep.target_module_path, dep.target_qualified_name);

        const SemanticCityBuilding* source_building = find_building(source_key, dep.source_qualified_name, dep.source_module_path);
        const SemanticCityBuilding* target_building = find_building(target_key, dep.target_qualified_name, dep.target_module_path);

        // When focusing a bundle, match by the bundle building's key.
        if (!focus_key.empty())
        {
            const std::string effective_source_key = source_building
                ? building_key(source_building->source_file_path, source_building->module_path, source_building->qualified_name)
                : source_key;
            const std::string effective_target_key = target_building
                ? building_key(target_building->source_file_path, target_building->module_path, target_building->qualified_name)
                : target_key;
            if (effective_source_key != focus_key && effective_target_key != focus_key)
                continue;
        }

        if (source_building == target_building || !source_building || !target_building)
            continue;

        const std::string edge_key = source_key + "→" + target_key;
        const bool is_function_type = dep.field_type_name.find("function") != std::string::npos;
        auto [it, inserted] = seen_pairs.emplace(edge_key, route_pairs.size());
        if (!inserted)
        {
            // Duplicate (source, target). Replace if the existing one is a function type and this one isn't.
            if (is_function_type)
                continue;
            auto& existing = route_pairs[it->second];
            if (existing.field_type_name.find("function") != std::string::npos)
                existing = { source_building, target_building, dep.source_qualified_name, dep.target_qualified_name,
                    dep.field_name, dep.field_type_name, dep.is_abstract_ref };
            continue;
        }

        route_pairs.push_back({
            source_building,
            target_building,
            dep.source_qualified_name,
            dep.target_qualified_name,
            dep.field_name,
            dep.field_type_name,
            dep.is_abstract_ref,
        });
    }

    return route_pairs;
}

struct AssignedRoutePorts
{
    std::optional<BuildingRoutePort> source_port;
    std::optional<BuildingRoutePort> target_port;
};

glm::vec2 brick_center_offset(
    const SemanticCityBuilding& building, std::string_view individual_name, const SemanticCityLayoutOptions& config)
{
    if (!building.is_struct_stack || individual_name.empty())
        return { 0.0f, 0.0f };
    const int grid_size = std::max(config.struct_brick_grid_size, 1);
    const int bpf = brick_slots_per_floor(grid_size);
    const float footprint = std::max(building.metrics.footprint, 0.1f);
    const float brick_gap = std::max(config.struct_brick_gap, 0.0f);
    const float total_gap = static_cast<float>(grid_size - 1) * brick_gap;
    const float brick_size = std::max((footprint - total_gap) / static_cast<float>(grid_size), 0.01f);
    const float half = footprint * 0.5f;

    for (size_t li = 0; li < building.layers.size(); ++li)
    {
        if (building.layers[li].function_name == individual_name)
        {
            const int local = static_cast<int>(li) % bpf;
            const auto [col, row] = brick_slot_position(local, grid_size);
            const float cx = -half + static_cast<float>(col) * (brick_size + brick_gap) + brick_size * 0.5f;
            const float cz = -half + static_cast<float>(row) * (brick_size + brick_gap) + brick_size * 0.5f;
            return { cx, cz };
        }
    }
    return { 0.0f, 0.0f };
}

std::vector<AssignedRoutePorts> assign_route_ports(
    const std::vector<RoutePair>& route_pairs, const CityGrid& grid, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();

    // Pre-compute a shared source side for abstract fan-out clusters.
    // All routes from the same source building + field that are abstract refs
    // exit from the same side (toward the centroid of their targets).
    struct AbstractClusterKey
    {
        std::string source_key;
        std::string field_name;
        bool operator==(const AbstractClusterKey& other) const
        {
            return source_key == other.source_key && field_name == other.field_name;
        }
    };
    struct AbstractClusterKeyHash
    {
        size_t operator()(const AbstractClusterKey& k) const
        {
            size_t h1 = std::hash<std::string>{}(k.source_key);
            size_t h2 = std::hash<std::string>{}(k.field_name);
            return h1 ^ (h2 * 2654435761u);
        }
    };
    struct ClusterAccum
    {
        const SemanticCityBuilding* source = nullptr;
        glm::vec2 target_centroid_sum{ 0.0f };
        int count = 0;
    };
    std::unordered_map<AbstractClusterKey, ClusterAccum, AbstractClusterKeyHash> abstract_clusters;
    for (const auto& route : route_pairs)
    {
        if (!route.is_abstract_ref || route.source == nullptr || route.target == nullptr)
            continue;
        AbstractClusterKey key{
            building_key(route.source->source_file_path, route.source->module_path, route.source->qualified_name),
            route.field_name
        };
        auto& accum = abstract_clusters[key];
        accum.source = route.source;
        accum.target_centroid_sum += route.target->center;
        ++accum.count;
    }
    // Resolve each cluster with 2+ targets to a shared side.
    std::unordered_map<AbstractClusterKey, PortSide, AbstractClusterKeyHash> abstract_shared_side;
    for (const auto& [key, accum] : abstract_clusters)
    {
        if (accum.count < 2)
            continue;
        const glm::vec2 centroid = accum.target_centroid_sum / static_cast<float>(accum.count);
        SemanticCityBuilding centroid_building;
        centroid_building.center = centroid;
        abstract_shared_side[key] = side_towards(*accum.source, centroid_building);
    }

    std::unordered_map<std::string, std::vector<RouteEndpointRequest>> groups;
    groups.reserve(route_pairs.size() * 2);

    for (size_t route_index = 0; route_index < route_pairs.size(); ++route_index)
    {
        const RoutePair& route = route_pairs[route_index];
        if (route.source == nullptr || route.target == nullptr)
            continue;

        PortSide source_side = side_towards(*route.source, *route.target);
        // Override with shared side for abstract fan-out clusters.
        if (route.is_abstract_ref)
        {
            AbstractClusterKey key{
                building_key(route.source->source_file_path, route.source->module_path, route.source->qualified_name),
                route.field_name
            };
            const auto shared_it = abstract_shared_side.find(key);
            if (shared_it != abstract_shared_side.end())
                source_side = shared_it->second;
        }
        const PortSide target_side = side_towards(*route.target, *route.source);

        groups[route_port_group_key(
                   building_key(route.source->source_file_path, route.source->module_path, route.source->qualified_name),
                   source_side)]
            .push_back({
                route_index,
                true,
                route.source,
                route.target,
                source_side,
                side_sort_key(source_side, *route.target),
                route.field_name,
                route.source_qualified_name,
                route.is_abstract_ref,
            });
        groups[route_port_group_key(
                   building_key(route.target->source_file_path, route.target->module_path, route.target->qualified_name),
                   target_side)]
            .push_back({
                route_index,
                false,
                route.target,
                route.source,
                target_side,
                side_sort_key(target_side, *route.source),
                route.field_name,
                route.target_qualified_name,
                route.is_abstract_ref,
            });
    }

    std::vector<AssignedRoutePorts> assignments(route_pairs.size());
    for (auto& [group_key, requests] : groups)
    {
        (void)group_key;
        if (requests.empty() || requests.front().building == nullptr)
            continue;

        std::sort(requests.begin(), requests.end(), [](const RouteEndpointRequest& lhs, const RouteEndpointRequest& rhs) {
            if (lhs.sort_key != rhs.sort_key)
                return lhs.sort_key < rhs.sort_key;
            return lhs.route_index < rhs.route_index;
        });

        const SemanticCityBuilding& building = *requests.front().building;
        const float half_footprint = building.metrics.footprint * 0.5f;
        const float edge_margin = std::min(
            half_footprint * 0.65f,
            std::max(grid.cell_size * 0.75f, building.metrics.footprint * 0.08f));
        const float usable_half_span = std::max(grid.cell_size * 0.25f, half_footprint - edge_margin);
        const size_t count = requests.size();

        // Abstract source endpoints with the same field_name share a port slot.
        std::vector<size_t> slot_indices(count);
        size_t next_slot = 0;
        std::unordered_map<std::string, size_t> abstract_source_slots;
        for (size_t index = 0; index < count; ++index)
        {
            const auto& req = requests[index];
            if (req.source_endpoint && req.is_abstract_ref)
            {
                auto [it, inserted] = abstract_source_slots.emplace(req.field_name, next_slot);
                if (inserted)
                    ++next_slot;
                slot_indices[index] = it->second;
            }
            else
            {
                slot_indices[index] = next_slot++;
            }
        }
        const size_t total_slots = next_slot;

        for (size_t index = 0; index < count; ++index)
        {
            const float fraction = static_cast<float>(slot_indices[index] + 1) / static_cast<float>(total_slots + 1);
            const float tangent_offset = total_slots == 1
                ? 0.0f
                : glm::mix(-usable_half_span, usable_half_span, fraction);
            std::optional<BuildingRoutePort> port
                = make_building_route_port(building, requests[index].side, tangent_offset, grid);
            if (!port.has_value())
                continue;

            // Offset edge_world to the specific brick for struct stacks.
            if (building.is_struct_stack)
            {
                const glm::vec2 offset = brick_center_offset(building, requests[index].individual_qualified_name, config);
                port->edge_world += offset;
            }

            if (requests[index].source_endpoint)
                assignments[requests[index].route_index].source_port = port;
            else
                assignments[requests[index].route_index].target_port = port;
        }
    }

    return assignments;
}

void append_point_if_new(std::vector<glm::vec2>& points, const glm::vec2& point)
{
    constexpr float kPointEpsilon = 1e-4f;
    if (!points.empty())
    {
        const glm::vec2 delta = points.back() - point;
        if (std::abs(delta.x) <= kPointEpsilon && std::abs(delta.y) <= kPointEpsilon)
            return;
    }
    points.push_back(point);
}

void simplify_polyline(std::vector<glm::vec2>& points)
{
    PERF_MEASURE();
    if (points.size() < 3)
        return;

    std::vector<glm::vec2> simplified;
    simplified.reserve(points.size());
    simplified.push_back(points.front());
    for (size_t i = 1; i + 1 < points.size(); ++i)
    {
        const glm::vec2 a = simplified.back();
        const glm::vec2 b = points[i];
        const glm::vec2 c = points[i + 1];
        const glm::vec2 ab = b - a;
        const glm::vec2 bc = c - b;
        const float cross = ab.x * bc.y - ab.y * bc.x;
        const float dot = ab.x * bc.x + ab.y * bc.y;
        if (std::abs(cross) <= 1e-4f && dot >= 0.0f)
            continue;
        simplified.push_back(b);
    }
    simplified.push_back(points.back());
    points = std::move(simplified);
}

// ---------------------------------------------------------------------------
// Grid-based A* pathfinder — replaces the old O(n^2*m) visibility graph.
// Routes through road and sidewalk cells, avoiding buildings and parks.
// ---------------------------------------------------------------------------

bool is_walkable_cell(uint8_t cell)
{
    return cell == kCityGridRoad || cell == kCityGridSidewalk;
}

glm::ivec2 nearest_walkable_cell(const CityGrid& grid, const glm::vec2& world_pos)
{
    int col = static_cast<int>(std::floor((world_pos.x - grid.origin_x) / grid.cell_size));
    int row = static_cast<int>(std::floor((world_pos.y - grid.origin_z) / grid.cell_size));
    col = std::clamp(col, 0, grid.cols - 1);
    row = std::clamp(row, 0, grid.rows - 1);

    if (is_walkable_cell(grid.at(col, row)))
        return { col, row };

    // Spiral outward to find the nearest walkable cell.
    constexpr int kMaxSearch = 16;
    for (int radius = 1; radius <= kMaxSearch; ++radius)
    {
        for (int dc = -radius; dc <= radius; ++dc)
        {
            for (int dr = -radius; dr <= radius; ++dr)
            {
                if (std::abs(dc) != radius && std::abs(dr) != radius)
                    continue;
                const int c = col + dc;
                const int r = row + dr;
                if (c >= 0 && c < grid.cols && r >= 0 && r < grid.rows
                    && is_walkable_cell(grid.at(c, r)))
                    return { c, r };
            }
        }
    }
    return { -1, -1 };
}

bool find_grid_path(
    const CityGrid& grid,
    const glm::vec2& start_world,
    const glm::vec2& goal_world,
    std::vector<glm::vec2>& path_out)
{
    PERF_MEASURE();
    path_out.clear();
    if (grid.cols <= 0 || grid.rows <= 0 || grid.cells.empty())
        return false;

    const glm::ivec2 sc = nearest_walkable_cell(grid, start_world);
    const glm::ivec2 gc = nearest_walkable_cell(grid, goal_world);
    if (sc.x < 0 || gc.x < 0)
        return false;
    if (sc == gc)
    {
        path_out.push_back(start_world);
        return true;
    }

    const int total = grid.cols * grid.rows;
    const auto idx = [&](int c, int r) { return r * grid.cols + c; };

    std::vector<float> g_score(total, std::numeric_limits<float>::max());
    std::vector<int> predecessor(total, -1);

    const auto heuristic = [&](int c, int r) {
        const float dx = static_cast<float>(std::abs(c - gc.x));
        const float dy = static_cast<float>(std::abs(r - gc.y));
        return std::max(dx, dy) + (1.41421356f - 1.0f) * std::min(dx, dy);
    };

    using QE = std::pair<float, int>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> frontier;

    const int start_idx = idx(sc.x, sc.y);
    const int goal_idx = idx(gc.x, gc.y);
    g_score[start_idx] = 0.0f;
    frontier.push({ heuristic(sc.x, sc.y), start_idx });

    static constexpr int kDx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static constexpr int kDy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    static constexpr float kCost[] = { 1.41421356f, 1.0f, 1.41421356f, 1.0f, 1.0f, 1.41421356f, 1.0f, 1.41421356f };

    while (!frontier.empty())
    {
        const auto [f, ci] = frontier.top();
        frontier.pop();
        if (ci == goal_idx)
            break;

        const int cc = ci % grid.cols;
        const int cr = ci / grid.cols;
        if (f > g_score[ci] + heuristic(cc, cr) + 0.01f)
            continue;

        for (int d = 0; d < 8; ++d)
        {
            const int nc = cc + kDx[d];
            const int nr = cr + kDy[d];
            if (nc < 0 || nc >= grid.cols || nr < 0 || nr >= grid.rows)
                continue;
            if (!is_walkable_cell(grid.at(nc, nr)))
                continue;
            // Diagonal: both cardinal neighbors must be walkable to prevent corner-cutting.
            if (kDx[d] != 0 && kDy[d] != 0)
            {
                if (!is_walkable_cell(grid.at(cc + kDx[d], cr))
                    || !is_walkable_cell(grid.at(cc, cr + kDy[d])))
                    continue;
            }
            const int ni = idx(nc, nr);
            const float tg = g_score[ci] + kCost[d];
            if (tg + 1e-5f >= g_score[ni])
                continue;
            g_score[ni] = tg;
            predecessor[ni] = ci;
            frontier.push({ tg + heuristic(nc, nr), ni });
        }
    }

    if (g_score[goal_idx] == std::numeric_limits<float>::max())
        return false;

    // Reconstruct cell path.
    std::vector<glm::ivec2> cells;
    for (int ci = goal_idx; ci >= 0; ci = (ci == start_idx) ? -1 : predecessor[ci])
    {
        cells.push_back({ ci % grid.cols, ci / grid.cols });
        if (ci == start_idx)
            break;
    }
    std::reverse(cells.begin(), cells.end());

    // Convert to world coordinates (cell centers), but keep the precise
    // start/goal positions so the path doesn't overshoot the port anchor.
    path_out.reserve(cells.size());
    for (size_t i = 0; i < cells.size(); ++i)
    {
        if (i == 0)
            path_out.push_back(start_world);
        else if (i == cells.size() - 1)
            path_out.push_back(goal_world);
        else
            path_out.push_back({
                grid.origin_x + (cells[i].x + 0.5f) * grid.cell_size,
                grid.origin_z + (cells[i].y + 0.5f) * grid.cell_size,
            });
    }
    return true;
}

std::vector<CityGrid::RoutePolyline> build_city_routes_from_grid(
    const SemanticMegacityLayout& layout, const SemanticMegacityModel& model, const CityGrid& grid,
    const SemanticCityLayoutOptions& config,
    std::string_view focus_source_file_path,
    std::string_view focus_module_path,
    std::string_view focus_qualified_name,
    std::string_view focus_function_name = {})
{
    PERF_MEASURE();
    const std::vector<RoutePair> route_pairs = collect_route_pairs(
        layout,
        model,
        focus_source_file_path,
        focus_module_path,
        focus_qualified_name,
        focus_function_name);
    const std::vector<AssignedRoutePorts> assigned_ports = assign_route_ports(route_pairs, grid, config);

    std::vector<std::optional<CityGrid::RoutePolyline>> route_results(route_pairs.size());
    const auto solve_route = [&](size_t route_index) {
        const RoutePair& pair = route_pairs[route_index];
        const AssignedRoutePorts& ports = assigned_ports[route_index];
        if (pair.source == nullptr || pair.target == nullptr
            || !ports.source_port.has_value() || !ports.target_port.has_value()
            || grid.cols <= 0 || grid.rows <= 0)
        {
            return;
        }

        std::vector<glm::vec2> road_path;
        if (!find_grid_path(
                grid,
                ports.source_port->road_entry_world,
                ports.target_port->road_entry_world,
                road_path))
        {
            return;
        }

        std::vector<glm::vec2> world_points;
        append_point_if_new(world_points, ports.source_port->edge_world);
        append_point_if_new(world_points, ports.source_port->road_entry_world);
        for (const glm::vec2& point : road_path)
            append_point_if_new(world_points, point);
        append_point_if_new(world_points, ports.target_port->road_entry_world);
        append_point_if_new(world_points, ports.target_port->edge_world);

        simplify_polyline(world_points);
        route_results[route_index] = CityGrid::RoutePolyline{
            pair.source->source_file_path,
            pair.source->module_path,
            pair.source_qualified_name,
            pair.target->source_file_path,
            pair.target->module_path,
            pair.target_qualified_name,
            pair.field_name,
            pair.field_type_name,
            pair.is_abstract_ref ? kAbstractOutgoingRouteColor : kOutgoingRouteColor,
            kIncomingRouteColor,
            std::move(world_points),
            0.0f, // source_elevation — assigned below
            0.0f, // target_elevation — assigned below
        };
    };

    const unsigned hw_threads = std::thread::hardware_concurrency();
    const size_t worker_count = std::min<size_t>(
        route_pairs.size(),
        std::max<size_t>(1, hw_threads == 0 ? 1 : hw_threads));
    if (worker_count <= 1 || route_pairs.size() < 4)
    {
        for (size_t route_index = 0; route_index < route_pairs.size(); ++route_index)
            solve_route(route_index);
    }
    else
    {
        std::atomic<size_t> next_route_index{ 0 };
        std::vector<std::thread> workers;
        workers.reserve(worker_count);
        for (size_t worker_index = 0; worker_index < worker_count; ++worker_index)
        {
            workers.emplace_back([&next_route_index, &route_pairs, &solve_route]() {
                while (true)
                {
                    const size_t route_index = next_route_index.fetch_add(1);
                    if (route_index >= route_pairs.size())
                        break;
                    solve_route(route_index);
                }
            });
        }
        for (std::thread& worker : workers)
            worker.join();
    }

    struct SuccessfulRoute
    {
        size_t pair_index = 0;
        CityGrid::RoutePolyline route;
    };
    std::vector<SuccessfulRoute> successful_routes;
    successful_routes.reserve(route_pairs.size());
    for (size_t pair_index = 0; pair_index < route_results.size(); ++pair_index)
    {
        auto& route_result = route_results[pair_index];
        if (route_result.has_value())
            successful_routes.push_back({ pair_index, std::move(*route_result) });
    }

    // Assign per-route stacked elevation so pick code can use it per-route.
    // For function bundles with a focused function, use the layer's Y position
    // so the route exits at the correct floor height.
    const float base_elev = route_base_elevation(config);
    const float bldg_base_elev = building_base_elevation(config);
    const float layer_step = std::max(config.dependency_route_layer_step, 0.0f);

    // Resolve the elevation of a specific layer within a building.  Returns -1
    // when the building has no matching layer (i.e. it is not a bundle/stack or
    // the focus name is not among its layers).
    auto resolve_layer_elev = [&](const SemanticCityBuilding* bldg,
                                  std::string_view fn_name) -> float {
        if (fn_name.empty() || bldg == nullptr
            || (!bldg->is_free_function && !bldg->is_struct_stack))
            return -1.0f;
        if (bldg->is_struct_stack)
        {
            const int gs = std::max(config.struct_brick_grid_size, 1);
            const int bpf = brick_slots_per_floor(gs);
            const float fg = std::max(config.struct_stack_gap, 0.0f);
            for (size_t li = 0; li < bldg->layers.size(); ++li)
            {
                if (bldg->layers[li].function_name != fn_name)
                    continue;
                const int floor = static_cast<int>(li) / bpf;
                float cumulative = 0.0f;
                for (int fi = 0; fi < floor; ++fi)
                {
                    const size_t fs = static_cast<size_t>(fi) * bpf;
                    const size_t fe = std::min(fs + static_cast<size_t>(bpf), bldg->layers.size());
                    float fh = 0.0f;
                    for (size_t bi = fs; bi < fe; ++bi)
                        fh = std::max(fh, bldg->layers[bi].height);
                    cumulative += fh + fg;
                }
                const size_t cfs = static_cast<size_t>(floor) * bpf;
                const size_t cfe = std::min(cfs + static_cast<size_t>(bpf), bldg->layers.size());
                float cfh = 0.0f;
                for (size_t bi = cfs; bi < cfe; ++bi)
                    cfh = std::max(cfh, bldg->layers[bi].height);
                return bldg_base_elev + cumulative + cfh * 0.5f;
            }
        }
        else
        {
            float cumulative = 0.0f;
            for (size_t li = 0; li < bldg->layers.size(); ++li)
            {
                const auto& layer = bldg->layers[li];
                cumulative += layer.height;
                if (layer.function_name == fn_name)
                    return bldg_base_elev + cumulative - layer.height * 0.5f;
            }
        }
        return -1.0f;
    };

    std::unordered_map<std::string, int> side_layer_counters;
    for (SuccessfulRoute& successful : successful_routes)
    {
        auto& route = successful.route;
        if (route.world_points.size() < 2)
        {
            route.source_elevation = base_elev;
            route.target_elevation = base_elev;
            continue;
        }
        const glm::vec2 edge = route.world_points.front();
        const glm::vec2 road = route.world_points[1];
        const glm::vec2 dir = road - edge;
        const char side = std::abs(dir.x) > std::abs(dir.y)
            ? (dir.x > 0.0f ? 'E' : 'W')
            : (dir.y > 0.0f ? 'N' : 'S');
        const std::string key = route.source_file_path + "#" + route.source_module_path + "#"
            + route.source_qualified_name + '#' + side;
        const int side_layer = side_layer_counters[key]++;
        const float fallback_elev = base_elev + static_cast<float>(side_layer) * layer_step;

        // Resolve per-end elevations.  For the focused building, use the
        // specific layer height; for the other end use the fallback.
        const auto& rp = route_pairs[successful.pair_index];
        const float src_layer = resolve_layer_elev(rp.source, focus_function_name);
        const float tgt_layer = resolve_layer_elev(rp.target, focus_function_name);

        route.source_elevation = src_layer >= 0.0f ? src_layer : fallback_elev;
        route.target_elevation = tgt_layer >= 0.0f ? tgt_layer : fallback_elev;
    }
    std::vector<CityGrid::RoutePolyline> routes;
    routes.reserve(successful_routes.size());
    for (auto& successful : successful_routes)
        routes.push_back(std::move(successful.route));
    return routes;
}


} // namespace

std::vector<CityGrid::RoutePolyline> build_city_routes(
    const SemanticMegacityLayout& layout, const SemanticMegacityModel& model, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    const CityGrid routing_grid = build_city_grid(layout, config);
    return build_city_routes_from_grid(layout, model, routing_grid, config, {}, {}, {});
}

std::vector<CityGrid::RoutePolyline> build_city_routes_for_selection(
    const SemanticMegacityLayout& layout, const SemanticMegacityModel& model, const CityGrid& grid,
    const SemanticCityLayoutOptions& config,
    std::string_view focus_source_file_path,
    std::string_view focus_module_path,
    std::string_view focus_qualified_name,
    std::string_view focus_function_name)
{
    PERF_MEASURE();
    if (focus_qualified_name.empty())
        return {};
    return build_city_routes_from_grid(
        layout,
        model,
        grid,
        config,
        focus_source_file_path,
        focus_module_path,
        focus_qualified_name,
        focus_function_name);
}

std::vector<CityGrid::RouteRenderSegment> build_city_route_render_segments(
    const std::vector<CityGrid::RoutePolyline>& routes, float lane_spacing)
{
    PERF_MEASURE();
    (void)lane_spacing;

    std::vector<CityGrid::RouteRenderSegment> segments;
    for (const auto& route : routes)
    {
        if (route.world_points.size() < 2)
            continue;

        float total_length = 0.0f;
        for (size_t point_index = 1; point_index < route.world_points.size(); ++point_index)
            total_length += glm::length(route.world_points[point_index] - route.world_points[point_index - 1]);
        if (total_length <= 1e-4f)
            continue;

        float traversed_length = 0.0f;
        for (size_t point_index = 1; point_index < route.world_points.size(); ++point_index)
        {
            const glm::vec2 a = route.world_points[point_index - 1];
            const glm::vec2 b = route.world_points[point_index];
            const float segment_length = glm::length(b - a);
            if (segment_length <= 1e-4f)
                continue;

            const float segment_mid_length = traversed_length + segment_length * 0.5f;
            const float color_t = std::clamp(segment_mid_length / total_length, 0.0f, 1.0f);
            const glm::vec4 segment_color = glm::mix(route.source_color, route.target_color, color_t);
            segments.push_back(
                {
                    a,
                    b,
                    segment_color,
                    route.source_module_path,
                    route.source_qualified_name,
                    route.target_module_path,
                    route.target_qualified_name,
                });
            traversed_length += segment_length;
        }
    }

    return segments;
}

} // namespace draxul

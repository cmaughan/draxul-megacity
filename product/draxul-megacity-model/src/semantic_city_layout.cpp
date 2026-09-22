#include <draxul/semantic_city_layout.h>

#include <algorithm>
#include <cmath>
#include <draxul/perf_timing.h>
#include <glm/geometric.hpp>
#include <limits>
#include <numbers>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace draxul
{
namespace
{

constexpr float kModuleSurfaceBorderWidthScale = 0.5f;
constexpr float kModuleSurfaceBorderWidthMin = 0.2f;

float snap_to_grid(float value, float grid_step)
{
    return std::round(value / grid_step) * grid_step;
}

struct LotRect
{
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
};

bool overlaps(const LotRect& a, const LotRect& b)
{
    return a.min_x < b.max_x && a.max_x > b.min_x && a.min_z < b.max_z && a.max_z > b.min_z;
}

// Uniform spatial grid for fast AABB overlap queries against placed lots.
// Replaces O(n) linear scans in try_place_candidate and touching_lot_candidates.
struct SpatialLotGrid
{
    static constexpr float kCellSize = 10.0f;

    struct CellKey
    {
        int x = 0;
        int z = 0;
        bool operator==(const CellKey&) const = default;
    };

    struct CellKeyHash
    {
        size_t operator()(const CellKey& k) const
        {
            return std::hash<int>()(k.x) ^ (std::hash<int>()(k.z) << 16);
        }
    };

    std::vector<LotRect> lots;
    std::unordered_map<CellKey, std::vector<size_t>, CellKeyHash> cells;

    void clear()
    {
        lots.clear();
        cells.clear();
    }

    void reserve(size_t n)
    {
        lots.reserve(n);
    }

    size_t insert(const LotRect& lot)
    {
        const size_t idx = lots.size();
        lots.push_back(lot);
        const int min_cx = static_cast<int>(std::floor(lot.min_x / kCellSize));
        const int max_cx = static_cast<int>(std::floor(lot.max_x / kCellSize));
        const int min_cz = static_cast<int>(std::floor(lot.min_z / kCellSize));
        const int max_cz = static_cast<int>(std::floor(lot.max_z / kCellSize));
        for (int cx = min_cx; cx <= max_cx; ++cx)
            for (int cz = min_cz; cz <= max_cz; ++cz)
                cells[{ cx, cz }].push_back(idx);
        return idx;
    }

    bool any_overlap(const LotRect& query) const
    {
        const int min_cx = static_cast<int>(std::floor(query.min_x / kCellSize));
        const int max_cx = static_cast<int>(std::floor(query.max_x / kCellSize));
        const int min_cz = static_cast<int>(std::floor(query.min_z / kCellSize));
        const int max_cz = static_cast<int>(std::floor(query.max_z / kCellSize));
        for (int cx = min_cx; cx <= max_cx; ++cx)
            for (int cz = min_cz; cz <= max_cz; ++cz)
            {
                auto it = cells.find({ cx, cz });
                if (it == cells.end())
                    continue;
                for (const size_t idx : it->second)
                    if (overlaps(query, lots[idx]))
                        return true;
            }
        return false;
    }

    // Visit indices of lots whose cells overlap the expanded query AABB.
    // Visitor may see the same index multiple times; caller deduplicates if needed.
    template <typename Fn>
    void for_each_nearby(const LotRect& query, float expansion, Fn&& fn) const
    {
        const int min_cx = static_cast<int>(std::floor((query.min_x - expansion) / kCellSize));
        const int max_cx = static_cast<int>(std::floor((query.max_x + expansion) / kCellSize));
        const int min_cz = static_cast<int>(std::floor((query.min_z - expansion) / kCellSize));
        const int max_cz = static_cast<int>(std::floor((query.max_z + expansion) / kCellSize));
        for (int cx = min_cx; cx <= max_cx; ++cx)
            for (int cz = min_cz; cz <= max_cz; ++cz)
            {
                auto it = cells.find({ cx, cz });
                if (it == cells.end())
                    continue;
                for (const size_t idx : it->second)
                    fn(idx);
            }
    }
};

LotRect centered_building_lot(const BuildingMetrics& metrics, const SemanticCityLayoutOptions& config)
{
    const float step = std::max(config.placement_step, 0.01f);
    const float raw_half_extent = metrics.footprint * 0.5f + metrics.sidewalk_width + metrics.road_width;
    const float half_extent = std::max(step, snap_to_grid(raw_half_extent, step));
    return {
        -half_extent,
        half_extent,
        -half_extent,
        half_extent,
    };
}

LotRect translate_lot(const LotRect& local_lot, const glm::vec2& offset)
{
    return {
        local_lot.min_x + offset.x,
        local_lot.max_x + offset.x,
        local_lot.min_z + offset.y,
        local_lot.max_z + offset.y,
    };
}

float lot_center_x(const LotRect& lot)
{
    return (lot.min_x + lot.max_x) * 0.5f;
}

float lot_center_z(const LotRect& lot)
{
    return (lot.min_z + lot.max_z) * 0.5f;
}

float lot_half_extent_x(const LotRect& lot)
{
    return (lot.max_x - lot.min_x) * 0.5f;
}

float lot_half_extent_z(const LotRect& lot)
{
    return (lot.max_z - lot.min_z) * 0.5f;
}

template <typename Fn>
bool for_each_spiral_candidate(float placement_step, int max_spiral_rings, Fn&& fn, int start_ring = 0)
{
    PERF_MEASURE();
    if (start_ring <= 0)
    {
        if (!fn(glm::vec2(0.0f)))
            return false;
    }

    for (int ring = std::max(1, start_ring); ring < max_spiral_rings; ++ring)
    {
        const float radius = static_cast<float>(ring) * placement_step;
        for (int ix = -ring; ix <= ring; ++ix)
        {
            if (!fn(glm::vec2(static_cast<float>(ix) * placement_step, -radius)))
                return false;
        }
        for (int iz = -ring + 1; iz <= ring; ++iz)
        {
            if (!fn(glm::vec2(radius, static_cast<float>(iz) * placement_step)))
                return false;
        }
        for (int ix = ring - 1; ix >= -ring; --ix)
        {
            if (!fn(glm::vec2(static_cast<float>(ix) * placement_step, radius)))
                return false;
        }
        for (int iz = ring - 1; iz >= -ring + 1; --iz)
        {
            if (!fn(glm::vec2(-radius, static_cast<float>(iz) * placement_step)))
                return false;
        }
    }

    return true;
}

void push_candidate(std::vector<glm::vec2>& candidates, const glm::vec2& candidate)
{
    candidates.push_back(candidate);
}

void add_contact_candidates_for_side(
    std::vector<glm::vec2>& candidates, float fixed_axis, float center_axis, float overlap_limit, bool fixed_x,
    const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    constexpr int kMaxEdgeSamplesPerSide = 24;
    push_candidate(candidates, fixed_x ? glm::vec2(fixed_axis, center_axis) : glm::vec2(center_axis, fixed_axis));

    if (overlap_limit <= 0.0f)
        return;

    const float placement_step = std::max(config.placement_step, 0.01f);
    const int desired_samples = std::max(1, static_cast<int>(std::ceil(overlap_limit / placement_step)));
    const int sample_count = std::min(desired_samples, kMaxEdgeSamplesPerSide);

    for (int sample = 1; sample <= sample_count; ++sample)
    {
        const float t = static_cast<float>(sample) / static_cast<float>(sample_count + 1);
        const float offset = t * overlap_limit;
        push_candidate(candidates,
            fixed_x ? glm::vec2(fixed_axis, center_axis + offset) : glm::vec2(center_axis + offset, fixed_axis));
        push_candidate(candidates,
            fixed_x ? glm::vec2(fixed_axis, center_axis - offset) : glm::vec2(center_axis - offset, fixed_axis));
    }
}

std::vector<glm::vec2> touching_lot_candidates(
    const SpatialLotGrid& grid, const LotRect& local_lot, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    std::vector<glm::vec2> candidates;
    if (grid.lots.empty())
    {
        candidates.push_back(glm::vec2(0.0f));
        return candidates;
    }

    const float local_center_x = lot_center_x(local_lot);
    const float local_center_z = lot_center_z(local_lot);
    const float local_half_extent_x = lot_half_extent_x(local_lot);
    const float local_half_extent_z = lot_half_extent_z(local_lot);

    // Only check recent lots (frontier). Buildings spiral outward, so recently placed
    // lots are on the outer ring. Interior lots produce candidates that always collide.
    constexpr size_t kMaxFrontierLots = 256;
    const size_t frontier_start = grid.lots.size() > kMaxFrontierLots ? grid.lots.size() - kMaxFrontierLots : 0;
    candidates.reserve((grid.lots.size() - frontier_start) * 200);
    for (size_t i = frontier_start; i < grid.lots.size(); ++i)
    {
        const LotRect& occupied = grid.lots[i];
        const float center_x = lot_center_x(occupied);
        const float center_z = lot_center_z(occupied);
        const float overlap_limit_z = lot_half_extent_z(occupied) + local_half_extent_z;
        const float overlap_limit_x = lot_half_extent_x(occupied) + local_half_extent_x;

        add_contact_candidates_for_side(
            candidates, occupied.min_x - local_lot.max_x, center_z - local_center_z, overlap_limit_z, true, config);
        add_contact_candidates_for_side(
            candidates, occupied.max_x - local_lot.min_x, center_z - local_center_z, overlap_limit_z, true, config);
        add_contact_candidates_for_side(
            candidates, occupied.min_z - local_lot.max_z, center_x - local_center_x, overlap_limit_x, false, config);
        add_contact_candidates_for_side(
            candidates, occupied.max_z - local_lot.min_z, center_x - local_center_x, overlap_limit_x, false, config);
    }

    // Cheaply partition the nearest candidates (O(n)) then sort only those (O(k log k)).
    // Far-away candidates almost never succeed — the spiral fallback handles edge cases.
    constexpr size_t kMaxSortedCandidates = 512;
    auto distance_less = [](const glm::vec2& a, const glm::vec2& b) {
        return glm::dot(a, a) < glm::dot(b, b);
    };

    if (candidates.size() > kMaxSortedCandidates)
    {
        std::nth_element(
            candidates.begin(), candidates.begin() + kMaxSortedCandidates, candidates.end(), distance_less);
        candidates.resize(kMaxSortedCandidates);
    }

    std::sort(candidates.begin(), candidates.end(), [](const glm::vec2& a, const glm::vec2& b) {
        const float radius_a = glm::dot(a, a);
        const float radius_b = glm::dot(b, b);
        if (radius_a != radius_b)
            return radius_a < radius_b;

        float angle_a = std::atan2(a.y, a.x);
        float angle_b = std::atan2(b.y, b.x);
        if (angle_a < 0.0f)
            angle_a += 2.0f * std::numbers::pi_v<float>;
        if (angle_b < 0.0f)
            angle_b += 2.0f * std::numbers::pi_v<float>;
        return angle_a < angle_b;
    });

    constexpr float kDuplicateEpsilon = 1e-4f;
    candidates.erase(
        std::unique(candidates.begin(), candidates.end(),
            [](const glm::vec2& a, const glm::vec2& b) {
                return std::abs(a.x - b.x) <= kDuplicateEpsilon && std::abs(a.y - b.y) <= kDuplicateEpsilon;
            }),
        candidates.end());

    return candidates;
}

bool try_place_candidate(
    const SpatialLotGrid& grid,
    const LotRect& local_lot,
    const glm::vec2& offset,
    glm::vec2& chosen_offset,
    LotRect& chosen_lot)
{
    const LotRect lot = translate_lot(local_lot, offset);
    if (grid.any_overlap(lot))
        return false;

    chosen_offset = offset;
    chosen_lot = lot;
    return true;
}

} // namespace

std::array<RoadSegmentPlacement, 4> build_sidewalk_segments(const SemanticCityBuilding& building)
{
    PERF_MEASURE();
    const float half_footprint = building.metrics.footprint * 0.5f;
    const float sidewalk_width = building.metrics.sidewalk_width;
    const float sidewalk_outer_span = building.metrics.footprint + 2.0f * sidewalk_width;
    const glm::vec2 center = building.center;

    return {
        RoadSegmentPlacement{
            { center.x, center.y + half_footprint + sidewalk_width * 0.5f },
            { sidewalk_outer_span, sidewalk_width },
        },
        RoadSegmentPlacement{
            { center.x, center.y - half_footprint - sidewalk_width * 0.5f },
            { sidewalk_outer_span, sidewalk_width },
        },
        RoadSegmentPlacement{
            { center.x - half_footprint - sidewalk_width * 0.5f, center.y },
            { sidewalk_width, building.metrics.footprint },
        },
        RoadSegmentPlacement{
            { center.x + half_footprint + sidewalk_width * 0.5f, center.y },
            { sidewalk_width, building.metrics.footprint },
        },
    };
}

std::array<RoadSegmentPlacement, 4> build_road_segments(const SemanticCityBuilding& building)
{
    PERF_MEASURE();
    const float half_footprint = building.metrics.footprint * 0.5f;
    const float sidewalk_width = building.metrics.sidewalk_width;
    const float road_width = building.metrics.road_width;
    const float inner_span = building.metrics.footprint + 2.0f * sidewalk_width;
    const float outer_span = inner_span + 2.0f * road_width;
    const glm::vec2 center = building.center;

    return {
        RoadSegmentPlacement{
            { center.x, center.y + half_footprint + sidewalk_width + road_width * 0.5f },
            { outer_span, road_width },
        },
        RoadSegmentPlacement{
            { center.x, center.y - half_footprint - sidewalk_width - road_width * 0.5f },
            { outer_span, road_width },
        },
        RoadSegmentPlacement{
            { center.x - half_footprint - sidewalk_width - road_width * 0.5f, center.y },
            { road_width, inner_span },
        },
        RoadSegmentPlacement{
            { center.x + half_footprint + sidewalk_width + road_width * 0.5f, center.y },
            { road_width, inner_span },
        },
    };
}

float compute_module_border_width(const SemanticCityModuleLayout& module_layout, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    const float extent_x = module_layout.max_x - module_layout.min_x;
    const float extent_z = module_layout.max_z - module_layout.min_z;
    if (extent_x <= 1e-4f || extent_z <= 1e-4f)
        return 0.0f;

    return std::min(
        std::min(extent_x, extent_z) * 0.5f,
        std::max(config.placement_step * kModuleSurfaceBorderWidthScale, kModuleSurfaceBorderWidthMin));
}

std::array<ModuleBoundarySignPlacement, 2> build_module_boundary_sign_placements(
    const SemanticCityModuleLayout& module_layout, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    ModuleBoundarySignPlacement south;
    ModuleBoundarySignPlacement north;

    const float extent_x = module_layout.max_x - module_layout.min_x;
    const float border_width = compute_module_border_width(module_layout, config);
    const float usable_width = std::max(0.35f, extent_x - 2.0f * border_width);
    const float sign_width = module_layout.park_footprint > 0.0f
        ? std::max(0.35f, std::min(module_layout.park_footprint, usable_width))
        : usable_width;
    const float sign_depth = config.roof_sign_thickness * 0.5f;
    const float center_x = (module_layout.min_x + module_layout.max_x) * 0.5f;

    south.center = { center_x, module_layout.min_z + sign_depth * 0.5f };
    south.width = sign_width;
    south.depth = sign_depth;
    south.yaw_radians = std::numbers::pi_v<float>;

    north.center = { center_x, module_layout.max_z - sign_depth * 0.5f };
    north.width = sign_width;
    north.depth = sign_depth;
    north.yaw_radians = 0.0f;

    return { south, north };
}

CitySurfaceBounds compute_city_road_surface_bounds(const SemanticMegacityLayout& layout)
{
    PERF_MEASURE();
    CitySurfaceBounds bounds;
    bool have_bounds = false;

    auto expand = [&](float min_x, float max_x, float min_z, float max_z) {
        if (!have_bounds)
        {
            bounds.min_x = min_x;
            bounds.max_x = max_x;
            bounds.min_z = min_z;
            bounds.max_z = max_z;
            have_bounds = true;
            return;
        }
        bounds.min_x = std::min(bounds.min_x, min_x);
        bounds.max_x = std::max(bounds.max_x, max_x);
        bounds.min_z = std::min(bounds.min_z, min_z);
        bounds.max_z = std::max(bounds.max_z, max_z);
    };

    for (const auto& module_layout : layout.modules)
    {
        for (const auto& building : module_layout.buildings)
        {
            const float half_extent
                = building.metrics.footprint * 0.5f + building.metrics.sidewalk_width + building.metrics.road_width;
            expand(building.center.x - half_extent, building.center.x + half_extent,
                building.center.y - half_extent, building.center.y + half_extent);
        }

        if (module_layout.park_footprint > 0.0f)
        {
            const float half_extent = module_layout.park_footprint * 0.5f
                + module_layout.park_sidewalk_width + module_layout.park_road_width;
            expand(module_layout.park_center.x - half_extent, module_layout.park_center.x + half_extent,
                module_layout.park_center.y - half_extent, module_layout.park_center.y + half_extent);
        }
    }

    return bounds;
}

SemanticCityLayout build_semantic_city_layout(
    const SemanticCityModuleModel& module_model, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    SemanticCityLayout layout;
    if (module_model.empty())
        return layout;

    SpatialLotGrid grid;
    grid.reserve(module_model.buildings.size() + 1);
    layout.min_x = std::numeric_limits<float>::max();
    layout.max_x = std::numeric_limits<float>::lowest();
    layout.min_z = std::numeric_limits<float>::max();
    layout.max_z = std::numeric_limits<float>::lowest();

    // Reserve a park lot at the center so buildings spiral outward from it.
    const float step = std::max(config.placement_step, 0.01f);
    const float park_fp = std::max(step, snap_to_grid(config.park_footprint, step));
    if (park_fp > 0.0f)
    {
        const float park_margin = park_fp * 0.5f + config.park_sidewalk_width + config.park_road_width;
        const float park_lot_half = std::max(step, snap_to_grid(park_margin, step));
        layout.park_center = { 0.0f, 0.0f };
        layout.park_footprint = park_fp;
        layout.park_sidewalk_width = config.park_sidewalk_width;
        layout.park_road_width = config.park_road_width;
        grid.insert({ -park_lot_half, park_lot_half, -park_lot_half, park_lot_half });
        layout.min_x = -park_lot_half;
        layout.max_x = park_lot_half;
        layout.min_z = -park_lot_half;
        layout.max_z = park_lot_half;
    }

    for (const SemanticCityBuilding& building : module_model.buildings)
    {
        const LotRect local_lot = centered_building_lot(building.metrics, config);
        glm::vec2 chosen_center{ 0.0f };
        LotRect chosen_lot{};
        bool placed = false;

        const std::vector<glm::vec2> contact_candidates = touching_lot_candidates(grid, local_lot, config);
        for (const glm::vec2& center : contact_candidates)
        {
            if (try_place_candidate(grid, local_lot, center, chosen_center, chosen_lot))
            {
                placed = true;
                break;
            }
        }

        if (!placed)
        {
            const float spiral_step = std::max(config.placement_step, 0.01f);
            // Start the spiral near the city frontier — inner rings are fully occupied.
            const float city_extent = std::max(layout.max_x - layout.min_x, layout.max_z - layout.min_z) * 0.5f;
            const float local_extent = std::max(local_lot.max_x - local_lot.min_x, local_lot.max_z - local_lot.min_z) * 0.5f;
            const int frontier_ring = std::max(0, static_cast<int>((city_extent - local_extent) / spiral_step) - 2);
            for_each_spiral_candidate(spiral_step, config.max_spiral_rings, [&grid, &local_lot, &chosen_center, &chosen_lot, &placed](const glm::vec2& center) {
                if (!try_place_candidate(grid, local_lot, center, chosen_center, chosen_lot))
                    return true;

                placed = true;
                return false; }, frontier_ring);
        }

        if (!placed)
            continue;

        grid.insert(chosen_lot);
        SemanticCityBuilding placed_building = building;
        placed_building.center = chosen_center;
        layout.buildings.push_back(std::move(placed_building));
        layout.min_x = std::min(layout.min_x, chosen_lot.min_x);
        layout.max_x = std::max(layout.max_x, chosen_lot.max_x);
        layout.min_z = std::min(layout.min_z, chosen_lot.min_z);
        layout.max_z = std::max(layout.max_z, chosen_lot.max_z);
    }

    if (layout.buildings.empty())
    {
        layout.min_x = 0.0f;
        layout.max_x = 0.0f;
        layout.min_z = 0.0f;
        layout.max_z = 0.0f;
    }

    return layout;
}

SemanticCityLayout build_semantic_city_layout(
    const std::vector<CityClassRecord>& rows, const SemanticCityLayoutOptions& config)
{
    const SemanticCityModuleModel module_model = build_semantic_city_model(std::string_view{}, rows, config);
    return build_semantic_city_layout(module_model, config);
}

SemanticMegacityLayout build_semantic_megacity_layout(
    const SemanticMegacityModel& model, const SemanticCityLayoutOptions& config)
{
    PERF_MEASURE();
    struct ModuleCandidate
    {
        std::string module_path;
        int connectivity = 0;
        float quality = 0.5f;
        CodebaseHealthMetrics health;
        SemanticCityLayout layout;
        LotRect local_lot;
        float area = 0.0f;
    };

    std::vector<ModuleCandidate> candidates;
    candidates.reserve(model.modules.size());
    for (const auto& module_model : model.modules)
    {
        SemanticCityLayout layout = build_semantic_city_layout(module_model, config);
        if (layout.empty())
            continue;

        const float width = layout.max_x - layout.min_x;
        const float depth = layout.max_z - layout.min_z;
        ModuleCandidate mc;
        mc.module_path = module_model.module_path;
        mc.connectivity = module_model.connectivity;
        mc.quality = module_model.quality;
        mc.health = module_model.health;
        mc.layout = std::move(layout);
        mc.area = width * depth;
        candidates.push_back(std::move(mc));
        candidates.back().local_lot = {
            candidates.back().layout.min_x,
            candidates.back().layout.max_x,
            candidates.back().layout.min_z,
            candidates.back().layout.max_z,
        };
    }

    // Most connected module first (hub of the codebase at the center),
    // then by area, then by name.
    std::sort(candidates.begin(), candidates.end(), [](const ModuleCandidate& a, const ModuleCandidate& b) {
        if (a.connectivity != b.connectivity)
            return a.connectivity > b.connectivity;
        if (a.area != b.area)
            return a.area > b.area;
        return a.module_path < b.module_path;
    });

    SemanticMegacityLayout megacity;
    if (candidates.empty())
        return megacity;

    SpatialLotGrid module_grid;
    module_grid.reserve(candidates.size() + 1);
    megacity.min_x = std::numeric_limits<float>::max();
    megacity.max_x = std::numeric_limits<float>::lowest();
    megacity.min_z = std::numeric_limits<float>::max();
    megacity.max_z = std::numeric_limits<float>::lowest();

    // Place the central park module at the origin — it represents the whole codebase.
    // Only shown in full city view (multiple modules), not single-module view.
    // Central park is double the size of regular module parks.
    if (candidates.size() > 1)
    {
        const float step = std::max(config.placement_step, 0.01f);
        const float area_scale = std::clamp(config.central_park_scale.x, 1.0f, 3.0f);
        const float border_scale = std::clamp(config.central_park_scale.y, 1.0f, 3.0f);
        const float park_fp = std::max(step, snap_to_grid(config.park_footprint * area_scale, step));
        const float park_sw = config.park_sidewalk_width * border_scale;
        const float park_rw = config.park_road_width * border_scale;
        const float park_margin = park_fp * 0.5f + park_sw + park_rw;
        const float park_lot_half = std::max(step, snap_to_grid(park_margin, step));

        SemanticCityModuleLayout central;
        central.module_path = "central_park";
        central.is_central_park = true;
        central.offset = { 0.0f, 0.0f };
        central.min_x = -park_lot_half;
        central.max_x = park_lot_half;
        central.min_z = -park_lot_half;
        central.max_z = park_lot_half;
        central.quality = (model.codebase_health.complexity + model.codebase_health.cohesion + model.codebase_health.coupling) / 3.0f;
        central.health = model.codebase_health;
        central.park_center = { 0.0f, 0.0f };
        central.park_footprint = park_fp;
        central.park_sidewalk_width = park_sw;
        central.park_road_width = park_rw;

        module_grid.insert({ -park_lot_half, park_lot_half, -park_lot_half, park_lot_half });
        megacity.modules.push_back(std::move(central));
        megacity.min_x = -park_lot_half;
        megacity.max_x = park_lot_half;
        megacity.min_z = -park_lot_half;
        megacity.max_z = park_lot_half;
    }

    for (const ModuleCandidate& candidate : candidates)
    {
        glm::vec2 chosen_offset{ 0.0f };
        LotRect chosen_lot{};
        bool placed = false;

        const std::vector<glm::vec2> contact_candidates = touching_lot_candidates(module_grid, candidate.local_lot, config);
        for (const glm::vec2& offset : contact_candidates)
        {
            if (try_place_candidate(module_grid, candidate.local_lot, offset, chosen_offset, chosen_lot))
            {
                placed = true;
                break;
            }
        }

        if (!placed)
        {
            const float lot_width = candidate.local_lot.max_x - candidate.local_lot.min_x;
            const float lot_depth = candidate.local_lot.max_z - candidate.local_lot.min_z;
            const float module_step = std::max(std::min(lot_width, lot_depth) * 0.5f, std::max(config.placement_step, 0.01f));
            for_each_spiral_candidate(module_step, config.max_spiral_rings, [&module_grid, &candidate, &chosen_offset, &chosen_lot, &placed](const glm::vec2& offset) {
                if (!try_place_candidate(module_grid, candidate.local_lot, offset, chosen_offset, chosen_lot))
                    return true;

                placed = true;
                return false;
            });
        }

        if (!placed)
            continue;

        SemanticCityModuleLayout module_layout;
        module_layout.module_path = candidate.module_path;
        module_layout.offset = chosen_offset;
        module_layout.min_x = chosen_lot.min_x;
        module_layout.max_x = chosen_lot.max_x;
        module_layout.min_z = chosen_lot.min_z;
        module_layout.max_z = chosen_lot.max_z;
        module_layout.quality = candidate.quality;
        module_layout.health = candidate.health;
        module_layout.park_center = candidate.layout.park_center + chosen_offset;
        module_layout.park_footprint = candidate.layout.park_footprint;
        module_layout.park_sidewalk_width = candidate.layout.park_sidewalk_width;
        module_layout.park_road_width = candidate.layout.park_road_width;
        module_layout.buildings.reserve(candidate.layout.buildings.size());
        for (const SemanticCityBuilding& building : candidate.layout.buildings)
        {
            SemanticCityBuilding translated = building;
            translated.center += chosen_offset;
            module_layout.buildings.push_back(std::move(translated));
        }

        module_grid.insert(chosen_lot);
        megacity.modules.push_back(std::move(module_layout));
        megacity.min_x = std::min(megacity.min_x, chosen_lot.min_x);
        megacity.max_x = std::max(megacity.max_x, chosen_lot.max_x);
        megacity.min_z = std::min(megacity.min_z, chosen_lot.min_z);
        megacity.max_z = std::max(megacity.max_z, chosen_lot.max_z);
    }

    if (megacity.modules.empty())
    {
        megacity.min_x = 0.0f;
        megacity.max_x = 0.0f;
        megacity.min_z = 0.0f;
        megacity.max_z = 0.0f;
    }

    return megacity;
}

SemanticMegacityLayout build_semantic_megacity_layout(
    const std::vector<SemanticCityModuleInput>& modules, const SemanticCityLayoutOptions& config)
{
    const SemanticMegacityModel model = build_semantic_megacity_model(modules, config);
    return build_semantic_megacity_layout(model, config);
}

} // namespace draxul

# MegaCity model and layout boundary

`draxul-megacity-model` owns four CPU-only implementation units behind the
public `semantic_city_layout.h` value/function API:

- `semantic_city_model.cpp` owns semantic model records and projection:
  `CityClassRecord`, dependency records,
  module/model snapshots, metric derivation, bundle and stack identity;
- `semantic_city_layout.cpp` owns spatial layout: building/module placement,
  parks, surface bounds, sidewalks,
  roads, and module signs;
- `city_grid.cpp` owns `CityGrid` rasterization and cell classification;
- `city_routing.cpp` owns route-pair collection, port assignment, A* pathfinding, polyline
  simplification, endpoint elevation, and render-segment conversion.

Lot grids, pathfinding queues, route-port records, brick placement, and
simplification helpers stay private under `draxul-megacity-model/src`.

Scene mesh construction, colors/materials, picking, panels, host lifecycle,
worker cancellation, generation checks, and snapshot publication remain in
`draxul-megacity`. The model library is synchronous and host-thread agnostic;
its caller owns background execution and immutable publication.

Production consumers are the city builder, selection/map/metrics panels,
scene snapshot builder, semantic source controller, and `MegaCityHost`. Direct
tests live in `megacity_scene_layout_tests.cpp`; downstream presentation tests
consume the resulting records. These consumers require the public value types,
layout/grid/route construction functions, and no renderer or host API.

The public model API accepts only `SemanticCityLayoutOptions`: semantic visibility/stacking limits, metric
scales and clamps, placement/grid limits, sidewalk/road/park dimensions,
route-layer spacing, sign thickness, and struct brick geometry currently read
by the four implementation units. Render tuning, AO, camera, UI, assets, and
worker settings do not belong in the model API. `draxul-megacity` owns the
adapter from `MegaCityCodeConfig` and compatibility overloads for product
consumers; the model library has no product-config dependency.

Determinism depends on stable module/source/qualified-name identity, preserving
input dependency order after failed routes are removed, sorted type references,
stable route-port ordering, row-major grid cells, and fixed tie-breaking in the
pathfinder. Route polylines retain their originating dependency pair through
compaction. Geometry produced downstream continues to use 16-bit indices.

`draxul-test-megacity-model` includes the public header and links the model
target directly. It owns deterministic semantic projection, placement, grid,
route-port/pathfinding, duplicate-identity, and segment-conversion coverage;
host/renderer tests remain in `draxul-test-megacity`.

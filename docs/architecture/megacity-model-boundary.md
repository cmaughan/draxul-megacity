# MegaCity model and layout boundary

`semantic_city_layout.h/.cpp` currently contains four CPU-only groups that
belong in one `draxul-megacity-model` library:

- semantic model records and projection: `CityClassRecord`, dependency records,
  module/model snapshots, metric derivation, bundle and stack identity;
- spatial layout: building/module placement, parks, surface bounds, sidewalks,
  roads, and module signs;
- grid construction: `CityGrid` rasterization and cell classification;
- routing: route-pair collection, port assignment, A* pathfinding, polyline
  simplification, endpoint elevation, and render-segment conversion.

Scene mesh construction, colors/materials, picking, panels, host lifecycle,
worker cancellation, generation checks, and snapshot publication remain in
`draxul-megacity`. The model library is synchronous and host-thread agnostic;
its caller owns background execution and immutable publication.

Production consumers are the city builder, selection/map/metrics panels,
scene snapshot builder, semantic source controller, and `MegaCityHost`. Direct
tests live in `megacity_scene_layout_tests.cpp`; downstream presentation tests
consume the resulting records. These consumers require the public value types,
layout/grid/route construction functions, and no renderer or host API.

The narrow layout options are the semantic visibility/stacking limits, metric
scales and clamps, placement/grid limits, sidewalk/road/park dimensions,
route-layer spacing, sign thickness, and struct brick geometry currently read
by `semantic_city_layout.cpp`. Render tuning, AO, camera, UI, assets, and worker
settings do not belong in the model API. Until migration is complete,
`MegaCityCodeConfig` is the adapter source for those values.

Determinism depends on stable module/source/qualified-name identity, preserving
input dependency order after failed routes are removed, sorted type references,
stable route-port ordering, row-major grid cells, and fixed tie-breaking in the
pathfinder. Route polylines retain their originating dependency pair through
compaction. Geometry produced downstream continues to use 16-bit indices.

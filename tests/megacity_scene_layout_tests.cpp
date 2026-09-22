#include "support/megacity_scene_test_support.h"

#ifdef DRAXUL_ENABLE_MEGACITY

TEST_CASE("semantic layout options adapt CPU model settings", "[megacity]")
{
    MegaCityCodeConfig config;
    config.placement_step = 0.75f;
    config.max_spiral_rings = 123;
    config.struct_brick_grid_size = 4;
    config.dependency_route_layer_step = 0.42f;
    config.park_footprint = 9.0f;
    config.ao_radius = 99.0f;

    const SemanticCityLayoutOptions options = semantic_city_layout_options_from_config(config);
    CHECK(options.placement_step == Catch::Approx(0.75f));
    CHECK(options.max_spiral_rings == 123);
    CHECK(options.struct_brick_grid_size == 4);
    CHECK(options.dependency_route_layer_step == Catch::Approx(0.42f));
    CHECK(options.park_footprint == Catch::Approx(9.0f));
}

TEST_CASE("roof sign mesh textures only the top face", "[megacity]")
{
    const MeshData mesh = build_top_label_panel_mesh();

    REQUIRE(mesh.vertices.size() == 24);
    REQUIRE(mesh.indices.size() == 36);

    size_t textured_vertices = 0;
    size_t top_facing_textured_vertices = 0;
    for (const auto& vertex : mesh.vertices)
    {
        if (vertex.tex_blend > 0.5f)
        {
            textured_vertices++;
            if (vertex.normal.y > 0.5f)
                top_facing_textured_vertices++;
        }
    }

    CHECK(textured_vertices == 4);
    CHECK(top_facing_textured_vertices == 4);
}

TEST_CASE("wall sign mesh textures only the front face", "[megacity]")
{
    const MeshData mesh = build_front_label_panel_mesh();

    REQUIRE(mesh.vertices.size() == 24);
    REQUIRE(mesh.indices.size() == 36);

    size_t textured_vertices = 0;
    size_t front_facing_textured_vertices = 0;
    for (const auto& vertex : mesh.vertices)
    {
        if (vertex.tex_blend > 0.5f)
        {
            textured_vertices++;
            if (vertex.normal.z > 0.5f)
                front_facing_textured_vertices++;
        }
    }

    CHECK(textured_vertices == 4);
    CHECK(front_facing_textured_vertices == 4);
}

TEST_CASE("megacity mesh library builds expected primitive counts", "[megacity]")
{
    const MeshData cube = build_unit_cube_mesh();
    const MeshData floor = build_floor_box_mesh();
    const MeshData foliage_stem = build_foliage_stem_mesh();
    const MeshData foliage_card = build_foliage_card_mesh();
    const MeshData filled = build_grid_mesh(2, 2, 1.0f);

    FloorGridSpec grid;
    grid.enabled = true;
    grid.min_x = 0;
    grid.max_x = 2;
    grid.min_z = 0;
    grid.max_z = 2;
    grid.tile_size = 1.0f;
    grid.line_width = 0.04f;

    const MeshData outline = build_outline_grid_mesh(grid);

    CHECK(cube.vertices.size() == 24);
    CHECK(cube.indices.size() == 36);

    CHECK(floor.vertices.size() == 24);
    CHECK(floor.indices.size() == 36);
    CHECK(triangle_up_normal_y(floor, 8) > 0.0f);

    CHECK_FALSE(foliage_stem.vertices.empty());
    CHECK_FALSE(foliage_stem.indices.empty());
    CHECK(foliage_stem.indices.size() % 3 == 0);
    CHECK_FALSE(foliage_card.vertices.empty());
    CHECK_FALSE(foliage_card.indices.empty());
    CHECK(foliage_card.indices.size() % 3 == 0);
    float tree_max_y = 0.0f;
    for (const auto& vertex : foliage_stem.vertices)
        tree_max_y = std::max(tree_max_y, vertex.position.y);
    for (const auto& vertex : foliage_card.vertices)
        tree_max_y = std::max(tree_max_y, vertex.position.y);
    CHECK(tree_max_y >= Catch::Approx(7.0f).margin(0.01f));

    CHECK(filled.vertices.size() == 16);
    CHECK(filled.indices.size() == 24);
    CHECK(triangle_up_normal_y(filled, 0) > 0.0f);

    CHECK(outline.vertices.size() == 24);
    CHECK(outline.indices.size() == 36);
    CHECK(triangle_up_normal_y(outline, 0) > 0.0f);
}

#endif

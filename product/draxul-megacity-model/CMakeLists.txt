cmake_minimum_required(VERSION 3.25)

# One CPU-only boundary for semantic projection, placement, grid construction,
# and dependency routing. Host workers and scene publication stay in
# draxul-megacity.
add_library(draxul-megacity-model STATIC
    src/city_grid.cpp
    src/city_routing.cpp
    src/semantic_city_layout.cpp
    src/semantic_city_model.cpp
)

target_include_directories(draxul-megacity-model
    PUBLIC include
)

target_link_libraries(draxul-megacity-model
    PUBLIC
        draxul-codeviz-scene
        draxul-code-semantics
    PRIVATE
        Draxul::PluginSupport::Performance
)

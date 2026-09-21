cmake_minimum_required(VERSION 3.25)

# One CPU-only boundary for semantic projection, placement, grid construction,
# and dependency routing. Host workers and scene publication stay in
# draxul-megacity.
add_library(draxul-megacity-model STATIC
    ../draxul-megacity/src/semantic_city_layout.cpp
)

target_include_directories(draxul-megacity-model
    PUBLIC include ../draxul-megacity/include
    PRIVATE ../draxul-megacity/src
)

target_link_libraries(draxul-megacity-model
    PUBLIC
        draxul-codeviz-scene
        draxul-code-semantics
        Draxul::PluginSupport::Config
    PRIVATE
        Draxul::PluginSupport::Performance
)

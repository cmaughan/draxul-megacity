set(_megacity_root "${CMAKE_CURRENT_LIST_DIR}/..")
file(GLOB _megacity_test_sources CONFIGURE_DEPENDS
    "${_megacity_root}/tests/*_tests.cpp")
set(_megacity_parser_test_source
    "${_megacity_root}/tests/treesitter_parser_tests.cpp")
set(_megacity_model_test_source
    "${_megacity_root}/tests/megacity_model_tests.cpp")
list(REMOVE_ITEM _megacity_test_sources
    "${_megacity_parser_test_source}"
    "${_megacity_model_test_source}")

draxul_add_test_target(
    draxul-test-megacity-parser megacity 1 ${_megacity_parser_test_source})
target_link_libraries(draxul-test-megacity-parser PRIVATE draxul-treesitter)
target_include_directories(draxul-test-megacity-parser PRIVATE
    "${_megacity_root}/product/draxul-treesitter/src")
target_compile_definitions(draxul-test-megacity-parser PRIVATE DRAXUL_ENABLE_MEGACITY)

draxul_add_test_target(
    draxul-test-megacity-model megacity 1 ${_megacity_model_test_source})
target_link_libraries(draxul-test-megacity-model PRIVATE draxul-megacity-model)
target_compile_definitions(draxul-test-megacity-model PRIVATE DRAXUL_ENABLE_MEGACITY)

draxul_add_test_target(
    draxul-test-megacity megacity 2 ${_megacity_test_sources})
target_link_libraries(draxul-test-megacity PRIVATE
    draxul-config
    draxul-geometry
    draxul-megacity
    draxul-megacity-test-internals
    draxul-codeviz-renderer-test-internals
    draxul-host-api
    draxul-renderer)
target_compile_definitions(draxul-test-megacity PRIVATE DRAXUL_ENABLE_MEGACITY)

add_dependencies(draxul-test-app draxul-megacity-plugin)
target_compile_definitions(draxul-test-app PRIVATE
    DRAXUL_MEGACITY_PLUGIN_PATH="$<TARGET_FILE:draxul-megacity-plugin>")

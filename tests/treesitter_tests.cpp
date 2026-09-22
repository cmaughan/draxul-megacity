#include <catch2/catch_test_macros.hpp>

#ifdef DRAXUL_ENABLE_MEGACITY

#include "support/codebase_snapshot_wait.h"

#include <draxul/treesitter.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace draxul
{

namespace
{

std::vector<std::string> collect_type_names(const ParsedFile& file)
{
    std::vector<std::string> names;
    for (const auto& symbol : file.symbols)
    {
        if (symbol.kind == SymbolKind::Class || symbol.kind == SymbolKind::Struct)
            names.push_back(symbol.name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace

TEST_CASE("tree-sitter scanner restart does not expose stale snapshots", "[treesitter]")
{
    const auto temp_root
        = std::filesystem::temp_directory_path() / "draxul-treesitter-restart";
    const auto first_root = temp_root / "first";
    const auto second_root = temp_root / "second";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(first_root);
    std::filesystem::create_directories(second_root);

    {
        std::ofstream out(first_root / "old.cpp");
        REQUIRE(out.is_open());
        out << "int old_function() { return 1; }\n";
    }
    {
        std::ofstream out(second_root / "new.cpp");
        REQUIRE(out.is_open());
        out << "int new_function() { return 2; }\n";
    }

    CodebaseScanner scanner;
    scanner.start(first_root);
    const auto first_snapshot = wait_for_complete_snapshot(scanner);
    scanner.stop();

    REQUIRE(first_snapshot);
    REQUIRE(first_snapshot->complete);
    REQUIRE(first_snapshot->files.size() == 1);
    REQUIRE(first_snapshot->files[0].path == "old.cpp");

    scanner.start(second_root);
    if (const auto after_restart = scanner.snapshot())
    {
        CHECK(std::none_of(after_restart->files.begin(), after_restart->files.end(), [](const ParsedFile& file) {
            return file.path == "old.cpp";
        }));
    }
    const auto second_snapshot = wait_for_complete_snapshot(scanner);
    scanner.stop();

    REQUIRE(second_snapshot);
    REQUIRE(second_snapshot->complete);
    REQUIRE(second_snapshot->files.size() == 1);
    CHECK(second_snapshot->files[0].path == "new.cpp");

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("tree-sitter scanner reports cheap progress separately from snapshots", "[treesitter]")
{
    const auto temp_root
        = std::filesystem::temp_directory_path() / "draxul-treesitter-progress";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root);

    {
        std::ofstream out(temp_root / "one.cpp");
        REQUIRE(out.is_open());
        out << "int one() { return 1; }\n";
    }
    {
        std::ofstream out(temp_root / "two.h");
        REQUIRE(out.is_open());
        out << "struct Two { int value; };\n";
    }

    CodebaseScanner scanner;
    scanner.start(temp_root);
    const auto snapshot = wait_for_complete_snapshot(scanner);
    scanner.stop();

    REQUIRE(snapshot);
    REQUIRE(snapshot->complete);
    REQUIRE(snapshot->files.size() == 2);

    const CodebaseScanProgress progress = scanner.progress();
    CHECK(progress.source_files_seen == 2);
    CHECK(progress.source_files_parsed == 2);
    CHECK(progress.source_bytes_read > 0);
    CHECK(progress.complete);
    CHECK_FALSE(progress.running);

    std::filesystem::remove_all(temp_root);
}

TEST_CASE("tree-sitter snapshot skips generated and dependency directories by default", "[treesitter]")
{
    const auto temp_root
        = std::filesystem::temp_directory_path() / "draxul-treesitter-skip-vcpkg";
    std::filesystem::remove_all(temp_root);
    std::filesystem::create_directories(temp_root / "src");
    std::filesystem::create_directories(temp_root / "build" / "generated");
    std::filesystem::create_directories(temp_root / "_deps" / "generated");
    std::filesystem::create_directories(temp_root / "CMakeFiles" / "generated");
    std::filesystem::create_directories(temp_root / "vcpkg");
    std::filesystem::create_directories(temp_root / "third_party" / "vcpkg");
    std::filesystem::create_directories(temp_root / ".cache");
    const auto skipped_relative_link_parent = temp_root / ".cache" / "vcpkg" / "buildtrees"
        / "sdl2" / "src" / "sdl.clean" / "android-project-ant";
    std::filesystem::create_directories(skipped_relative_link_parent);
    const auto skipped_file_style_relative_link_parent = temp_root / ".cache" / "vcpkg" / "buildtrees"
        / "sdl2" / "src" / "sdl.clean" / "android-project-ant-file-style";
    std::filesystem::create_directories(skipped_file_style_relative_link_parent);

    {
        std::ofstream out(temp_root / "src" / "kept.h");
        REQUIRE(out.is_open());
        out << "class Kept {};\n";
    }
    {
        std::ofstream out(temp_root / "vcpkg" / "ignored.h");
        REQUIRE(out.is_open());
        out << "class IgnoredTopLevel {};\n";
    }
    {
        std::ofstream out(temp_root / "third_party" / "vcpkg" / "ignored_nested.h");
        REQUIRE(out.is_open());
        out << "class IgnoredNested {};\n";
    }
    {
        std::ofstream out(temp_root / "build" / "generated" / "ignored_build.h");
        REQUIRE(out.is_open());
        out << "class IgnoredBuild {};\n";
    }
    {
        std::ofstream out(temp_root / "_deps" / "generated" / "ignored_deps.h");
        REQUIRE(out.is_open());
        out << "class IgnoredDeps {};\n";
    }
    {
        std::ofstream out(temp_root / "CMakeFiles" / "generated" / "ignored_cmake.h");
        REQUIRE(out.is_open());
        out << "class IgnoredCMakeFiles {};\n";
    }

    std::error_code file_symlink_ec;
    std::filesystem::create_symlink(
        temp_root / "missing_target.h",
        temp_root / "dangling_file.h",
        file_symlink_ec);

    std::error_code dir_symlink_ec;
    std::filesystem::create_directory_symlink(
        temp_root / "missing_dir",
        temp_root / "dangling_dir.h",
        dir_symlink_ec);

    std::error_code skipped_symlink_ec;
    std::filesystem::create_symlink(
        temp_root / ".cache" / "missing_target.h",
        temp_root / ".cache" / "dangling_source.h",
        skipped_symlink_ec);

    std::error_code skipped_relative_dir_symlink_ec;
    std::filesystem::create_directory_symlink(
        std::filesystem::path("..") / "android-project" / "app" / "src" / "main" / "java",
        skipped_relative_link_parent / "src",
        skipped_relative_dir_symlink_ec);

    std::error_code skipped_file_style_relative_symlink_ec;
    std::filesystem::create_symlink(
        std::filesystem::path("..") / "android-project" / "app" / "src" / "main" / "java",
        skipped_file_style_relative_link_parent / "src",
        skipped_file_style_relative_symlink_ec);

    const bool symlink_creation_unavailable = file_symlink_ec && dir_symlink_ec
        && skipped_symlink_ec && skipped_relative_dir_symlink_ec && skipped_file_style_relative_symlink_ec;
    if (symlink_creation_unavailable)
        INFO("Dangling symlink coverage skipped because this platform did not allow symlink creation.");

    CodebaseScanner scanner;
    scanner.start(temp_root);
    const auto snapshot = wait_for_complete_snapshot(scanner);
    scanner.stop();

    REQUIRE(snapshot);
    REQUIRE(snapshot->complete);
    REQUIRE(snapshot->files.size() == 1);
    CHECK(snapshot->files[0].path == "src/kept.h");

    const std::vector<std::string> type_names = collect_type_names(snapshot->files[0]);
    CHECK(type_names == std::vector<std::string>{ "Kept" });

    std::filesystem::remove_all(temp_root);
}

} // namespace draxul

#endif // DRAXUL_ENABLE_MEGACITY

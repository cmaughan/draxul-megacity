#include <catch2/catch_test_macros.hpp>

#ifdef DRAXUL_ENABLE_MEGACITY

#include "source_parser.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace draxul
{
namespace
{

const SymbolRecord* find_symbol(
    const ParsedFile& file, SymbolKind kind, const std::string& name)
{
    const auto found = std::find_if(file.symbols.begin(), file.symbols.end(),
        [&](const SymbolRecord& symbol) {
            return symbol.kind == kind && symbol.name == name;
        });
    return found == file.symbols.end() ? nullptr : &*found;
}

std::vector<std::string> symbol_names(const ParsedFile& file)
{
    std::vector<std::string> names;
    names.reserve(file.symbols.size());
    for (const SymbolRecord& symbol : file.symbols)
        names.push_back(symbol.name);
    return names;
}

} // namespace

TEST_CASE("Tree-sitter parser extracts C++ record families from memory",
    "[treesitter][parser]")
{
    detail::SourceParser parser;
    const std::string source = R"cpp(#include <memory>
class ForwardClass;
struct ForwardStruct;
class IAbstract {
public:
    virtual void draw() = 0;
};
struct Base {};
class Widget final : public IAbstract, public Base {
public:
    void draw() override {}
    void reset();
private:
    IAbstract& owner_;
    int count_;
};
Widget make_widget(IAbstract& owner, Base* base) { return Widget{}; }
void Widget::reset() { count_ = 0; }
)cpp";

    detail::SourceParseResult result = parser.parse(
        source, "src/widget.hpp");
    REQUIRE(result.status == detail::SourceParseStatus::Success);
    CHECK(result.file.path == "src/widget.hpp");
    CHECK(result.file.errors.empty());
    CHECK(symbol_names(result.file)
        == std::vector<std::string>{ "memory", "IAbstract", "Base",
            "Widget", "draw", "make_widget", "reset" });

    const SymbolRecord* include = find_symbol(
        result.file, SymbolKind::Include, "memory");
    REQUIRE(include);
    CHECK(include->line == 1);

    const SymbolRecord* abstract = find_symbol(
        result.file, SymbolKind::Class, "IAbstract");
    REQUIRE(abstract);
    CHECK(abstract->is_abstract);

    const SymbolRecord* widget = find_symbol(
        result.file, SymbolKind::Class, "Widget");
    REQUIRE(widget);
    CHECK_FALSE(widget->is_abstract);
    CHECK(widget->inherited_types
        == std::vector<std::string>{ "Base", "IAbstract" });
    REQUIRE(widget->fields.size() == 2);
    CHECK(widget->fields[0].name == "owner_");
    CHECK(widget->fields[0].type_name == "IAbstract");
    CHECK(widget->fields[0].referenced_types
        == std::vector<std::string>{ "IAbstract" });
    CHECK(widget->fields[1].name == "count_");
    CHECK(widget->fields[1].type_name == "int");
    CHECK(widget->fields[1].referenced_types.empty());

    const SymbolRecord* inline_method = find_symbol(
        result.file, SymbolKind::Function, "draw");
    REQUIRE(inline_method);
    CHECK(inline_method->parent == "Widget");

    const SymbolRecord* out_of_line_method = find_symbol(
        result.file, SymbolKind::Function, "reset");
    REQUIRE(out_of_line_method);
    CHECK(out_of_line_method->parent == "Widget");

    const SymbolRecord* function = find_symbol(
        result.file, SymbolKind::Function, "make_widget");
    REQUIRE(function);
    CHECK(function->parent.empty());
    CHECK(function->field_count == 2);
    CHECK(function->referenced_types
        == std::vector<std::string>{ "Base", "IAbstract" });

    CHECK(find_symbol(result.file, SymbolKind::Class, "ForwardClass")
        == nullptr);
    CHECK(find_symbol(result.file, SymbolKind::Struct, "ForwardStruct")
        == nullptr);
}

TEST_CASE("Tree-sitter parser keeps nested fields on their declaring type",
    "[treesitter][parser]")
{
    detail::SourceParser parser;
    const std::string source = R"cpp(class Foo {};
class Bar {};
class Outer {
public:
    struct Dependencies {
        Foo* foo = nullptr;
        Bar* bar = nullptr;
    };
private:
    Dependencies dependencies_;
    int count_;
};
)cpp";

    const detail::SourceParseResult result = parser.parse(
        source, "nested/records.hpp");
    REQUIRE(result.status == detail::SourceParseStatus::Success);
    const SymbolRecord* outer = find_symbol(
        result.file, SymbolKind::Class, "Outer");
    REQUIRE(outer);
    REQUIRE(outer->fields.size() == 2);
    CHECK(outer->fields[0].name == "dependencies_");
    CHECK(outer->fields[0].referenced_types
        == std::vector<std::string>{ "Dependencies" });
    CHECK(outer->fields[1].name == "count_");

    const SymbolRecord* dependencies = find_symbol(
        result.file, SymbolKind::Struct, "Dependencies");
    REQUIRE(dependencies);
    REQUIRE(dependencies->fields.size() == 2);
    CHECK(dependencies->fields[0].name == "foo");
    CHECK(dependencies->fields[1].name == "bar");
}

TEST_CASE("Tree-sitter parser reports malformed in-memory source",
    "[treesitter][parser]")
{
    detail::SourceParser parser;
    const std::string malformed
        = "class Visible {};\nclass Broken { void parse( { int value = ;\n";

    const detail::SourceParseResult result = parser.parse(
        malformed, "broken/source.cpp");
    REQUIRE(result.status == detail::SourceParseStatus::Success);
    CHECK(result.file.path == "broken/source.cpp");
    CHECK_FALSE(result.file.errors.empty());
    CHECK(find_symbol(result.file, SymbolKind::Class, "Visible") != nullptr);
}

TEST_CASE("Tree-sitter parser reuses resources and tolerates optional query fallback",
    "[treesitter][parser]")
{
    const std::string abstract_source
        = "class Interface { public: virtual void run() = 0; };\n";

    detail::SourceParser parser({ .enable_abstract_query = false });
    const detail::SourceParseResult first = parser.parse(
        abstract_source, "first.hpp");
    const detail::SourceParseResult second = parser.parse(
        "int second() { return 2; }\n", "second.cpp");
    REQUIRE(first.status == detail::SourceParseStatus::Success);
    REQUIRE(second.status == detail::SourceParseStatus::Success);
    const SymbolRecord* interface = find_symbol(
        first.file, SymbolKind::Class, "Interface");
    REQUIRE(interface);
    CHECK_FALSE(interface->is_abstract);
    CHECK(find_symbol(second.file, SymbolKind::Function, "second")
        != nullptr);

    detail::SourceParser errors_only({
        .enable_symbol_query = false,
        .enable_abstract_query = false,
    });
    const detail::SourceParseResult fallback = errors_only.parse(
        "class Broken { int value = ; };\n", "fallback.hpp");
    REQUIRE(fallback.status == detail::SourceParseStatus::Success);
    CHECK(fallback.file.symbols.empty());
    CHECK_FALSE(fallback.file.errors.empty());

    detail::SourceParser unavailable({ .enable_parser = false });
    const detail::SourceParseResult skipped = unavailable.parse(
        "int skipped() { return 0; }\n", "skipped.cpp");
    CHECK(skipped.status == detail::SourceParseStatus::Skipped);
    CHECK(skipped.file.path.empty());
}

} // namespace draxul

#endif // DRAXUL_ENABLE_MEGACITY

#pragma once

#include <draxul/treesitter.h>

#include <memory>
#include <string>

namespace draxul::detail
{

enum class SourceParseStatus
{
    Success,
    Skipped,
};

struct SourceParseResult
{
    SourceParseStatus status = SourceParseStatus::Skipped;
    ParsedFile file;
};

struct SourceParserOptions
{
    bool enable_parser = true;
    bool enable_symbol_query = true;
    bool enable_abstract_query = true;
};

// Synchronous, filesystem-free C++ source extraction. One instance owns and
// reuses its parser and optional queries across every source in a scan.
class SourceParser
{
public:
    explicit SourceParser(SourceParserOptions options = {});
    ~SourceParser();

    SourceParser(const SourceParser&) = delete;
    SourceParser& operator=(const SourceParser&) = delete;

    [[nodiscard]] SourceParseResult parse(
        const std::string& source, std::string normalized_path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace draxul::detail

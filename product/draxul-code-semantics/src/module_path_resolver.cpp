#include <draxul/module_path_resolver.h>

#include <algorithm>

namespace draxul
{

std::string module_path_for_source_file(std::string_view file_path)
{
    std::string normalized(file_path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    if (normalized.empty())
        return {};

    const auto first_slash = normalized.find('/');
    if (first_slash == std::string::npos)
        return normalized.find('.') == std::string::npos ? normalized : ".";

    const std::string_view first(normalized.data(), first_slash);
    if (first == "libs" || first == "modules")
    {
        const auto second_slash = normalized.find('/', first_slash + 1);
        return second_slash == std::string::npos
            ? normalized
            : normalized.substr(0, second_slash);
    }

    return std::string(first);
}

std::string source_folder_path_for_source_file(std::string_view file_path)
{
    std::string normalized(file_path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    while (normalized.starts_with("./"))
        normalized.erase(0, 2);
    while (normalized.size() > 1 && normalized.ends_with('/'))
        normalized.pop_back();

    if (normalized.empty())
        return {};

    const auto last_slash = normalized.rfind('/');
    if (last_slash == std::string::npos)
        return ".";
    if (last_slash == 0)
        return "/";

    // Treat conventional source and public-header trees as implementation
    // details of their owning project. Everything beneath either marker is
    // grouped into the directory immediately above it, so e.g.
    // project/include/project/api.h and project/src/api.cpp share "project".
    size_t component_start = 0;
    while (component_start < last_slash)
    {
        const size_t slash = normalized.find('/', component_start);
        const size_t component_end = std::min(slash, last_slash);
        const std::string_view component(
            normalized.data() + component_start,
            component_end - component_start);
        if (component == "include" || component == "src")
        {
            if (component_start == 0)
                return ".";
            return normalized.substr(0, component_start - 1);
        }
        if (slash == std::string::npos || slash >= last_slash)
            break;
        component_start = slash + 1;
    }

    return normalized.substr(0, last_slash);
}

} // namespace draxul

#pragma once

#include <string>
#include <string_view>

namespace draxul
{

[[nodiscard]] std::string module_path_for_source_file(std::string_view file_path);
[[nodiscard]] std::string source_folder_path_for_source_file(std::string_view file_path);

} // namespace draxul

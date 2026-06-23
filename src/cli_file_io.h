#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace qmap::cli {

std::string read_text_file(const std::filesystem::path& path);
std::vector<std::byte> read_binary_file(const std::filesystem::path& path);
void write_output_file(const std::filesystem::path& path, std::string_view content, bool force);
std::string lowercase_extension(const std::filesystem::path& path);

} // namespace qmap::cli

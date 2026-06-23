#include "cli_file_io.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace qmap::cli {

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("unable to open input file: " + path.string());
    }

    return {
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>(),
    };
}

std::vector<std::byte> read_binary_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("unable to open input file: " + path.string());
    }

    std::vector<std::byte> bytes;
    char ch = 0;
    while (file.get(ch)) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
    }
    return bytes;
}

void write_output_file(const std::filesystem::path& path, std::string_view content, bool force)
{
    if (!force && std::filesystem::exists(path)) {
        throw std::runtime_error("output file already exists: " + path.string());
    }

    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("unable to open output file: " + path.string());
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string lowercase_extension(const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
}

} // namespace qmap::cli

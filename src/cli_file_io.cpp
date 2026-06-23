#include "cli_file_io.h"

#include <algorithm>
#include <cctype>
#include <system_error>
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

Result<std::vector<std::byte>> read_binary_file_result(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return Result<std::vector<std::byte>>::fail({
            "unable to open input file: " + path.string(),
            0,
        });
    }

    std::vector<std::byte> bytes;
    char ch = 0;
    while (file.get(ch)) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
    }
    if (file.bad()) {
        return Result<std::vector<std::byte>>::fail({
            "unable to read input file: " + path.string(),
            bytes.size(),
        });
    }
    return Result<std::vector<std::byte>>::ok(std::move(bytes));
}

std::vector<std::byte> read_binary_file(const std::filesystem::path& path)
{
    auto read = read_binary_file_result(path);
    if (!read) {
        throw std::runtime_error(read.error().message);
    }
    auto bytes = std::move(read.value());
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

Result<void> write_binary_output_file(
    const std::filesystem::path& path,
    const std::vector<std::byte>& content,
    bool force
)
{
    std::error_code error;
    const auto exists = std::filesystem::exists(path, error);
    if (error) {
        return Result<void>::fail({
            "unable to check output file: " + path.string() + ": " + error.message(),
            0,
        });
    }
    if (!force && exists) {
        return Result<void>::fail({"output file already exists: " + path.string(), 0});
    }

    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return Result<void>::fail({
                "unable to create output directory: " + path.parent_path().string()
                    + ": " + error.message(),
                0,
            });
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return Result<void>::fail({"unable to open output file: " + path.string(), 0});
    }

    file.write(
        reinterpret_cast<const char*>(content.data()),
        static_cast<std::streamsize>(content.size())
    );
    if (!file) {
        return Result<void>::fail({"unable to write output file: " + path.string(), 0});
    }
    return Result<void>::ok();
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

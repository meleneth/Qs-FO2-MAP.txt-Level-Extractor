#pragma once

#include "qmap_result.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace qmap {

class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    std::size_t offset() const
    {
        return offset_;
    }

    std::size_t remaining() const
    {
        return bytes_.size() - offset_;
    }

    bool can_read(std::size_t byte_count) const
    {
        return byte_count <= remaining();
    }

    Result<std::span<const std::byte>> read_bytes(std::size_t byte_count)
    {
        if (!can_read(byte_count)) {
            return Result<std::span<const std::byte>>::fail({
                "unexpected end of input",
                offset_,
            });
        }

        const auto start = offset_;
        offset_ += byte_count;
        return Result<std::span<const std::byte>>::ok(bytes_.subspan(start, byte_count));
    }

    Result<std::uint8_t> read_u8()
    {
        auto bytes = read_bytes(1);
        if (!bytes) {
            return Result<std::uint8_t>::fail(bytes.error());
        }

        return Result<std::uint8_t>::ok(to_u8(bytes.value()[0]));
    }

    Result<std::uint16_t> read_u16_be()
    {
        auto bytes = read_bytes(2);
        if (!bytes) {
            return Result<std::uint16_t>::fail(bytes.error());
        }

        const auto view = bytes.value();
        return Result<std::uint16_t>::ok(
            static_cast<std::uint16_t>((to_u8(view[0]) << 8) | to_u8(view[1]))
        );
    }

    Result<std::uint32_t> read_u32_be()
    {
        auto bytes = read_bytes(4);
        if (!bytes) {
            return Result<std::uint32_t>::fail(bytes.error());
        }

        const auto view = bytes.value();
        return Result<std::uint32_t>::ok(
            (static_cast<std::uint32_t>(to_u8(view[0])) << 24)
            | (static_cast<std::uint32_t>(to_u8(view[1])) << 16)
            | (static_cast<std::uint32_t>(to_u8(view[2])) << 8)
            | static_cast<std::uint32_t>(to_u8(view[3]))
        );
    }

    Result<std::int32_t> read_i32_be()
    {
        auto value = read_u32_be();
        if (!value) {
            return Result<std::int32_t>::fail(value.error());
        }

        return Result<std::int32_t>::ok(std::bit_cast<std::int32_t>(value.value()));
    }

private:
    static std::uint8_t to_u8(std::byte byte)
    {
        return static_cast<std::uint8_t>(byte);
    }

    std::span<const std::byte> bytes_;
    std::size_t offset_ = 0;
};

} // namespace qmap

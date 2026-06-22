#include <catch2/catch_test_macros.hpp>

#include "byte_reader.h"
#include "qmap_result.h"
#include "qmap_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace {

std::byte b(unsigned int value)
{
    return static_cast<std::byte>(value);
}

} // namespace

TEST_CASE("Range describes half-open offsets", "[foundation][range]")
{
    const qmap::Range range{4, 3};

    CHECK(range.offset == 4);
    CHECK(range.size == 3);
    CHECK(range.end() == 7);
    CHECK_FALSE(range.empty());
    CHECK_FALSE(range.contains(3));
    CHECK(range.contains(4));
    CHECK(range.contains(6));
    CHECK_FALSE(range.contains(7));
}

TEST_CASE("Range containment does not overflow wrapped ends", "[foundation][range]")
{
    const qmap::Range range{std::numeric_limits<std::size_t>::max() - 1, 4};

    CHECK_FALSE(range.contains(0));
    CHECK_FALSE(range.contains(std::numeric_limits<std::size_t>::max() - 2));
    CHECK(range.contains(std::numeric_limits<std::size_t>::max() - 1));
    CHECK(range.contains(std::numeric_limits<std::size_t>::max()));
}

TEST_CASE("view_range returns a string_view for valid ranges", "[foundation][range]")
{
    const std::string_view text = "header\nbody\n";

    const auto view = qmap::view_range(text, qmap::Range{7, 4});

    REQUIRE(view.has_value());
    CHECK(*view == "body");
}

TEST_CASE("view_range rejects ranges past the source text", "[foundation][range]")
{
    const std::string_view text = "short";

    CHECK_FALSE(qmap::view_range(text, qmap::Range{3, 99}).has_value());
}

TEST_CASE("Result carries either a value or an error", "[foundation][result]")
{
    auto success = qmap::Result<int>::ok(42);
    auto failure = qmap::Result<int>::fail({"nope", 12});

    REQUIRE(success);
    CHECK(success.value() == 42);

    REQUIRE_FALSE(failure);
    CHECK(failure.error().message == "nope");
    CHECK(failure.error().offset == 12);
}

TEST_CASE("ByteReader reads big-endian values and advances the cursor", "[foundation][byte-reader]")
{
    const std::array bytes{
        b(0x12), b(0x34),
        b(0xFF), b(0xFF), b(0xFF), b(0xFE),
        b(0x80), b(0x00), b(0x00), b(0x00),
    };
    qmap::ByteReader reader(bytes);

    auto word = reader.read_u16_be();
    REQUIRE(word);
    CHECK(word.value() == 0x1234);
    CHECK(reader.offset() == 2);

    auto minus_two = reader.read_i32_be();
    REQUIRE(minus_two);
    CHECK(minus_two.value() == -2);
    CHECK(reader.offset() == 6);

    auto high_bit = reader.read_u32_be();
    REQUIRE(high_bit);
    CHECK(high_bit.value() == 0x80000000u);
    CHECK(reader.offset() == bytes.size());
}

TEST_CASE("ByteReader reports offset when reads exceed input", "[foundation][byte-reader]")
{
    const std::array bytes{b(0xAA), b(0xBB), b(0xCC)};
    qmap::ByteReader reader(bytes);

    auto first = reader.read_u8();
    REQUIRE(first);
    CHECK(first.value() == 0xAA);

    auto missing_word = reader.read_u32_be();

    REQUIRE_FALSE(missing_word);
    CHECK(missing_word.error().message == "unexpected end of input");
    CHECK(missing_word.error().offset == 1);
    CHECK(reader.offset() == 1);
}
